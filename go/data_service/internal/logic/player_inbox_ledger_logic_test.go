package logic

import (
	"context"
	"testing"

	"data_service/internal/config"
	"data_service/internal/routing"
	"data_service/internal/svc"
	componentpb "proto/common/component"
	dbpb "proto/common/database"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// GetPlayerInboxLedger 的契约测试(docs/design/player-inbox-service.md §5.1、§9.3)。
// 语义表与 GetPlayerAssetOpLedger 相同(共用 readPersistedPlayerDatabase),这里逐行再钉一遍:
// 对账方把 found=false 计为 unknown、不告警,任何一条错误路径退化成 found=false 都会把故障藏起来。
// 装配与 asset_op_ledger_logic_test.go 相同(miniredis + 真 Router,dev 单 Redis 模式),复用其中的
// ledgerTestPlayerID / ledgerTestKey / putPlayerBlob;不涉及时间。

// sampleInboxLedger 造一份每个字段都非零的收件箱账本,逐字段比较才有意义。
func sampleInboxLedger() *componentpb.PlayerInboxLedgerComp {
	return &componentpb.PlayerInboxLedgerComp{
		Cursors: []*componentpb.PlayerInboxDomainCursor{{
			Domain:            1, // PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION
			DecidedThroughSeq: 7340032 << 12,
			Recent: []*componentpb.PlayerInboxDecision{
				{Seq: (7340032 << 12) - 1, Outcome: 2, Reason: 29001, RequestId: 900001, MsgType: 3},
				{Seq: 7340032 << 12, Outcome: 1, Reason: 0, RequestId: 900002, MsgType: 1},
			},
		}},
	}
}

// I1:key 不存在(redis.Nil)→ found=false,无 error,账本为 nil。
func TestGetPlayerInboxLedger_I1_AbsentBlob(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)

	ledger, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	assert.False(t, found)
	assert.Nil(t, ledger)
}

// I2:正常 → found=true,收件箱账本逐字段相等,且不会读成同一份 blob 里的资产账本;
// blob 里没有收件箱账本(从未消费过,或由不认识新字段的旧 scene 写出)时回空消息而不是 nil。
func TestGetPlayerInboxLedger_I2_ReturnsPersistedLedger(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	want := sampleInboxLedger()
	putPlayerBlob(t, mr, ledgerTestKey(ledgerTestPlayerID), &dbpb.PlayerAllData{
		PlayerDatabaseData: &dbpb.PlayerDatabase{
			PlayerId:      ledgerTestPlayerID,
			InboxLedger:   want,
			AssetOpLedger: sampleLedger(),
		},
	})

	got, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	require.True(t, found)
	assert.True(t, proto.Equal(want, got), "收件箱账本应逐字段相等: want=%v got=%v", want, got)

	t.Run("从未消费过收件箱", func(t *testing.T) {
		const other uint64 = ledgerTestPlayerID + 1
		putPlayerBlob(t, mr, ledgerTestKey(other), &dbpb.PlayerAllData{
			PlayerDatabaseData: &dbpb.PlayerDatabase{PlayerId: other, AssetOpLedger: sampleLedger()},
		})
		got, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, other)
		require.NoError(t, err)
		require.True(t, found)
		require.NotNil(t, got, "found=true 时账本必须非 nil(空消息),否则对账方分不清有没有 blob")
		assert.Empty(t, got.GetCursors())
	})
}

// I3:blob 的 player_id 与请求不符 → Internal;player_database_data 缺席同样落进这一支。
func TestGetPlayerInboxLedger_I3_PlayerIDMismatchIsInternal(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	putPlayerBlob(t, mr, ledgerTestKey(ledgerTestPlayerID), &dbpb.PlayerAllData{
		PlayerDatabaseData: &dbpb.PlayerDatabase{PlayerId: ledgerTestPlayerID + 9, InboxLedger: sampleInboxLedger()},
	})

	ledger, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	assert.Equal(t, codes.Internal, status.Code(err), "err=%v", err)
	assert.False(t, found)
	assert.Nil(t, ledger)

	t.Run("player_database_data 缺席", func(t *testing.T) {
		const other uint64 = ledgerTestPlayerID + 2
		putPlayerBlob(t, mr, ledgerTestKey(other), &dbpb.PlayerAllData{})
		_, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, other)
		assert.Equal(t, codes.Internal, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})
}

