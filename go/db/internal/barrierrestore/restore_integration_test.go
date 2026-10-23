package barrierrestore

import (
    "strings"
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/binary"
	"fmt"
	"os"
	"strconv"
	"testing"
	"time"

	"github.com/go-sql-driver/mysql"
	"github.com/luyuancpp/proto2mysql"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	componentpb "proto/common/component"
	databasepb "proto/common/database"
	"shared/authoritysql"
	"shared/ownerepoch"
	"shared/placement"
	"shared/savebarrier"
)

// 全部门控为空时遵循默认单测口径；显式集成一旦启用，任何缺项都失败，不能退化为 MySQL 或跳过。
func TestIntegrationBarrierRestoreStrongTiDBWithRedis(t *testing.T) {
	dsn, addr, strict := os.Getenv("BARRIER_RESTORE_TEST_DSN"), os.Getenv("BARRIER_RESTORE_TEST_REDIS"), os.Getenv("BARRIER_RESTORE_REQUIRE_TIDB")
	if dsn == "" && addr == "" && strict == "" {
		t.Skip("未启用独立 BARRIER_RESTORE 真 TiDB/Redis 集成")
	}
	if dsn == "" || addr == "" || strict != "1" {
		t.Fatal("显式集成必须同时指定无库 DSN、Redis 地址和 BARRIER_RESTORE_REQUIRE_TIDB=1")
	}
	f := newRestoreIntegrationFixture(t, dsn, addr)

	t.Run("非零SQL围栏恢复和原计划重入不改资产", func(t *testing.T) {
		id, first, second := f.player(t, 8, 90)
		before := f.assets(t, id)
		p, err := f.r.Prepare(f.ctx, f.schema, f.zone, f.maintenance, []string{strconv.FormatUint(id, 10)}, false)
		require.NoError(t, err)
		require.Equal(t, savebarrier.Version(8, 90), p.Players[0].Target)
		f.noBarrier(t, id)
		for attempt := 0; attempt < 2; attempt++ {
			require.NoError(t, f.r.ApplyOne(f.ctx, p, 0))
			f.barrier(t, id, p.Players[0].Target)
			require.Equal(t, before, f.assets(t, id))
			f.rowsEqual(t, first, second)
		}
	})

	t.Run("历史零围栏双表升级且完整实例原字节不变", func(t *testing.T) {
		id, first, second := f.player(t, 0, 0)
		before := f.assets(t, id)
		instanceBefore, err := proto.Marshal(first.BagComponent.Items[0].Instance)
		require.NoError(t, err)
		p, err := f.r.Prepare(f.ctx, f.schema, f.zone, f.maintenance, []string{strconv.FormatUint(id, 10)}, true)
		require.NoError(t, err)
		require.Equal(t, savebarrier.Version(19, 1), p.Players[0].Target)
		first.FenceOwnerEpoch, first.FenceSaveSeq = 19, 1
		second.FenceOwnerEpoch, second.FenceSaveSeq = 19, 1
		for attempt := 0; attempt < 2; attempt++ {
			require.NoError(t, f.r.ApplyOne(f.ctx, p, 0))
			f.barrier(t, id, p.Players[0].Target)
			require.Equal(t, before, f.assets(t, id), "只可迁移围栏列，所有业务列字节必须保留")
			f.rowsEqual(t, first, second)
		}
		loaded := &databasepb.PlayerDatabase{PlayerId: id}
		require.NoError(t, f.model.WithContext(f.ctx).FindOneByPK(loaded))
		instanceAfter, err := proto.Marshal(loaded.BagComponent.Items[0].Instance)
		require.NoError(t, err)
		require.Equal(t, instanceBefore, instanceAfter, "成长、GUID 与未来 unknown 字段不得重建或丢失")
	})

	t.Run("审定计划后SQL资产变化拒绝且不升级元数据", func(t *testing.T) {
		id, first, second := f.player(t, 0, 0)
		p, err := f.r.Prepare(f.ctx, f.schema, f.zone, f.maintenance, []string{strconv.FormatUint(id, 10)}, true)
		require.NoError(t, err)
		first.Currency.Values[1]++
		changed, err := proto.Marshal(first.Currency)
		require.NoError(t, err)
		_, err = f.r.DB.ExecContext(f.ctx, "UPDATE player_database SET currency=? WHERE player_id=?", changed, strconv.FormatUint(id, 10))
		require.NoError(t, err)
		beforeRejected := f.assets(t, id)
		err = f.r.ApplyOne(f.ctx, p, 0)
		require.ErrorContains(t, err, "SQL 资产行偏离审定计划")
		require.Equal(t, beforeRejected, f.assets(t, id))
		f.rowsEqual(t, first, second)
		f.noBarrier(t, id)
	})

	t.Run("第63桶仍有待投意图时拒绝且零围栏保持原样", func(t *testing.T) {
		id, first, second := f.player(t, 0, 0)
		p, err := f.r.Prepare(f.ctx, f.schema, f.zone, f.maintenance, []string{strconv.FormatUint(id, 10)}, true)
		require.NoError(t, err)
		before := f.assets(t, id)
		key := fmt.Sprintf("player:save_pending:%d:63", f.zone)
		created, err := f.r.Redis.Eval(f.ctx, `if redis.call('EXISTS',KEYS[1]) ~= 0 then return 0 end
redis.call('ZADD',KEYS[1],0,ARGV[1]); return 1`, []string{key}, strconv.FormatUint(id, 10)).Int()
		require.NoError(t, err)
		require.Equal(t, 1, created, "不得借用或修改预先存在的桶")
		f.ownedKeys = append(f.ownedKeys, key)
		err = f.r.ApplyOne(f.ctx, p, 0)
		require.ErrorContains(t, err, "zone pending outbox not drained")
		_, err = f.r.Prepare(f.ctx, f.schema, f.zone, f.maintenance, []string{strconv.FormatUint(id, 10)}, true)
		require.ErrorContains(t, err, "zone pending outbox not drained")
		require.Equal(t, before, f.assets(t, id))
		f.rowsEqual(t, first, second)
		pending, err := f.r.Redis.ZCard(f.ctx, key).Result()
		require.NoError(t, err)
		require.EqualValues(t, 1, pending, "拒绝不能丢弃待投原件")
		f.noBarrier(t, id)
	})
}

