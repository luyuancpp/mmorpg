package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"sort"
	"strings"

	"data_service/cmd/debugutil"
	"data_service/internal/config"
	"data_service/internal/routing"

	databasepb "proto/common/database"

	"shared/assetop"
	"shared/generated/persistschema"
	"shared/ownerepoch"
	"shared/placement"
	"shared/savebarrier"
	"shared/scenenode"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// ── player-bundle 导入 ─────────────────────────────────────────────────
//
// 目标:把线上一名玩家的数据灌进**本地隔离环境**,让本地 login 走真实的冷加载链路把他加载起来复现问题。
//
// 冷加载(login dataloader parent_commit_barrier.go)对有历史数据的玩家要求三件事同时成立:
//  1. 两张 SQL 行的行内围栏 (fence_owner_epoch, fence_save_seq) 都等于存盘屏障 pending;
//  2. 屏障 hash 的 pending 与两张表的 ACK 相等;
//  3. owner_epoch 不低于 pending 的 epoch。
//
// 所以导入不搬线上的屏障与代次,而是**在本地重铸一个版本** V=(E,S) 同时写进两张行、屏障与 owner_epoch,
// 再删掉本地父快照与子表缓存,让 login 从 SQL 组装父快照。E 取线上与本地观察到的最大代次 + 1:
// 本地残留的旧 DBTask(代次 ≤ 本地旧值)会被行内围栏拒掉,不会覆盖导入的行。
//
// 写入分三段:① Redis 原子复核「不在线、owner 未变」后删父快照与缓存、屏障只写 pending、owner 前进到 E;
// ② 一个事务写两张 SQL 行;③ 复核 pending 未变后补齐两表 ACK 与账号反向索引;最后登记映射。
// ① 之后任何一步中断,屏障都是 pending≠ACK,login 冷加载与父快照快路径都 fail-closed,旧代次的残留存盘被
// owner 挡住;带 -replace 重跑即可收敛。不会出现半套数据被当成有效存档加载,也不会让本地旧父快照借快路径复活。

type bundleImportOptions struct {
	dbConfigPath string
	configPath   string
	localZone    uint32
	source       string
	localAccount string
	replace      bool
	dryRun       bool
	allowHosts   string
}

const (
	sourceAuto        = "auto"
	sourceRedisParent = "redis-parent"
	sourceSQL         = "sql"
)

// importPlan 是不依赖本地状态就能算出的部分(纯函数,单测覆盖)。
type importPlan struct {
	playerID uint64
	source   string
	// rows 按物理表名升序:多表同一事务时的取锁顺序。
	rows []planRow
	// baseEpoch / seq:线上观察到的最大代次与最大存盘序号,本地版本在此之上重铸。
	baseEpoch uint64
	seq       uint64
	notes     []string
}

type planRow struct {
	table string
	msg   proto.Message
}

func isBundleFile(raw []byte) bool {
	var probe struct {
		Format string `json:"format"`
	}
	return json.Unmarshal(raw, &probe) == nil && probe.Format == debugutil.BundleFormat
}

func parseBundle(raw []byte) (*debugutil.Bundle, error) {
	var b debugutil.Bundle
	// player_id 等 uint64 字段直接按整数解析,不经 float64。
	if err := json.Unmarshal(raw, &b); err != nil {
		return nil, fmt.Errorf("parse bundle: %w", err)
	}
	if b.Format != debugutil.BundleFormat {
		return nil, fmt.Errorf("not a %s file", debugutil.BundleFormat)
	}
	if b.FormatVersion != debugutil.BundleFormatVersion {
		return nil, fmt.Errorf("bundle format_version %d 不受支持(本工具只认 %d),请用同一版本的 debug_fetch 重新导出",
			b.FormatVersion, debugutil.BundleFormatVersion)
	}
	if b.PlayerID == 0 {
		return nil, errors.New("bundle 没有 player_id")
	}
	return &b, nil
}

