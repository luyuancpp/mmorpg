package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"runtime/debug"
	"sort"
	"strings"
	"time"

	"data_service/cmd/debugutil"

	databasepb "proto/common/database"
	rollbackpb "proto/common/rollback"

	"shared/ownerepoch"
	"shared/placement"
	"shared/savebarrier"
	"shared/scenenode"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// bundleOptions 是 -mode player-bundle 的参数。所有连接都只读:Redis 只发 TYPE/PTTL/GET/HGETALL/ZSCORE,
// MySQL 只在可重复读事务里发固定的 SELECT,结束时回滚。
type bundleOptions struct {
	dbConfigPath     string
	configPath       string
	playerID         uint64
	fallbackZone     uint32
	databaseOverride string
	limit            int
	reason           string
	operator         string
	serverRevision   string
	redisAddr        string
	mappingRedisAddr string
	mysqlAddr        string
	globalMySQLAddr  string
	redisPasswordEnv string
	mysqlPasswordEnv string
	skipGlobal       bool
	skipAccount      bool
	now              func() time.Time
}

func runPlayerBundleMode(ctx context.Context, opts bundleOptions) error {
	if opts.playerID == 0 {
		return errors.New("-player is required for player-bundle mode")
	}
	if strings.TrimSpace(opts.reason) == "" {
		return errors.New("-reason is required for player-bundle mode(写工单号或反馈编号,随导出包留档)")
	}
	cfg, err := debugutil.LoadDBConfig(opts.dbConfigPath)
	if err != nil {
		return err
	}

	core, mapping, err := bundleRedisEndpoints(cfg, opts)
	if err != nil {
		return err
	}
	coreClient, err := debugutil.OpenRedis(ctx, core)
	if err != nil {
		return fmt.Errorf("core redis: %w", err)
	}
	defer coreClient.Close()
	mappingClient := coreClient
	if mapping != core {
		mappingClient, err = debugutil.OpenRedis(ctx, mapping)
		if err != nil {
			return fmt.Errorf("mapping redis: %w", err)
		}
		defer mappingClient.Close()
	}

	bundle := &debugutil.Bundle{
		Format:         debugutil.BundleFormat,
		FormatVersion:  debugutil.BundleFormatVersion,
		PlayerID:       opts.playerID,
		ExportedAtUTC:  opts.now().UTC().Format(time.RFC3339),
		Operator:       opts.operator,
		Reason:         strings.TrimSpace(opts.reason),
		ServerRevision: strings.TrimSpace(opts.serverRevision),
		ToolRevision:   toolRevision(),
		Sources: debugutil.BundleSources{
			CoreRedis: core.Describe(), CoreRedisID: debugutil.RedisServerIdentity(ctx, coreClient),
			MappingRedis: mapping.Describe(), MappingRedisID: debugutil.RedisServerIdentity(ctx, mappingClient),
		},
	}

	// 先读映射两把键定落点:save_pending 索引与合服闸门的键名都要 home_zone。
	routingEntries, err := debugutil.ReadRedisKeys(ctx, mappingClient, []debugutil.RedisKeySpec{
		{Key: placement.HomeZoneKey(opts.playerID), Instance: debugutil.RedisMapping, Kind: debugutil.RedisKindString},
		{Key: placement.Key(opts.playerID), Instance: debugutil.RedisMapping, Kind: debugutil.RedisKindString},
	})
	if err != nil {
		return err
	}
	routing, routingWarnings, err := resolveBundleRouting(routingInput{
		home: &routingEntries[0], placement: &routingEntries[1],
		fallbackFlag: opts.fallbackZone, configZone: cfg.ZoneId, override: opts.databaseOverride,
		required: cfg.Placement.Required, requireReady: cfg.Placement.RequireRouteReady,
	})
	if err != nil {
		return err
	}
	bundle.Routing = routing
	bundle.Analysis.Warnings = append(bundle.Analysis.Warnings, routingWarnings...)

	if bundle.Redis, err = readBundleRedis(ctx, coreClient, mappingClient, opts.playerID, routing.HomeZone, !opts.skipAccount); err != nil {
		return err
	}
	bundle.Analysis.Warnings = append(bundle.Analysis.Warnings, routingDrift(routingEntries, bundle.Redis)...)

	if routing.StorageDB == "" {
		bundle.MySQL.Error = "落点畸形,未读落点库:" + routing.Malformed + "。确认要读的库后用 -database 指定"
	} else {
		zone := cfg.ZoneMySQL(routing.StorageDB)
		if opts.mysqlAddr != "" {
			zone.Hosts = opts.mysqlAddr
		}
		if zone.Password, err = debugutil.PasswordFromEnv(opts.mysqlPasswordEnv, zone.Password); err != nil {
			return err
		}
		bundle.Sources.ZoneMySQL = zone.Describe()
		bundle.MySQL, bundle.Sources.ZoneMySQLID = readBundleMySQL(ctx, zone, opts.playerID)
	}

	if !opts.skipGlobal {
		bundle.Global = readBundleGlobal(ctx, opts, bundle)
	}

	analyzeBundle(bundle)
	return writeJSON(bundle)
}

