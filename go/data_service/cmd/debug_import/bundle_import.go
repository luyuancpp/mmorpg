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
	"shared/ownerepoch"
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
// 写入顺序 SQL → 核心 Redis → 映射。中途失败时本地处于「SQL 有围栏、屏障缺失或不符」的状态,
// login 冷加载 fail-closed 拒绝该玩家,带 -replace 重跑即可收敛;不会出现半套数据被当成有效存档加载。

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