// buildImportPlan 选数据来源并收集线上版本号。来源:
//   - redis-parent:线上 Redis 父快照(write-behind 下比 SQL 新,包含尚未落库的最后状态);
//   - sql:线上落点库的两张行(最后一次两表都确认的版本);
//   - auto:父快照存在且可解就用它,否则用 SQL。
func buildImportPlan(b *debugutil.Bundle, source string) (*importPlan, error) {
	plan := &importPlan{playerID: b.PlayerID}
	parent, parentErr := bundleParent(b)
	sqlRows, sqlErr := bundleSQLRows(b)

	switch source {
	case sourceAuto, "":
		if parent != nil {
			source = sourceRedisParent
			break
		}
		source = sourceSQL
		if parentErr != nil {
			// 父快照存在却不可用时线上 login 会拒绝它;改按 SQL 导入复现不出这一点,必须说出来。
			plan.notes = append(plan.notes, "父快照不可用("+parentErr.Error()+"),已改按 SQL 行导入;线上 login 会拒绝该父快照,本地复现不出这个拒绝")
		}
	case sourceRedisParent, sourceSQL:
	default:
		return nil, fmt.Errorf("-source 只能是 auto | redis-parent | sql,得到 %q", source)
	}
	plan.source = source

	var rows map[string]proto.Message
	switch source {
	case sourceRedisParent:
		if parent == nil {
			if parentErr == nil {
				parentErr = errors.New("导出包里没有父快照")
			}
			return nil, fmt.Errorf("-source=redis-parent 不可用: %w", parentErr)
		}
		rows = map[string]proto.Message{
			"player_database":   proto.Clone(parent.GetPlayerDatabaseData()),
			"player_database_1": proto.Clone(parent.GetPlayerDatabase_1Data()),
		}
		plan.seq = parent.GetLastSaveSeq()
	case sourceSQL:
		if sqlErr != nil {
			if parentErr != nil {
				return nil, fmt.Errorf("父快照与 SQL 行都不可用: 父快照 %v; SQL %w", parentErr, sqlErr)
			}
			return nil, fmt.Errorf("-source=sql 不可用: %w", sqlErr)
		}
		rows = sqlRows
	}

	names := make([]string, 0, len(rows))
	for name := range rows {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		msg := rows[name]
		if id := playerIDOf(msg); id != b.PlayerID {
			return nil, fmt.Errorf("表 %s 的 player_id=%d 与导出包 player_id=%d 不符", name, id, b.PlayerID)
		}
		// 混版本保护(docs/design/mixed-version-persist-field-protection.md §5.4):行的持久 schema 世代高于本工具时,
		// 本工具不认识新世代的列,写库时会把它们丢掉,却原样拷走声称完整的世代戳 —— 导入结果对后来者「说谎」。
		if gen := persistSchemaGenOf(msg); gen > uint64(persistschema.Gen) {
			return nil, fmt.Errorf("表 %s 的持久 schema 世代 %d 高于本工具的 %d;用不低于该世代的 debug_import 重新导入",
				name, gen, persistschema.Gen)
		}
		plan.rows = append(plan.rows, planRow{table: name, msg: msg})
	}

	// 线上见过的所有代次 / 序号:owner_epoch、屏障三令牌、两张 SQL 行的围栏。
	observe := func(epoch, seq uint64) {
		plan.baseEpoch = max(plan.baseEpoch, epoch)
		plan.seq = max(plan.seq, seq)
	}
	if owner := debugutil.EntryText(debugutil.FindRedisEntry(b.Redis, ownerepoch.OwnerEpochKey(b.PlayerID))); owner != "" {
		epoch, err := ownerepoch.ParseEpoch(owner)
		if err != nil {
			return nil, fmt.Errorf("导出包 owner_epoch 畸形: %w", err)
		}
		observe(epoch, 0)
	}
	barrier := debugutil.FindRedisEntry(b.Redis, savebarrier.Key(b.PlayerID))
	for _, field := range []string{"pending", "player_database", "player_database_1"} {
		if token := debugutil.HashText(barrier, field); token != "" {
			epoch, seq, err := savebarrier.Parse(token)
			if err != nil {
				return nil, fmt.Errorf("导出包存盘屏障 %s 畸形: %w", field, err)
			}
			observe(epoch, seq)
		}
	}
	for _, msg := range sqlRows {
		if epoch, seq, ok := debugutil.RowFence(msg); ok {
			observe(epoch, seq)
		}
	}
	if plan.seq == 0 {
		plan.seq = 1
	}

	if source == sourceRedisParent && sqlErr == nil {
		plan.notes = append(plan.notes, "按父快照导入:包含线上尚未落库的最后状态;只想复现 SQL 里的版本用 -source=sql")
	}
	if location := debugutil.FindRedisEntry(b.Redis, scenenode.LocationKey(b.PlayerID)); location != nil && location.Type != "none" {
		plan.notes = append(plan.notes, "导出时玩家在场景里(位置键存在),导入的不一定是最终态")
	}
	return plan, nil
}