// bundleRedisEndpoints 套上命令行覆盖。映射实例没有单独配置(Placement.Redis 缺省)时跟随核心实例的覆盖,
// 与 go/db「缺省复用 RedisClient」同义。
func bundleRedisEndpoints(cfg *debugutil.DBConfigFile, opts bundleOptions) (core, mapping debugutil.RedisEndpoint, err error) {
	core = cfg.CoreRedis()
	mappingFollowsCore := cfg.Placement.Redis == nil || strings.TrimSpace(cfg.Placement.Redis.Hosts) == ""
	mapping = cfg.MappingRedis()
	if opts.redisAddr != "" {
		core.Hosts = opts.redisAddr
	}
	if core.Password, err = debugutil.PasswordFromEnv(opts.redisPasswordEnv, core.Password); err != nil {
		return core, mapping, err
	}
	switch {
	case opts.mappingRedisAddr != "":
		mapping.Hosts = opts.mappingRedisAddr
		if mapping.Password, err = debugutil.PasswordFromEnv(opts.redisPasswordEnv, mapping.Password); err != nil {
			return core, mapping, err
		}
	case mappingFollowsCore:
		mapping = core
	default:
		if mapping.Password, err = debugutil.PasswordFromEnv(opts.redisPasswordEnv, mapping.Password); err != nil {
			return core, mapping, err
		}
	}
	if strings.TrimSpace(core.Hosts) == "" {
		return core, mapping, errors.New("db config 没有 ServerConfig.RedisClient.Hosts,用 -redis-addr 指定核心 Redis")
	}
	return core, mapping, nil
}

// routingInput 是选库判定的输入:映射实例上两把键的读取结果 + db.yaml 的回退 zone 与落点开关。
type routingInput struct {
	home, placement          *debugutil.RedisEntry
	fallbackFlag, configZone uint32
	override                 string
	required, requireReady   bool
}

// redisStringValue 按 go/db 读这两把键的口径(MGET)取值:键不存在或不是 string 类型都算缺席(MGET 对非 string 返回 nil);
// 值不是合法文本(二进制)时返回 malformed=true —— go/db 会把它交给解析器并 fail-closed。
func redisStringValue(entry *debugutil.RedisEntry) (raw string, present, malformed bool, note string) {
	if entry == nil || entry.Type == "none" {
		return "", false, false, ""
	}
	if entry.Type != "string" {
		return "", false, false, fmt.Sprintf("%s 的类型是 %s,go/db 按缺席处理", entry.Key, entry.Type)
	}
	if entry.Value == nil || entry.Value.Kind == "binary" {
		return "", true, true, ""
	}
	return entry.Value.Text, true, false, ""
}