type restoreIntegrationFixture struct {
	ctx         context.Context
	r           Restorer
	model       *proto2mysql.DB
	schema      string
	zone        uint32
	maintenance string
	ownedKeys   []string
}

func newRestoreIntegrationFixture(t *testing.T, dsn, addr string, global ...bool) *restoreIntegrationFixture {
	t.Helper()
	parsed, err := mysql.ParseDSN(dsn)
	if err != nil || parsed.DBName != "" {
		t.Fatal("fixture DSN 必须合法且不指定数据库")
	}
	parsed.Timeout, parsed.ReadTimeout, parsed.WriteTimeout = time.Second, 5*time.Second, 5*time.Second
	admin, err := sql.Open("mysql", parsed.FormatDSN())
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, admin.Close()) })
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	t.Cleanup(cancel)
	var nonce [4]byte
	_, err = rand.Read(nonce[:])
	require.NoError(t, err)
	zone := uint32(100000 + binary.BigEndian.Uint32(nonce[:])%900000)
	schema := fmt.Sprintf("zone_%d_db", zone)
	if len(global) > 0 && global[0] {
		schema = fmt.Sprintf("player_store_%d_db", placement.FirstNonZoneStorageID+zone)
	}
	// CREATE 成功才登记清理；名称冲突即失败，不触碰旧库。
	_, err = admin.ExecContext(ctx, "CREATE DATABASE `"+schema+"`")
	require.NoError(t, err)
	t.Cleanup(func() {
		cleanup, done := context.WithTimeout(context.Background(), 15*time.Second)
		defer done()
		_, err := admin.ExecContext(cleanup, "DROP DATABASE `"+schema+"`")
		require.NoError(t, err, "清理本次新建库")
		var remaining int
		require.NoError(t, admin.QueryRowContext(cleanup, "SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name=?", schema).Scan(&remaining))
		require.Zero(t, remaining)
		t.Logf("SQL 清理确认：%s 剩余 0", schema)
	})
	parsed.DBName = schema
	migration, err := authoritysql.OpenForMigration(ctx, parsed.FormatDSN(), schema, true)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, migration.Close()) })
	ddlModel := proto2mysql.NewDB()
	require.NoError(t, ddlModel.OpenDB(migration, schema))
	for _, row := range []proto.Message{&databasepb.PlayerDatabase{}, &databasepb.PlayerDatabase_1{}} {
		ddlModel.RegisterTable(row)
		require.NoError(t, ddlModel.WithContext(ctx).CreateOrUpdateTable(row))
	}
	for _, table := range []string{"player_database", "player_database_1"} {
		var name, ddl string
		require.NoError(t, migration.QueryRowContext(ctx, "SHOW CREATE TABLE `"+table+"`").Scan(&name, &ddl))
		t.Logf("正式夹具 DDL（%s）：%s", name, ddl)
	}
	require.NoError(t, migration.Close())
	handle, err := authoritysql.Open(ctx, parsed.FormatDSN(), schema, true, 1)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, handle.Close()) })
	require.NoError(t, authoritysql.AssertCurrentSession(ctx, handle, true))
	model := proto2mysql.NewDB()
	require.NoError(t, model.OpenDB(handle, schema))
	model.RegisterTable(&databasepb.PlayerDatabase{})
	model.RegisterTable(&databasepb.PlayerDatabase_1{})
	rc := redis.NewClient(&redis.Options{Addr: addr, DB: 0, MaxRetries: -1, DialTimeout: time.Second, ReadTimeout: time.Second, WriteTimeout: time.Second})
	t.Cleanup(func() { require.NoError(t, rc.Close()) })
	require.NoError(t, rc.Ping(ctx).Err())
	f := &restoreIntegrationFixture{ctx: ctx, r: Restorer{DB: handle, Redis: rc, RoutingMode: RoutingModeRedis}, model: model, schema: schema, zone: zone, maintenance: "barrier-integration-" + schema}
	t.Cleanup(func() {
		cleanup, done := context.WithTimeout(context.Background(), 10*time.Second)
		defer done()
		if len(f.ownedKeys) > 0 {
			require.NoError(t, rc.Del(cleanup, f.ownedKeys...).Err())
			remaining, err := rc.Exists(cleanup, f.ownedKeys...).Result()
			require.NoError(t, err)
			require.Zero(t, remaining)
		}
		t.Logf("Redis DB0 清理确认：本次 %d 个键剩余 0", len(f.ownedKeys))
	})
	created, err := rc.SetNX(ctx, MaintenanceKey(zone), f.maintenance, 0).Result()
	require.NoError(t, err)
	require.True(t, created, "不覆盖既有维护工单")
	f.ownedKeys = append(f.ownedKeys, MaintenanceKey(zone))
	return f
}