func bundleParent(b *debugutil.Bundle) (*databasepb.PlayerAllData, error) {
	entry := debugutil.FindRedisEntry(b.Redis, debugutil.ParentKey(b.PlayerID))
	if entry == nil || entry.Type == "none" || entry.Value == nil {
		return nil, nil
	}
	raw, ok, err := entry.Value.RawBytes()
	if err != nil {
		return nil, err
	}
	if !ok {
		return nil, errors.New("父快照没有原字节")
	}
	parent := &databasepb.PlayerAllData{}
	if err := proto.Unmarshal(raw, parent); err != nil {
		return nil, fmt.Errorf("父快照解析失败: %w", err)
	}
	// 与 login beginParentLoad 同一身份校验:缺子表或身份不符的父快照线上会被拒,不能补骨架后照常导入。
	if parent.GetPlayerDatabaseData() == nil || parent.GetPlayerDatabase_1Data() == nil {
		return nil, errors.New("父快照缺少 player_database_data 或 player_database_1_data(线上 login 会以「父快照玩家身份不符」拒绝)")
	}
	if parent.GetPlayerDatabaseData().GetPlayerId() != b.PlayerID || parent.GetPlayerDatabase_1Data().GetPlayerId() != b.PlayerID {
		return nil, errors.New("父快照里的 player_id 与导出包不符(线上 login 会拒绝)")
	}
	return parent, nil
}

// bundleSQLRows 取导出包里登录加载链路读的两张表;两张都要有,缺一张就不能单独复现 SQL 版本。
func bundleSQLRows(b *debugutil.Bundle) (map[string]proto.Message, error) {
	if b.MySQL.Error != "" {
		return nil, fmt.Errorf("导出时落点库读取失败: %s", b.MySQL.Error)
	}
	rows := map[string]proto.Message{}
	for _, table := range debugutil.PlayerTables {
		if !table.LoadPath {
			continue
		}
		name, err := debugutil.PhysicalTableName(table.New())
		if err != nil {
			return nil, err
		}
		var exported *debugutil.TableRow
		for i := range b.MySQL.Tables {
			if b.MySQL.Tables[i].Table == name {
				exported = &b.MySQL.Tables[i]
			}
		}
		if exported == nil || exported.Error != "" || !exported.Found || exported.Row == nil {
			return nil, fmt.Errorf("导出包缺少表 %s 的行", name)
		}
		if len(exported.LiveExtraColumns) > 0 {
			return nil, fmt.Errorf("表 %s 导出时线上有本地 proto 没有的列 %v,这些列没导出;切到线上镜像的 commit 重导",
				name, exported.LiveExtraColumns)
		}
		raw, ok, err := exported.Row.RawBytes()
		if err != nil {
			return nil, fmt.Errorf("表 %s: %w", name, err)
		}
		if !ok {
			return nil, fmt.Errorf("表 %s 没有原字节", name)
		}
		msg := table.New()
		if err := proto.Unmarshal(raw, msg); err != nil {
			return nil, fmt.Errorf("表 %s 行解析失败: %w", name, err)
		}
		rows[name] = msg
	}
	return rows, nil
}

func playerIDOf(msg proto.Message) uint64 {
	fd := msg.ProtoReflect().Descriptor().Fields().ByName("player_id")
	if fd == nil {
		return 0
	}
	return msg.ProtoReflect().Get(fd).Uint()
}

// persistSchemaGenOf 读玩家表的覆盖世代;没有这个字段的表返回 0(按未盖戳处理,不拦)。
func persistSchemaGenOf(msg proto.Message) uint64 {
	fd := msg.ProtoReflect().Descriptor().Fields().ByName("persist_schema_gen")
	if fd == nil {
		return 0
	}
	return msg.ProtoReflect().Get(fd).Uint()
}