// resolveBundleRouting 按 go/db 的选库规则(placement_route.go parsePlacementSnapshot + placement.EffectiveStorage)
// 定落点库:有落点记录用记录,否则用 home_zone,都没有用回退 zone(-zone,缺省 db.yaml ZoneId)。
//
// 映射或落点值畸形时 go/db 对这名玩家读写全部 fail-closed。这正是要导出排查的情形,所以不报错退出:
// 记下 Malformed 与强警告,不自行选库(StorageDB 为空,除非 -database 显式指定)。
// -database 显式覆盖时以它为准但留警告。只有完全无法确定(无映射、无记录、无回退 zone、无 -database)时报错。
func resolveBundleRouting(in routingInput) (debugutil.BundleRouting, []string, error) {
	var routing debugutil.BundleRouting
	var warnings []string
	var malformed []string

	homeRaw, homeKeyPresent, homeBinary, homeNote := redisStringValue(in.home)
	recRaw, recKeyPresent, recBinary, recNote := redisStringValue(in.placement)
	for _, note := range []string{homeNote, recNote} {
		if note != "" {
			warnings = append(warnings, note)
		}
	}

	var rec placement.Record
	recPresent := false
	switch {
	case recBinary:
		malformed = append(malformed, "player:placement 值不是文本")
	case recKeyPresent:
		// 空串也是畸形(placement.Parse 的契约),不能当缺席回落 home_zone。
		routing.PlacementRaw = recRaw
		parsed, err := placement.Parse(recRaw)
		if err != nil {
			malformed = append(malformed, fmt.Sprintf("player:placement=%q: %v", recRaw, err))
			break
		}
		rec, recPresent = parsed, true
		routing.PlacementFrozen = rec.Frozen
		if rec.Frozen {
			warnings = append(warnings, "落点记录处于冻结态(搬库进行中):go/db 读照常按记录选库,写一律延后;源库与目标库的行可能都不是最终态")
		}
	}

	var home uint32
	homePresent := false
	switch {
	case homeBinary:
		malformed = append(malformed, "player:zone 值不是文本")
	case homeKeyPresent:
		parsed, ok, err := placement.ParseHomeZone(homeRaw)
		switch {
		case err != nil:
			malformed = append(malformed, fmt.Sprintf("player:zone=%q: %v", homeRaw, err))
		case ok && !placement.IsZoneStorage(parsed):
			malformed = append(malformed, fmt.Sprintf("player:zone=%d 超出 zone 库编号范围 1..%d", parsed, placement.MaxZoneStorageID))
		default:
			home, homePresent = parsed, ok
		}
	}
	routing.HomeZone, routing.HomeZonePresent = home, homePresent

	if len(malformed) > 0 {
		routing.Malformed = strings.Join(malformed, "; ")
		warnings = append(warnings, "映射 / 落点值畸形("+routing.Malformed+"):线上 go/db 对该玩家读写全部 fail-closed、login 冷加载会失败,根因在映射而不在玩家数据")
	} else {
		if in.required && !recPresent {
			warnings = append(warnings, "db.yaml Placement.Required=true 且该玩家没有 player:placement 记录:线上 go/db 会以 missing_required 拒读(冷加载失败)、写进死信;下面按 home_zone 读出的库与 pin-placement 将钉入的库一致")
		}
		fallback := in.fallbackFlag
		if fallback == 0 {
			fallback = in.configZone
		}
		routing.StorageID = placement.EffectiveStorage(rec, recPresent, home, homePresent, fallback)
		switch {
		case recPresent:
			routing.ResolvedBy = "placement"
		case homePresent:
			routing.ResolvedBy = "home_zone"
		default:
			routing.ResolvedBy = "fallback_zone"
			warnings = append(warnings, fmt.Sprintf("player:zone 映射缺失,落点按回退 zone %d 选库;该玩家可能不存在或映射已丢", fallback))
		}
	}
	if in.requireReady {
		warnings = append(warnings, "db.yaml Placement.RequireRouteReady=true:线上 go/db 只信就绪标记有效的映射副本,本工具没有校验就绪标记")
	}

	dbName, ok := placement.StoreDBName(routing.StorageID)
	if override := strings.TrimSpace(in.override); override != "" {
		if ok && override != dbName {
			warnings = append(warnings, fmt.Sprintf("-database=%s 与按落点解析出的 %s 不同,已按 -database 读取", override, dbName))
		}
		routing.StorageDB = override
		routing.ResolvedBy = "override"
		return routing, warnings, nil
	}
	if routing.Malformed != "" {
		return routing, warnings, nil
	}
	if !ok {
		return routing, nil, errors.New("无法确定落点库:没有映射与落点记录,db config 也没有 ZoneId;用 -zone 或 -database 指定")
	}
	routing.StorageDB = dbName
	return routing, warnings, nil
}

// routingDrift 比较选库时的预读与主读取里的两把映射键:两次读之间映射被改(合服 / 搬库 / 切换)时提示,
// 选库仍按预读值。整份导出包不是跨存储的原子快照,这里只覆盖最容易出错的选库输入。
func routingDrift(preread []debugutil.RedisEntry, entries []debugutil.RedisEntry) []string {
	var warnings []string
	for i := range preread {
		later := debugutil.FindRedisEntry(entries, preread[i].Key)
		if later == nil {
			continue
		}
		before, _, _, _ := redisStringValue(&preread[i])
		after, _, _, _ := redisStringValue(later)
		if preread[i].Type != later.Type || before != after {
			warnings = append(warnings, fmt.Sprintf("导出期间 %s 发生变化(选库时 %q,随后 %q);选库按前者,结果可能对应合服 / 搬库的中间态", preread[i].Key, before, after))
		}
	}
	return warnings
}

// readBundleRedis 读核心与映射两组键。两组在同一实例时合成一次 MULTI,得到同一时刻的视图。
func readBundleRedis(ctx context.Context, coreClient, mappingClient *redis.Client, playerID uint64, homeZone uint32, includeAccount bool) ([]debugutil.RedisEntry, error) {
	specs := debugutil.PlayerRedisKeySpecs(playerID, homeZone)
	var entries []debugutil.RedisEntry
	if coreClient == mappingClient {
		all, err := debugutil.ReadRedisKeys(ctx, coreClient, specs)
		if err != nil {
			return nil, err
		}
		entries = all
	} else {
		var coreSpecs, mappingSpecs []debugutil.RedisKeySpec
		for _, spec := range specs {
			if spec.Instance == debugutil.RedisMapping {
				mappingSpecs = append(mappingSpecs, spec)
			} else {
				coreSpecs = append(coreSpecs, spec)
			}
		}
		coreEntries, err := debugutil.ReadRedisKeys(ctx, coreClient, coreSpecs)
		if err != nil {
			return nil, err
		}
		mappingEntries, err := debugutil.ReadRedisKeys(ctx, mappingClient, mappingSpecs)
		if err != nil {
			return nil, err
		}
		entries = append(coreEntries, mappingEntries...)
	}

	if !includeAccount {
		return entries, nil
	}
	account := debugutil.EntryText(debugutil.FindRedisEntry(entries, debugutil.PlayerToAccountKey(playerID)))
	if account == "" {
		return entries, nil
	}
	// 账号名来自上一次读取,与上面不是同一个事务;角色目录只供查看,不参与导入。
	accountEntries, err := debugutil.ReadRedisKeys(ctx, coreClient, []debugutil.RedisKeySpec{debugutil.AccountRedisKeySpec(account)})
	if err != nil {
		return nil, err
	}
	return append(entries, accountEntries...), nil
}