func (f *restoreIntegrationFixture) player(t *testing.T, epoch, seq uint64) (uint64, *databasepb.PlayerDatabase, *databasepb.PlayerDatabase_1) {
	t.Helper()
	var nonce [8]byte
	_, err := rand.Read(nonce[:])
	require.NoError(t, err)
	// 本专项用 10^12 以内随机号；保留正式自增/分片 DDL，不覆盖分配器高位边界，生产字段与参数仍为 uint64。
	id := binary.BigEndian.Uint64(nonce[:])%999_999_999_999 + 1
	t.Logf("隔离随机玩家 ID：%d（低于 10^12 的测试夹具）", id)
	for _, key := range recoveryKeys(f.zone, id)[1:8] {
		n, err := f.r.Redis.Exists(f.ctx, key).Result()
		require.NoError(t, err)
		require.Zero(t, n, "随机玩家不得借用既有 Redis 状态")
	}
	created, err := f.r.Redis.SetNX(f.ctx, ownerepoch.OwnerEpochKey(id), "19", 0).Result()
	require.NoError(t, err)
	require.True(t, created)
	f.ownedKeys = append(f.ownedKeys, ownerepoch.OwnerEpochKey(id), savebarrier.Key(id))
	storageID, ok := placement.StorageIDFromDBName(f.schema)
	require.True(t, ok)
	for _, mapping := range []struct{ key, value string }{
		{placement.HomeZoneKey(id), strconv.FormatUint(uint64(f.zone), 10)},
		{placement.Key(id), placement.StableValue(storageID, 1)},
	} {
		created, err := f.r.Redis.SetNX(f.ctx, mapping.key, mapping.value, 0).Result()
		require.NoError(t, err)
		require.True(t, created, "不覆盖既有玩家路由映射")
		f.ownedKeys = append(f.ownedKeys, mapping.key)
	}
	instance := &componentpb.ItemComp{ItemId: id ^ 37, ConfigId: 1101, Size: 1, AcquireSeq: 41,
		Equip: &componentpb.EquipInstanceData{EnhanceLevel: 17, Affixes: []*componentpb.EquipAffix{{AttrId: 3, Tier: 2, Value: 79, Seq: 0}}}}
	instance.ProtoReflect().SetUnknown([]byte{0xa0, 0x06, 0x07})
	first := &databasepb.PlayerDatabase{PlayerId: id, FenceOwnerEpoch: epoch, FenceSaveSeq: seq,
		Currency:     &componentpb.CurrencyComp{Values: []uint64{0, 17}},
		BagComponent: &databasepb.BagAllData{Items: []*databasepb.ItemEntry{{ItemUuid: instance.ItemId, ConfigId: instance.ConfigId, StackSize: 1, Instance: instance}}}}
	probe := &componentpb.PlayerStressTestProbe{}
	probe.ProtoReflect().SetUnknown([]byte{0xa0, 0x06, 0x09})
	second := &databasepb.PlayerDatabase_1{PlayerId: id, FenceOwnerEpoch: epoch, FenceSaveSeq: seq, StressTestProbe: probe}
	require.NoError(t, f.model.WithContext(f.ctx).Insert(first))
	require.NoError(t, f.model.WithContext(f.ctx).Insert(second))
	return id, first, second
}