// ── 执行 ───────────────────────────────────────────────────────────────

func runBundleImport(ctx context.Context, raw []byte, opts bundleImportOptions) error {
	b, err := parseBundle(raw)
	if err != nil {
		return err
	}
	plan, err := buildImportPlan(b, opts.source)
	if err != nil {
		return err
	}
	cfg, err := debugutil.LoadDBConfig(opts.dbConfigPath)
	if err != nil {
		return err
	}
	localZone := opts.localZone
	if localZone == 0 {
		localZone = cfg.ZoneId
	}
	if localZone == 0 {
		return errors.New("本地 zone 未知:db config 没有 ZoneId,用 -zone 指定")
	}
	// 写进 go/db 会读的那个库:本地没有落点记录时 go/db 按 home_zone 派生 zone_{zone}_db(placement.StoreDBName)。
	// db.yaml 写了别的 DBName 时导入与 go/db 必然各走各的库,直接拒绝。
	if !placement.IsZoneStorage(localZone) {
		return fmt.Errorf("本地 zone %d 不是合法的 zone 库编号", localZone)
	}
	targetDB, _ := placement.StoreDBName(localZone)
	if name := strings.TrimSpace(cfg.ServerConfig.Database.DBName); name != "" && name != targetDB {
		return fmt.Errorf("db.yaml 的 DBName=%s 与本地 zone %d 的库 %s 不同:go/db 按 zone 派生库名,导入写进 DBName 会读不到;去掉 DBName 或改 -zone", name, localZone, targetDB)
	}
	dsCfg, err := debugutil.LoadDataServiceConfig(opts.configPath)
	if err != nil {
		return err
	}
	core, mapping := cfg.CoreRedis(), cfg.MappingRedis()

	printBundlePlan(b, plan, localZone, opts)
	fmt.Fprintf(os.Stderr, "  写目标:      MySQL %s/%s;核心 Redis %s;映射登记 %s(data_service MappingRedis)\n",
		cfg.ServerConfig.Database.Hosts, targetDB, core.Describe(), dsCfg.MappingRedis.Host)
	if opts.dryRun {
		fmt.Fprintf(os.Stderr, "\n[DRY RUN] 未写入。确认上面的写目标都是本地隔离环境后加 --dry-run=false 执行。\n")
		return nil
	}

	// 写前闸门一:写目标必须是本机(或显式放行的本地服务名)。先按配置校验,再建任何连接。
	if err := debugutil.CheckLocalWriteTarget("MySQL", cfg.ServerConfig.Database.Hosts, opts.allowHosts); err != nil {
		return err
	}
	if err := debugutil.CheckLocalWriteTarget("核心 Redis", core.Hosts, opts.allowHosts); err != nil {
		return err
	}
	if err := checkDataServiceTargets(dsCfg, opts.allowHosts); err != nil {
		return err
	}

	db, _, _, err := debugutil.OpenZoneDBNamed(ctx, opts.dbConfigPath, localZone, targetDB)
	if err != nil {
		return err
	}
	defer db.Close()
	rdb, err := debugutil.OpenRedis(ctx, core)
	if err != nil {
		return err
	}
	defer rdb.Close()
	mappingClient := rdb
	if mapping != core {
		if mappingClient, err = debugutil.OpenRedis(ctx, mapping); err != nil {
			return fmt.Errorf("go/db 映射 Redis: %w", err)
		}
		defer mappingClient.Close()
	}

	// 写前闸门二:目标实例不能就是导出来源。port-forward 之后线上实例也在 127.0.0.1,只看地址拦不住。
	if err := checkNotExportSource(ctx, b.Sources, db, rdb, mappingClient, dsCfg); err != nil {
		return err
	}

	svcCtx, cleanup, err := debugutil.NewServiceContext(opts.configPath)
	if err != nil {
		return err
	}
	defer cleanup()

	local, err := inspectLocalPlayer(ctx, db, rdb, mappingClient, svcCtx.Router, plan)
	if err != nil {
		return err
	}
	if err := local.admit(opts.replace, localZone); err != nil {
		return err
	}

	epoch := max(plan.baseEpoch, local.ownerEpoch) + 1
	version := savebarrier.Version(epoch, plan.seq)
	fmt.Fprintf(os.Stderr, "\n本地重铸版本: owner_epoch=%d save_seq=%d (%s)\n", epoch, plan.seq, version)

	// 三段写入,任一段之后中断都让 login fail-closed(屏障 pending 已是新版本、ACK 未齐 → 冷加载与父快照快路径都拒),
	// 且旧代次的残留存盘被新 owner_epoch 挡住;带 -replace 重跑即可收敛。
	if err := stageLocalRedis(ctx, rdb, plan.playerID, localZone, local.ownerRaw, epoch, version); err != nil {
		return fmt.Errorf("第 1 步(Redis 预置屏障)失败,未写任何数据,可直接重跑: %w", err)
	}
	if err := writeLocalRows(ctx, db, plan, epoch); err != nil {
		return fmt.Errorf("第 2 步(写 %s)失败;Redis 屏障已前进、ACK 未齐,login 会拒绝该玩家,加 -replace 重跑: %w", targetDB, err)
	}
	fmt.Fprintf(os.Stderr, "  [OK] %s: %d 张表已写入\n", targetDB, len(plan.rows))
	if err := commitLocalRedis(ctx, rdb, plan.playerID, version, opts.localAccount); err != nil {
		return fmt.Errorf("第 3 步(Redis 补齐屏障 ACK)失败;SQL 已写入,login 会拒绝该玩家,加 -replace 重跑: %w", err)
	}
	fmt.Fprintf(os.Stderr, "  [OK] 核心 Redis: 屏障 / owner_epoch 已对齐,本地父快照与子表缓存已清\n")

	if !local.mapped {
		if err := svcCtx.Router.RegisterPlayerZone(ctx, plan.playerID, localZone, 0); err != nil {
			return fmt.Errorf("登记 player:zone 失败(数据已写入,修好映射后重跑或手工登记): %w", err)
		}
		fmt.Fprintf(os.Stderr, "  [OK] 映射: player %d -> zone %d\n", plan.playerID, localZone)
	}

	fmt.Fprintf(os.Stderr, "\n[OK] 玩家 %d 已导入本地 zone %d(来源 %s)。\n", plan.playerID, localZone, plan.source)
	if opts.localAccount != "" {
		fmt.Fprintf(os.Stderr, "下一步:用账号 %s 先 Login 一次(建出账号目录),再 EnterGame player_id=%d;EnterGame 会按反向索引把角色补进账号目录。\n",
			opts.localAccount, plan.playerID)
	} else {
		fmt.Fprintf(os.Stderr, "未给 -local-account:本地没有账号拥有这个角色,EnterGame 会被拒。需要登录时加 -local-account=<本地 dev_/robot_ 账号> -replace 重跑。\n")
	}
	return nil
}