// readBundleMySQL 在一个可重复读事务里读落点库的玩家表,保证两张表来自同一快照。
// 不用 READ ONLY 事务:TiDB 8.5 默认拒绝 START TRANSACTION READ ONLY(go/mail inbox.go 同一取舍);这里只发 SELECT。
func readBundleMySQL(ctx context.Context, ep debugutil.MySQLEndpoint, playerID uint64) (debugutil.BundleMySQL, string) {
	out := debugutil.BundleMySQL{Database: ep.DBName}
	db, err := debugutil.OpenMySQL(ctx, ep, 1)
	if err != nil {
		out.Error = err.Error()
		return out, ""
	}
	defer db.Close()
	identity := debugutil.MySQLServerIdentity(ctx, db)
	tx, err := db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelRepeatableRead})
	if err != nil {
		out.Error = fmt.Sprintf("begin read snapshot: %v", err)
		return out, identity
	}
	defer tx.Rollback()

	for _, table := range debugutil.PlayerTables {
		out.Tables = append(out.Tables, readBundleTable(ctx, tx, table, playerID))
	}
	return out, identity
}

// liveExtraColumns 返回线上表有、本地 proto 没有的列。SELECT 只列本地 proto 的字段,本地检出比线上旧时
// 多出来的列会被静默丢掉;这里把它们查出来写进导出包,导入端据此拒绝按 SQL 来源导入。
func liveExtraColumns(ctx context.Context, q debugutil.Queryer, table string, msg proto.Message) ([]string, error) {
	rows, err := q.QueryContext(ctx,
		"SELECT column_name FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = ?", table)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	fields := msg.ProtoReflect().Descriptor().Fields()
	var extra []string
	for rows.Next() {
		var column string
		if err := rows.Scan(&column); err != nil {
			return nil, err
		}
		if fields.ByName(protoreflect.Name(strings.ToLower(column))) == nil {
			extra = append(extra, column)
		}
	}
	sort.Strings(extra)
	return extra, rows.Err()
}

func readBundleTable(ctx context.Context, q debugutil.Queryer, table debugutil.PlayerTable, playerID uint64) debugutil.TableRow {
	sample := table.New()
	row := debugutil.TableRow{ProtoType: string(sample.ProtoReflect().Descriptor().FullName())}
	name, err := debugutil.PhysicalTableName(sample)
	if err != nil {
		row.Error = err.Error()
		return row
	}
	row.Table = name
	extra, err := liveExtraColumns(ctx, q, name, sample)
	if err != nil {
		row.Error = fmt.Sprintf("read live columns: %v", err)
		return row
	}
	row.LiveExtraColumns = extra
	msg, found, err := debugutil.ReadPlayerRow(ctx, q, table.New, playerID)
	if err != nil {
		// 表缺列多半是本地检出的 proto 比线上新:切到线上镜像对应的 commit 再导。
		row.Error = err.Error()
		return row
	}
	row.Found = found
	if !found {
		return row
	}
	raw, err := proto.Marshal(msg)
	if err != nil {
		row.Error = fmt.Sprintf("marshal row: %v", err)
		return row
	}
	rendered := debugutil.RenderProto(raw, table.New, nil)
	row.Row = &rendered
	row.FenceVersion = debugutil.RowFenceVersion(msg)
	return row
}