// 独立读取全部非围栏列，保留 NULL/空字节差异；不用被测摘要算法证明自身正确。
func (f *restoreIntegrationFixture) assets(t *testing.T, id uint64) [2]map[string][]byte {
	t.Helper()
	var pair [2]map[string][]byte
	for i, table := range []string{"player_database", "player_database_1"} {
		rows, err := f.r.DB.QueryContext(f.ctx, "SELECT * FROM "+table+" WHERE player_id=?", strconv.FormatUint(id, 10))
		require.NoError(t, err)
		defer rows.Close()
		columns, err := rows.Columns()
		require.NoError(t, err)
		require.True(t, rows.Next())
		values := make([]sql.RawBytes, len(columns))
		args := make([]any, len(columns))
		for j := range values {
			args[j] = &values[j]
		}
		require.NoError(t, rows.Scan(args...))
		pair[i] = make(map[string][]byte)
		for j, name := range columns {
			if name == "fence_owner_epoch" || name == "fence_save_seq" {
				continue
			}
			if values[j] == nil {
				pair[i][name] = nil
			} else {
				pair[i][name] = append([]byte{}, values[j]...)
			}
		}
		require.False(t, rows.Next())
		require.NoError(t, rows.Err())
		require.NoError(t, rows.Close())
	}
	return pair
}

func (f *restoreIntegrationFixture) rowsEqual(t *testing.T, first *databasepb.PlayerDatabase, second *databasepb.PlayerDatabase_1) {
	t.Helper()
	gotFirst, gotSecond := &databasepb.PlayerDatabase{PlayerId: first.PlayerId}, &databasepb.PlayerDatabase_1{PlayerId: first.PlayerId}
	require.NoError(t, f.model.WithContext(f.ctx).FindOneByPK(gotFirst))
	require.NoError(t, f.model.WithContext(f.ctx).FindOneByPK(gotSecond))
	require.True(t, proto.Equal(first, gotFirst), "SQL 资产或围栏与预期不同")
	require.True(t, proto.Equal(second, gotSecond), "第二张 SQL 行与预期不同")
}

func (f *restoreIntegrationFixture) barrier(t *testing.T, id uint64, version string) {
	t.Helper()
	fields, err := f.r.Redis.HGetAll(f.ctx, savebarrier.Key(id)).Result()
	require.NoError(t, err)
	require.Equal(t, map[string]string{"pending": version, "player_database": version, "player_database_1": version, "schema_gen": "1", "handoff_fence": assertRestoredFence(t, fields["handoff_fence"], "19", "", "0", strings.Repeat("0", 32))}, fields)
	ttl, err := f.r.Redis.PTTL(f.ctx, savebarrier.Key(id)).Result()
	require.NoError(t, err)
	require.Equal(t, -time.Nanosecond, ttl, "提交屏障必须永久保留")
	owner, err := f.r.Redis.Get(f.ctx, ownerepoch.OwnerEpochKey(id)).Result()
	require.NoError(t, err)
	require.Equal(t, "19", owner, "工具不能重置归属计数器")
}

func (f *restoreIntegrationFixture) noBarrier(t *testing.T, id uint64) {
	t.Helper()
	n, err := f.r.Redis.Exists(f.ctx, savebarrier.Key(id)).Result()
	require.NoError(t, err)
	require.Zero(t, n, "未完成 SQL 核验不得发布屏障")
}