func printBundlePlan(b *debugutil.Bundle, plan *importPlan, localZone uint32, opts bundleImportOptions) {
	fmt.Fprintf(os.Stderr, "=== 导入预览 (player-bundle) ===\n")
	fmt.Fprintf(os.Stderr, "  玩家:        %d\n", b.PlayerID)
	fmt.Fprintf(os.Stderr, "  导出:        %s by %s,原因 %q\n", b.ExportedAtUTC, b.Operator, b.Reason)
	fmt.Fprintf(os.Stderr, "  线上落点:    home_zone=%d storage=%s(%s)\n", b.Routing.HomeZone, b.Routing.StorageDB, b.Routing.ResolvedBy)
	if b.ServerRevision != "" {
		fmt.Fprintf(os.Stderr, "  线上版本:    %s(本地检出需与之一致,否则 proto 字段可能错位)\n", b.ServerRevision)
	}
	fmt.Fprintf(os.Stderr, "  线上判读:    %s\n", b.Analysis.Barrier.Conclusion)
	for _, warning := range b.Analysis.Warnings {
		fmt.Fprintf(os.Stderr, "  导出警告:    %s\n", warning)
	}
	fmt.Fprintf(os.Stderr, "  数据来源:    %s\n", plan.source)
	fmt.Fprintf(os.Stderr, "  本地目标:    zone %d\n", localZone)
	for _, row := range plan.rows {
		fmt.Fprintf(os.Stderr, "    %-20s %6d bytes\n", row.table, proto.Size(row.msg))
	}
	fmt.Fprintf(os.Stderr, "  线上最大代次 %d,最大存盘序号 %d;本地版本 = max(线上, 本地) + 1\n", plan.baseEpoch, plan.seq)
	if opts.localAccount != "" {
		fmt.Fprintf(os.Stderr, "  本地账号:    %s(写 player_to_account 反向索引)\n", opts.localAccount)
	}
	for _, note := range plan.notes {
		fmt.Fprintf(os.Stderr, "  注意: %s\n", note)
	}
}