// readBundleGlobal 读全局库里的名字、快照、流水与回档审计。任一段失败只记在 errors 里,不让整份导出失败。
func readBundleGlobal(ctx context.Context, opts bundleOptions, bundle *debugutil.Bundle) *debugutil.BundleGlobal {
	out := &debugutil.BundleGlobal{Errors: map[string]string{}, TruncatedAtRows: opts.limit}
	c, err := debugutil.LoadDataServiceConfig(opts.configPath)
	if err != nil {
		out.Errors["config"] = err.Error()
		return out
	}
	ep := debugutil.GlobalMySQL(c)
	if opts.globalMySQLAddr != "" {
		ep.Hosts = opts.globalMySQLAddr
	}
	if ep.Password, err = debugutil.PasswordFromEnv(opts.mysqlPasswordEnv, ep.Password); err != nil {
		out.Errors["config"] = err.Error()
		return out
	}
	out.Database = ep.DBName
	bundle.Sources.GlobalMySQL = ep.Describe()

	db, err := debugutil.OpenMySQL(ctx, ep, 1)
	if err != nil {
		out.Errors["connect"] = err.Error()
		return out
	}
	defer db.Close()
	bundle.Sources.GlobalMySQLID = debugutil.MySQLServerIdentity(ctx, db)
	tx, err := db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelRepeatableRead})
	if err != nil {
		out.Errors["connect"] = fmt.Sprintf("begin read snapshot: %v", err)
		return out
	}
	defer tx.Rollback()

	id, limit := opts.playerID, opts.limit
	if !opts.skipAccount {
		if rows, err := debugutil.ReadRowsWhere(ctx, tx, func() proto.Message { return &databasepb.AccountRole{} }, "`player_id` = ?", id); err != nil {
			out.Errors["account_role"] = err.Error()
		} else if len(rows) == 1 {
			raw, err := proto.Marshal(rows[0])
			if err != nil {
				out.Errors["account_role"] = err.Error()
			} else {
				value := debugutil.RenderProto(raw, func() proto.Message { return &databasepb.AccountRole{} }, nil)
				out.AccountRole = &value
			}
		}
	}
	if rows, err := debugutil.ReadRowsWhere(ctx, tx, func() proto.Message { return &databasepb.PlayerName{} },
		"`player_id` = ?", id); err != nil {
		out.Errors["player_name"] = err.Error()
	} else if len(rows) > 0 {
		out.PlayerName, _ = debugutil.ProtoJSONOf(rows[0])
	}

	if rows, err := debugutil.ReadRowsWhere(ctx, tx, func() proto.Message { return &databasepb.PlayerSnapshot{} },
		"`player_id` = ? ORDER BY `created_at` DESC, `id` DESC LIMIT ?", id, limit); err != nil {
		out.Errors["player_snapshot"] = err.Error()
	} else {
		for _, row := range rows {
			out.Snapshots = append(out.Snapshots, renderSnapshot(row.(*databasepb.PlayerSnapshot)))
		}
	}

	// 玩家可能在流水的任一侧;两条走各自的 (列, timestamp_sec) 索引,合并后按时间倒序截断。
	var logs []*databasepb.TransactionLog
	for _, column := range []string{"from_player", "to_player"} {
		rows, err := debugutil.ReadRowsWhere(ctx, tx, func() proto.Message { return &databasepb.TransactionLog{} },
			"`"+column+"` = ? ORDER BY `timestamp_sec` DESC LIMIT ?", id, limit)
		if err != nil {
			out.Errors["transaction_log"] = err.Error()
			break
		}
		for _, row := range rows {
			logs = append(logs, row.(*databasepb.TransactionLog))
		}
	}
	for _, entry := range mergeTransactionLogs(logs, limit) {
		value, _ := debugutil.ProtoJSONOf(entry)
		out.TransactionLog = append(out.TransactionLog, value)
	}

	if rows, err := debugutil.ReadRowsWhere(ctx, tx, func() proto.Message { return &databasepb.RollbackAuditLog{} },
		"`player_id` = ? ORDER BY `created_at` DESC, `id` DESC LIMIT ?", id, limit); err != nil {
		out.Errors["rollback_audit_log"] = err.Error()
	} else {
		for _, row := range rows {
			value, _ := debugutil.ProtoJSONOf(row)
			out.RollbackAudit = append(out.RollbackAudit, value)
		}
	}
	if len(out.Errors) == 0 {
		out.Errors = nil
	}
	return out
}

// mergeTransactionLogs 去掉自己转给自己时两侧重复读到的同一行,按时间倒序(同秒按 tx_id)截断到 limit。
func mergeTransactionLogs(logs []*databasepb.TransactionLog, limit int) []*databasepb.TransactionLog {
	seen := make(map[uint64]struct{}, len(logs))
	merged := make([]*databasepb.TransactionLog, 0, len(logs))
	for _, entry := range logs {
		if _, dup := seen[entry.GetTxId()]; dup {
			continue
		}
		seen[entry.GetTxId()] = struct{}{}
		merged = append(merged, entry)
	}
	sort.SliceStable(merged, func(i, j int) bool {
		if merged[i].GetTimestampSec() != merged[j].GetTimestampSec() {
			return merged[i].GetTimestampSec() > merged[j].GetTimestampSec()
		}
		return merged[i].GetTxId() > merged[j].GetTxId()
	})
	if limit > 0 && len(merged) > limit {
		merged = merged[:limit]
	}
	return merged
}