// I4:坏字节 → Internal。
func TestGetPlayerInboxLedger_I4_CorruptBlobIsInternal(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	// 三个都带续位的字节 = 被截断的 varint tag,proto.Unmarshal 必然失败。
	require.NoError(t, mr.Set(ledgerTestKey(ledgerTestPlayerID), "\xff\xff\xff"))

	ledger, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	assert.Equal(t, codes.Internal, status.Code(err), "err=%v", err)
	assert.False(t, found)
	assert.Nil(t, ledger)
}

// I5:读的是 PlayerAllData 全名 key。只往 data_service 自己那套 player:{id}:<field> 写合法 blob,必须读不到。
func TestGetPlayerInboxLedger_I5_ReadsPlayerAllDataKeyOnly(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	blob := &dbpb.PlayerAllData{
		PlayerDatabaseData: &dbpb.PlayerDatabase{PlayerId: ledgerTestPlayerID, InboxLedger: sampleInboxLedger()},
	}
	for _, field := range []string{"inbox_ledger", "player_all_data", "PlayerAllData", "player_database"} {
		putPlayerBlob(t, mr, playerField(ledgerTestPlayerID, field), blob)
	}

	ledger, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	assert.False(t, found, "player:{id}:* 里的数据不得被当成已落盘账本")
	assert.Nil(t, ledger)

	// 反向对照:同一份 blob 放到 PlayerAllData 全名 key 上就能读到。
	putPlayerBlob(t, mr, ledgerTestKey(ledgerTestPlayerID), blob)
	_, found, err = GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	assert.True(t, found)
}

// 语义表其余几行:入参 0、home_zone 查不到、zone Redis 读失败、调用方取消都必须是 gRPC 错误,绝不能退化成 found=false。
func TestGetPlayerInboxLedger_FailureRowsNeverReportAbsent(t *testing.T) {
	t.Run("player_id=0 → InvalidArgument", func(t *testing.T) {
		svcCtx, _ := newTestSvcCtx(t)
		_, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, 0)
		assert.Equal(t, codes.InvalidArgument, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})

	t.Run("router 未装配 → Unavailable", func(t *testing.T) {
		_, found, err := GetPlayerInboxLedger(context.Background(), &svc.ServiceContext{}, ledgerTestPlayerID)
		assert.Equal(t, codes.Unavailable, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})

	t.Run("home_zone 查不到 → Unavailable", func(t *testing.T) {
		// 非 dev 模式:ClientForPlayer 必须先查映射。不配任何 Region,映射也不写。
		mr := miniredis.RunT(t)
		r := routing.NewRouter(config.Config{
			MappingRedis:     redis.RedisConf{Host: mr.Addr(), Type: "node"},
			PlayerLockTTLSec: 3,
		})
		t.Cleanup(func() { r.Close() })
		svcCtx := &svc.ServiceContext{Router: r}

		_, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
		assert.Equal(t, codes.Unavailable, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})

	t.Run("zone Redis 读失败 → Internal", func(t *testing.T) {
		svcCtx, mr := newTestSvcCtx(t)
		mr.SetError("injected failure")
		t.Cleanup(func() { mr.SetError("") })

		_, found, err := GetPlayerInboxLedger(context.Background(), svcCtx, ledgerTestPlayerID)
		assert.Equal(t, codes.Internal, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})

	t.Run("调用方已取消 → Canceled", func(t *testing.T) {
		svcCtx, _ := newTestSvcCtx(t)
		ctx, cancel := context.WithCancel(context.Background())
		cancel()

		_, found, err := GetPlayerInboxLedger(ctx, svcCtx, ledgerTestPlayerID)
		assert.Equal(t, codes.Canceled, status.Code(err), "err=%v", err)
		assert.False(t, found)
	})
}