// checkNotExportSource 比对实例身份(Redis run_id、MySQL server_uuid):任一写目标与导出包记录的来源是同一实例就拒绝。
// 身份取不到(空串)的一侧不参与比较。
func checkNotExportSource(ctx context.Context, src debugutil.BundleSources, db *sql.DB, core, mapping *redis.Client, dsCfg config.Config) error {
	exported := map[string]bool{}
	for _, id := range []string{src.CoreRedisID, src.MappingRedisID, src.ZoneMySQLID, src.GlobalMySQLID} {
		if id != "" {
			exported[id] = true
		}
	}
	targets := []struct{ what, id string }{
		{"MySQL", debugutil.MySQLServerIdentity(ctx, db)},
		{"核心 Redis", debugutil.RedisServerIdentity(ctx, core)},
		{"映射 Redis", debugutil.RedisServerIdentity(ctx, mapping)},
	}
	if host := strings.TrimSpace(dsCfg.MappingRedis.Host); host != "" {
		client := redis.NewClient(&redis.Options{Addr: host, Password: dsCfg.MappingRedis.Pass})
		targets = append(targets, struct{ what, id string }{"data_service MappingRedis", debugutil.RedisServerIdentity(ctx, client)})
		_ = client.Close()
	}
	for _, target := range targets {
		if target.id != "" && exported[target.id] {
			return fmt.Errorf("%s 与导出来源是同一个实例(身份 %s):导入只写隔离环境,检查是不是把线上 port-forward 当成了本地", target.what, target.id)
		}
	}
	return nil
}

// localPlayerState 是导入前本地已有的同号状态。
type localPlayerState struct {
	sqlTables  []string
	redisKeys  []string
	onlineKeys []string
	ownerRaw   string
	ownerEpoch uint64
	mapped     bool
	mappedZone uint32
	// placementRaw / placementPresent:go/db 选库用的映射实例上的 player:placement:<id>。
	placementRaw     string
	placementPresent bool
}

func inspectLocalPlayer(ctx context.Context, db *sql.DB, rdb, mappingClient *redis.Client, router *routing.Router, plan *importPlan) (*localPlayerState, error) {
	state := &localPlayerState{}
	for _, row := range plan.rows {
		newMsg := func() proto.Message { return row.msg.ProtoReflect().Type().New().Interface() }
		_, found, err := debugutil.ReadPlayerRow(ctx, db, newMsg, plan.playerID)
		if err != nil {
			return nil, fmt.Errorf("读本地 %s 失败(本地表结构与当前检出不一致时先跑本地迁移): %w", row.table, err)
		}
		if found {
			state.sqlTables = append(state.sqlTables, row.table)
		}
	}

	id := plan.playerID
	entries, err := debugutil.ReadRedisKeys(ctx, rdb, []debugutil.RedisKeySpec{
		{Key: debugutil.ParentKey(id), Kind: debugutil.RedisKindString},
		{Key: ownerepoch.OwnerEpochKey(id), Kind: debugutil.RedisKindString},
		{Key: savebarrier.Key(id), Kind: debugutil.RedisKindHash},
		{Key: scenenode.LocationKey(id), Kind: debugutil.RedisKindString},
		{Key: debugutil.LocatorSessionKey(id), Kind: debugutil.RedisKindString},
	})
	if err != nil {
		return nil, err
	}
	for i, entry := range entries {
		if entry.Type == "none" {
			continue
		}
		if i >= 3 {
			state.onlineKeys = append(state.onlineKeys, entry.Key)
			continue
		}
		state.redisKeys = append(state.redisKeys, entry.Key)
	}
	if owner := entries[1]; owner.Type != "none" {
		if owner.Type != "string" || owner.Value == nil {
			return nil, fmt.Errorf("本地 %s 类型异常(%s)", owner.Key, owner.Type)
		}
		state.ownerRaw = owner.Value.Text
		if state.ownerEpoch, err = ownerepoch.ParseEpoch(state.ownerRaw); err != nil {
			return nil, fmt.Errorf("本地 owner_epoch 畸形: %w", err)
		}
	}

	placementEntries, err := debugutil.ReadRedisKeys(ctx, mappingClient, []debugutil.RedisKeySpec{
		{Key: placement.Key(id), Kind: debugutil.RedisKindString},
	})
	if err != nil {
		return nil, err
	}
	if placementEntries[0].Type != "none" {
		state.placementPresent = true
		state.placementRaw = debugutil.EntryText(&placementEntries[0])
	}

	zone, err := router.GetPlayerHomeZone(ctx, id)
	switch {
	case err == nil:
		state.mapped, state.mappedZone = true, zone
	case errors.Is(err, routing.ErrHomeZoneNotMapped):
	default:
		return nil, fmt.Errorf("读本地 player:zone 失败: %w", err)
	}
	return state, nil
}