// renderSnapshot 按 source 解 data 列:1 = PlayerSnapshotEntry(再解两张表的 blob),0 = JSON 字段图。
func renderSnapshot(row *databasepb.PlayerSnapshot) debugutil.SnapshotItem {
	data := row.GetData()
	meta := proto.Clone(row).(*databasepb.PlayerSnapshot)
	meta.Data = nil
	item := debugutil.SnapshotItem{}
	item.Meta, _ = debugutil.ProtoJSONOf(meta)
	if len(data) == 0 {
		return item
	}
	if row.GetSource() == 1 {
		newEntry, newDB, newDB1 := debugutil.SnapshotEntryDecoders()
		entryValue := debugutil.RenderProto(data, newEntry, nil)
		item.Entry = &entryValue
		entry := &rollbackpb.PlayerSnapshotEntry{}
		if err := proto.Unmarshal(data, entry); err != nil {
			item.DecodeError = err.Error()
			return item
		}
		if blob := entry.GetPlayerDatabaseBlob(); len(blob) > 0 {
			value := debugutil.RenderProto(blob, newDB, nil)
			item.PlayerDatabase = &value
		}
		if blob := entry.GetPlayerDatabase_1Blob(); len(blob) > 0 {
			value := debugutil.RenderProto(blob, newDB1, nil)
			item.PlayerDatabase1 = &value
		}
		return item
	}
	var fields any
	if err := json.Unmarshal(data, &fields); err != nil {
		item.DecodeError = fmt.Sprintf("source=%d data 不是 JSON: %v", row.GetSource(), err)
		return item
	}
	item.DataJSON = fields
	return item
}

// ── 判读 ───────────────────────────────────────────────────────────────

func analyzeBundle(b *debugutil.Bundle) {
	id := b.PlayerID
	location := debugutil.FindRedisEntry(b.Redis, scenenode.LocationKey(id))
	session := debugutil.FindRedisEntry(b.Redis, debugutil.LocatorSessionKey(id))
	online := location != nil && location.Type != "none"
	switch {
	case online:
		b.Analysis.Online = "场景位置键存在:玩家在线或刚离场。内存态可能比父快照新(周期存盘最多约 300 秒,资产操作即时存盘);要确切的最终态,等玩家下线、屏障对齐后重导"
		b.Analysis.Warnings = append(b.Analysis.Warnings, "导出时玩家可能在线,数据不是最终态")
	case session != nil && session.Type != "none":
		b.Analysis.Online = "没有场景位置,但 player_locator 会话仍在(断线租约内或刚登出)"
	default:
		b.Analysis.Online = "没有场景位置与会话:玩家离线"
	}

	parentEntry := debugutil.FindRedisEntry(b.Redis, debugutil.ParentKey(id))
	parent := decodeParent(parentEntry)
	switch {
	case parent != nil:
		b.Analysis.ParentVsSQL = compareParentWithSQL(parent, b.MySQL)
	case parentEntry != nil && parentEntry.Type == "none":
		b.Analysis.Warnings = append(b.Analysis.Warnings, "Redis 没有父快照(玩家久未登录或缓存被清),以 SQL 行为准")
	default:
		b.Analysis.Warnings = append(b.Analysis.Warnings, "父快照存在但解不开,见 redis 段该键的 decode_error")
	}

	barrierEntry := debugutil.FindRedisEntry(b.Redis, savebarrier.Key(id))
	state := debugutil.BarrierState{
		OwnerEpoch:           debugutil.EntryText(debugutil.FindRedisEntry(b.Redis, ownerepoch.OwnerEpochKey(id))),
		Pending:              debugutil.HashText(barrierEntry, "pending"),
		PlayerDatabaseAck:    debugutil.HashText(barrierEntry, "player_database"),
		PlayerDatabase1Ack:   debugutil.HashText(barrierEntry, "player_database_1"),
		PlayerDatabaseFence:  tableFence(b.MySQL, "player_database"),
		PlayerDatabase1Fence: tableFence(b.MySQL, "player_database_1"),
		ParentPresent:        parent != nil,
		ParentLastSaveSeq:    parent.GetLastSaveSeq(),
		OutboxTerminal:       debugutil.HashText(barrierEntry, "outbox_terminal"),
		OutboxTerminalReason: debugutil.HashText(barrierEntry, "outbox_terminal_reason"),
	}
	if b.Routing.HomeZone != 0 {
		if idx := debugutil.FindRedisEntry(b.Redis, debugutil.SavePendingIndexKey(b.Routing.HomeZone, id)); idx != nil {
			state.SavePendingIndexed = idx.MemberPresent
		}
	}
	state.Conclusion = barrierConclusion(state)
	state.Anomalies = barrierAnomalies(state, b.MySQL.Error == "")
	b.Analysis.Barrier = state

	if b.MySQL.Error != "" {
		b.Analysis.Warnings = append(b.Analysis.Warnings, "落点库读取失败: "+b.MySQL.Error)
	}
	for _, row := range b.MySQL.Tables {
		if len(row.LiveExtraColumns) > 0 {
			b.Analysis.Warnings = append(b.Analysis.Warnings, fmt.Sprintf(
				"线上表 %s 有本地 proto 没有的列 %v,这些列没有导出:本地检出比线上旧,切到线上镜像的 commit 重导", row.Table, row.LiveExtraColumns))
		}
	}
	switch {
	case b.ServerRevision == "":
		b.Analysis.Warnings = append(b.Analysis.Warnings, "未填 -server-revision:解码用的是本地检出代码的 proto,字段有变化时会解出错误但「成功」的 JSON,请确认与线上镜像 commit 一致")
	case b.ToolRevision != "" && !sameRevision(b.ServerRevision, b.ToolRevision):
		b.Analysis.Warnings = append(b.Analysis.Warnings, fmt.Sprintf("工具构建自 %s,线上是 %s:proto 不一致时解码与 SQL 列都可能错位", b.ToolRevision, b.ServerRevision))
	}
}