// admit 决定能否写入:
//   - 本地同号玩家在线一律拒绝(scene 里的内存态会把导入的数据存回去覆盖);
//   - 本地映射指向别的 zone,或本地落点记录把这个号钉到别的库 / 处于冻结态时拒绝:go/db 会去别处读写,导入的行读不到;
//   - 本地已有同号存档时要 -replace。
func (s *localPlayerState) admit(replace bool, localZone uint32) error {
	if len(s.onlineKeys) > 0 {
		return fmt.Errorf("本地同号玩家在线或会话未释放(%s),先让他下线、等会话过期后再导入", strings.Join(s.onlineKeys, ", "))
	}
	if s.mapped && s.mappedZone != localZone {
		return fmt.Errorf("本地 player:zone 已指向 zone %d(不是本地 zone %d);用 -zone=%d 导入,或先在本地清掉这条映射",
			s.mappedZone, localZone, s.mappedZone)
	}
	if s.placementPresent {
		rec, err := placement.Parse(s.placementRaw)
		switch {
		case err != nil:
			return fmt.Errorf("本地落点记录 %q 畸形,go/db 会对该玩家 fail-closed;先在本地删掉 player:placement 记录: %w", s.placementRaw, err)
		case rec.Frozen:
			return fmt.Errorf("本地落点记录 %q 处于冻结态(搬库中),go/db 的写会一直延后;先在本地结束或清掉这条记录", s.placementRaw)
		case rec.StorageID != localZone:
			dbName, _ := placement.StoreDBName(rec.StorageID)
			return fmt.Errorf("本地落点记录 %q 把该玩家钉在 %s,go/db 会去那里读写,导入 zone %d 的行读不到;先在本地删掉 player:placement 记录", s.placementRaw, dbName, localZone)
		}
	}
	existing := append(append([]string{}, s.sqlTables...), s.redisKeys...)
	if len(existing) > 0 && !replace {
		return fmt.Errorf("本地已有该 player_id 的存档(%s),会被覆盖;确认可以覆盖后加 -replace", strings.Join(existing, ", "))
	}
	return nil
}

// writeLocalRows 在一个 RC 事务里按表名升序替换两张行,行内围栏写成本地版本。
func writeLocalRows(ctx context.Context, db *sql.DB, plan *importPlan, epoch uint64) error {
	tx, err := db.BeginTx(ctx, importTableTxOptions())
	if err != nil {
		return err
	}
	defer tx.Rollback()
	for _, row := range plan.rows {
		msg := proto.Clone(row.msg)
		if err := debugutil.SetRowFence(msg, epoch, plan.seq); err != nil {
			return err
		}
		if err := debugutil.ReplacePlayerRow(ctx, tx, msg); err != nil {
			return fmt.Errorf("表 %s: %w", row.table, err)
		}
	}
	return tx.Commit()
}

// stageLocalRedisScript 是三段写入的第 1 段,先原子复核预检结论:同号玩家此刻不在线、owner_epoch 与预检一致。
// 然后删掉本地父快照 / 子表缓存 / 资产证据 / 交接标记 / 旧屏障,屏障只写 pending(不写 ACK)、owner 前进到新代次,
// 并把玩家移出待落库索引。此后 login 见 pending≠ACK 必拒(含父快照快路径),旧代次的存盘被 owner 挡住。
//
// 父快照、owner 与屏障必须在同一实例(login parent_commit_barrier 的前提);多键脚本不支持 Cluster。
var stageLocalRedisScript = redis.NewScript(stageLocalRedisLua)

const stageLocalRedisLua = `
if redis.call('EXISTS', KEYS[8]) == 1 or redis.call('EXISTS', KEYS[9]) == 1 then
    return redis.error_reply('IMPORT_ONLINE local player became online since preflight')
end
local owner = redis.call('GET', KEYS[7])
if (owner or '') ~= ARGV[1] then
    return redis.error_reply('IMPORT_OWNER_CHANGED owner_epoch changed since preflight')
end
redis.call('DEL', KEYS[1], KEYS[2], KEYS[3], KEYS[4], KEYS[5], KEYS[6])
redis.call('HSET', KEYS[6], 'pending', ARGV[3])
redis.call('SET', KEYS[7], ARGV[2])
redis.call('ZREM', KEYS[10], ARGV[4])
return 1
`

func stageLocalRedis(ctx context.Context, rdb redis.Scripter, playerID uint64, localZone uint32, expectedOwner string, epoch uint64, version string) error {
	keys := []string{
		debugutil.ParentKey(playerID),
		debugutil.SubTableCacheKey(&databasepb.PlayerDatabase{}, playerID),
		debugutil.SubTableCacheKey(&databasepb.PlayerDatabase_1{}, playerID),
		assetop.SQLCommitKey(playerID),
		ownerepoch.HandoffKey(playerID),
		savebarrier.Key(playerID),
		ownerepoch.OwnerEpochKey(playerID),
		scenenode.LocationKey(playerID),
		debugutil.LocatorSessionKey(playerID),
		debugutil.SavePendingIndexKey(localZone, playerID),
	}
	return stageLocalRedisScript.Run(ctx, rdb, keys, expectedOwner, fmt.Sprint(epoch), version, fmt.Sprint(playerID)).Err()
}

// commitLocalRedisScript 是第 3 段:SQL 两行已写成版本 V 之后才补齐两表 ACK;pending 已不是 V(期间被改过)时拒绝。
var commitLocalRedisScript = redis.NewScript(commitLocalRedisLua)

const commitLocalRedisLua = `
if redis.call('HGET', KEYS[1], 'pending') ~= ARGV[1] then
    return redis.error_reply('IMPORT_BARRIER_CHANGED pending changed since stage')
end
redis.call('HSET', KEYS[1], 'player_database', ARGV[1], 'player_database_1', ARGV[1])
if ARGV[2] ~= '' then
    redis.call('SET', KEYS[2], ARGV[2])
end
return 1
`

func commitLocalRedis(ctx context.Context, rdb redis.Scripter, playerID uint64, version, localAccount string) error {
	keys := []string{savebarrier.Key(playerID), debugutil.PlayerToAccountKey(playerID)}
	return commitLocalRedisScript.Run(ctx, rdb, keys, version, localAccount).Err()
}

// checkDataServiceTargets 检查 Router.RegisterPlayerZone 会写的目标:映射 Redis,db_first 时还有全局库的 player_route。
func checkDataServiceTargets(c config.Config, allowHosts string) error {
	if err := debugutil.CheckLocalWriteTarget("data_service MappingRedis", c.MappingRedis.Host, allowHosts); err != nil {
		return err
	}
	if c.RouteStore.Normalize().DBFirst() {
		if err := debugutil.CheckLocalWriteTarget("data_service 全局库(RouteStore db_first)", c.SnapshotMySQL.Host, allowHosts); err != nil {
			return err
		}
	}
	return nil
}