// sameRevision 比较两个 git 修订号,允许一边是短 sha、忽略 +dirty 后缀。
func sameRevision(a, b string) bool {
	a, b = strings.TrimSuffix(a, "+dirty"), strings.TrimSuffix(b, "+dirty")
	if len(a) > len(b) {
		a, b = b, a
	}
	return a != "" && strings.HasPrefix(b, a)
}

func tableFence(mysql debugutil.BundleMySQL, table string) string {
	for _, row := range mysql.Tables {
		if row.Table == table && row.Found {
			return row.FenceVersion
		}
	}
	return ""
}

// barrierConclusion 按 login 冷加载(parent_commit_barrier.go)的判定顺序给出一句结论。
func barrierConclusion(s debugutil.BarrierState) string {
	switch {
	case s.Pending == "" && s.PlayerDatabaseAck == "" && s.PlayerDatabase1Ack == "":
		if s.OwnerEpoch == "" || s.OwnerEpoch == "0" {
			return "没有存盘屏障与归属代次:新号或从未在严格屏障下存过盘"
		}
		return "有归属代次但没有存盘屏障:历史数据缺提交证据,login 冷加载会拒绝(需停写恢复)"
	case s.Pending == "":
		return "屏障异常:没有 pending 却有 SQL ACK,login 会拒绝(parent pending barrier missing)"
	case s.Pending == s.PlayerDatabaseAck && s.Pending == s.PlayerDatabase1Ack:
		prefix := "最近一次存盘两张表都已落库(pending=两表 ACK)"
		if s.SavePendingIndexed {
			prefix += ",但玩家仍在待落库索引里:补投收尾未清理,观察是否残留"
		}
		if s.ParentPresent {
			return prefix + ";父快照与 SQL 是同一存盘版本"
		}
		return prefix + ";Redis 无父快照,login 将从 SQL 冷加载"
	case s.OutboxTerminal != "" && s.OutboxTerminal == s.Pending:
		return "补投已终止(原因 " + s.OutboxTerminalReason + "):go/db 判定该版本确定性拒写,不会再自动重投;MySQL 停在旧版本,login 会拒绝(parent SQL barrier not committed)"
	default:
		return "存盘在途或落库失败:pending 与 SQL ACK 不等,MySQL 落后于父快照,login 会拒绝冷加载;查 go/db 重试 / 死信队列里该玩家的 DBTask"
	}
}

// barrierAnomalies 列出 login 冷加载会拒绝、但正常写路径产生不了的状态(人工改键、部分恢复、库被回档)。
// sqlRead=false(落点库没读成)时跳过 SQL 围栏比较,免得把「没读到」当成「围栏为空」。
func barrierAnomalies(s debugutil.BarrierState, sqlRead bool) []string {
	if s.Pending == "" {
		return nil
	}
	var out []string
	pendingEpoch, pendingSeq, err := savebarrier.Parse(s.Pending)
	if err != nil {
		return []string{"pending 令牌畸形: " + err.Error()}
	}
	if owner, err := ownerepoch.ParseEpoch(s.OwnerEpoch); err != nil || owner < pendingEpoch {
		out = append(out, fmt.Sprintf("owner_epoch=%q 低于 pending 的代次 %d:login 会拒绝(父快照归属计数器缺失或落后于提交屏障)", s.OwnerEpoch, pendingEpoch))
	}
	if s.ParentPresent && s.ParentLastSaveSeq != pendingSeq {
		out = append(out, fmt.Sprintf("父快照 last_save_seq=%d 与 pending 的序号 %d 不符:login 会拒绝(父快照水位与提交屏障不符)", s.ParentLastSaveSeq, pendingSeq))
	}
	if sqlRead && s.Pending == s.PlayerDatabaseAck && s.Pending == s.PlayerDatabase1Ack {
		for table, fence := range map[string]string{"player_database": s.PlayerDatabaseFence, "player_database_1": s.PlayerDatabase1Fence} {
			if fence != s.Pending {
				out = append(out, fmt.Sprintf("屏障 ACK 已齐但 %s 行围栏=%q 与 pending 不符(库可能被回档 / 恢复):父快照缺失时 login 冷加载会拒绝(SQL 子表围栏尚未对齐父快照提交屏障)", table, fence))
			}
		}
		sort.Strings(out)
	}
	return out
}

func decodeParent(entry *debugutil.RedisEntry) *databasepb.PlayerAllData {
	if entry == nil || entry.Value == nil {
		return nil
	}
	raw, ok, err := entry.Value.RawBytes()
	if err != nil || !ok {
		return nil
	}
	parent := &databasepb.PlayerAllData{}
	if err := proto.Unmarshal(raw, parent); err != nil {
		return nil
	}
	return parent
}

// compareParentWithSQL 逐字段对比父快照里的子消息与 SQL 行,跳过行内围栏两列(父快照里不携带最终围栏)。
func compareParentWithSQL(parent *databasepb.PlayerAllData, mysql debugutil.BundleMySQL) []debugutil.TableDiff {
	pairs := []struct {
		table string
		sub   proto.Message
	}{
		{"player_database", parent.GetPlayerDatabaseData()},
		{"player_database_1", parent.GetPlayerDatabase_1Data()},
	}
	var diffs []debugutil.TableDiff
	for _, pair := range pairs {
		diff := debugutil.TableDiff{Table: pair.table, ParentPresent: pair.sub.ProtoReflect().IsValid()}
		var sqlRow proto.Message
		for _, row := range mysql.Tables {
			if row.Table != pair.table || !row.Found || row.Row == nil {
				continue
			}
			raw, ok, err := row.Row.RawBytes()
			if err != nil || !ok {
				continue
			}
			msg := pair.sub.ProtoReflect().Type().New().Interface()
			if proto.Unmarshal(raw, msg) == nil {
				sqlRow = msg
				diff.SQLPresent = true
				diff.SQLFence = row.FenceVersion
			}
		}
		if diff.ParentPresent && diff.SQLPresent {
			diff.DifferentFields = differentFields(pair.sub, sqlRow, fenceFields)
			diff.Equal = len(diff.DifferentFields) == 0
		}
		diffs = append(diffs, diff)
	}
	return diffs
}

var fenceFields = map[protoreflect.Name]bool{"fence_owner_epoch": true, "fence_save_seq": true}

// differentFields 返回 a、b 中取值不同的顶层字段名(同一消息类型)。
func differentFields(a, b proto.Message, skip map[protoreflect.Name]bool) []string {
	ra, rb := a.ProtoReflect(), b.ProtoReflect()
	fields := ra.Descriptor().Fields()
	var out []string
	for i := 0; i < fields.Len(); i++ {
		fd := fields.Get(i)
		if skip[fd.Name()] {
			continue
		}
		if !fieldEqual(ra, rb, fd) {
			out = append(out, string(fd.Name()))
		}
	}
	return out
}

func fieldEqual(ra, rb protoreflect.Message, fd protoreflect.FieldDescriptor) bool {
	if fd.Kind() == protoreflect.MessageKind && !fd.IsList() && !fd.IsMap() {
		// C++ 存盘对每个组件 mutable_xxx()->CopyFrom,父快照里空组件也带 presence;go/db 经 pbconv 落库时
		// 空子消息写成 "",读回即 Clear。两者在 SQL 往返语义下是同一个值,按「空」归一后再比内容。
		emptyA := !ra.Has(fd) || proto.Size(ra.Get(fd).Message().Interface()) == 0
		emptyB := !rb.Has(fd) || proto.Size(rb.Get(fd).Message().Interface()) == 0
		if emptyA || emptyB {
			return emptyA == emptyB
		}
		return proto.Equal(ra.Get(fd).Message().Interface(), rb.Get(fd).Message().Interface())
	}
	if ra.Has(fd) != rb.Has(fd) {
		return false
	}
	if !ra.Has(fd) {
		return true
	}
	ca := ra.Type().New()
	ca.Set(fd, ra.Get(fd))
	cb := rb.Type().New()
	cb.Set(fd, rb.Get(fd))
	return proto.Equal(ca.Interface(), cb.Interface())
}

// toolRevision 取 go build 嵌入的 vcs 修订号;go run 不嵌入时为空。
func toolRevision() string {
	info, ok := debug.ReadBuildInfo()
	if !ok {
		return ""
	}
	revision, modified := "", false
	for _, setting := range info.Settings {
		switch setting.Key {
		case "vcs.revision":
			revision = setting.Value
		case "vcs.modified":
			modified = setting.Value == "true"
		}
	}
	if revision != "" && modified {
		revision += "+dirty"
	}
	return revision
}

// defaultOperator 是导出人:-operator 优先,否则取操作系统用户名。
func defaultOperator() string {
	for _, name := range []string{"USERNAME", "USER"} {
		if v := strings.TrimSpace(os.Getenv(name)); v != "" {
			return v
		}
	}
	return "unknown"
}
