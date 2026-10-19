package logic

import (
	"context"
	"testing"
	"time"

	"data_service/internal/config"
	"data_service/internal/constants"
	"data_service/internal/routing"
	"data_service/internal/svc"

	"github.com/alicebob/miniredis/v2"
	goredis "github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

func newTestSvcCtx(t *testing.T) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)

	c := config.Config{
		MappingRedis: redis.RedisConf{
			Host: mr.Addr(),
			Type: "node",
		},
		DevRedis: config.DevRedisConfig{
			Host: mr.Addr(),
			DB:   0,
		},
		PlayerLockTTLSec: 3,
	}

	r := routing.NewRouter(c)
	t.Cleanup(func() { r.Close() })

	sc := &svc.ServiceContext{
		Config: c,
		Router: r,
	}
	return sc, mr
}

func setupPlayer(t *testing.T, svcCtx *svc.ServiceContext, playerID uint64, zoneID uint32) {
	t.Helper()
	ctx := context.Background()
	err := svcCtx.Router.RegisterPlayerZone(ctx, playerID, zoneID, 0)
	assert.NoError(t, err)
}

// ── LoadPlayerData ─────────────────────────────────────────────

func TestLoadPlayerData_Empty(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 1, 1)

	resp, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: 1})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	assert.Empty(t, resp.Data)
}

func TestLoadPlayerData_SpecificFields(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 1, 1)

	// Save some data first
	_, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 1,
		Data: map[string][]byte{
			"bag":   []byte("bag_data"),
			"equip": []byte("equip_data"),
			"quest": []byte("quest_data"),
		},
	})
	assert.NoError(t, err)

	// Load specific fields
	resp, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{
		PlayerID: 1,
		Fields:   []string{"bag", "equip"},
	})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	assert.Equal(t, []byte("bag_data"), resp.Data["bag"])
	assert.Equal(t, []byte("equip_data"), resp.Data["equip"])
	assert.Nil(t, resp.Data["quest"]) // not requested
}

func TestLoadPlayerData_AllFields(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 2, 1)

	_, _ = SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 2,
		Data: map[string][]byte{
			"bag":  []byte("b"),
			"attr": []byte("a"),
		},
	})

	resp, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: 2})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	assert.Equal(t, []byte("b"), resp.Data["bag"])
	assert.Equal(t, []byte("a"), resp.Data["attr"])
	assert.True(t, resp.Version > 0)
}

// mgetHook 在下一次 MGET 之前 / 之后各跑一次钩子,把一次存盘精确插进 LoadPlayerData 读路径的缝里。
// 挂在 dev 模式共享的 *goredis.Client 上(go-redis 原生 Hook),被测代码照常走 LoadPlayerData 公开入口。
// 钩子只触发一次,且存盘本身不发 MGET,不会递归。
type mgetHook struct {
	beforeMGet func()
	afterMGet  func()
}

func (h *mgetHook) DialHook(next goredis.DialHook) goredis.DialHook { return next }

func (h *mgetHook) ProcessPipelineHook(next goredis.ProcessPipelineHook) goredis.ProcessPipelineHook {
	return next
}

func (h *mgetHook) ProcessHook(next goredis.ProcessHook) goredis.ProcessHook {
	return func(ctx context.Context, cmd goredis.Cmder) error {
		if cmd.Name() != "mget" {
			return next(ctx, cmd)
		}
		if hook := h.beforeMGet; hook != nil {
			h.beforeMGet = nil
			hook()
		}
		err := next(ctx, cmd)
		if hook := h.afterMGet; hook != nil {
			h.afterMGet = nil
			hook()
		}
		return err
	}
}

func installMGetHook(t *testing.T, svcCtx *svc.ServiceContext, playerID uint64, h *mgetHook) {
	t.Helper()
	client, err := svcCtx.Router.ClientForPlayer(context.Background(), playerID)
	require.NoError(t, err)
	base, ok := client.(*goredis.Client)
	require.Truef(t, ok, "dev 模式下 ClientForPlayer 应返回 *goredis.Client,实际 %T", client)
	base.AddHook(h)
}

func mustSave(t *testing.T, svcCtx *svc.ServiceContext, playerID uint64, data map[string][]byte) uint64 {
	t.Helper()
	resp, err := SavePlayerData(context.Background(), svcCtx, &SavePlayerDataReq{PlayerID: playerID, Data: data})
	require.NoError(t, err)
	require.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	return resp.NewVersion
}

// requireLoadedVersionFencesLaterWrite:读之后已有别的写落地,带 Load 回的版本做 CAS 必须被拒。
// 能通过就说明返回的版本比数据新,调用方会据此覆盖掉中间那次写。
func requireLoadedVersionFencesLaterWrite(t *testing.T, svcCtx *svc.ServiceContext, playerID, loadedVersion uint64) {
	t.Helper()
	require.NotZero(t, loadedVersion, "expected_version=0 会跳过版本校验,用例必须从非 0 版本起步")
	resp, err := SavePlayerData(context.Background(), svcCtx, &SavePlayerDataReq{
		PlayerID:        playerID,
		Data:            map[string][]byte{"bag": []byte("stale-writer")},
		ExpectedVersion: loadedVersion,
	})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeVersionMismatch, resp.ErrorCode,
		"CAS with the loaded version must be rejected: a save landed after the data was read")
}

// TestLoadPlayerData_SaveRacingTheRead 把一次存盘插进读路径的各个缝里,断言 Version 永远不比 Data 新。
// 旧实现先读字段、再单独 GET 版本:存盘落在两步之间时返回「旧数据 + 新版本」,带它 CAS 能通过。
func TestLoadPlayerData_SaveRacingTheRead(t *testing.T) {
	cases := []struct {
		name   string
		fields []string
		// beforeMGet=true:存盘落在 MGET 之前(全量分支即 SCAN 与 MGET 之间),并新建一个 SCAN 没扫到的字段。
		beforeMGet bool
		// wantData 是 Load 应读到的 bag 值。
		wantData string
	}{
		{name: "selected fields, save after data read", fields: []string{"bag"}, wantData: "v1"},
		{name: "all fields, save after data read", wantData: "v1"},
		{name: "all fields, save between scan and mget creates new field", beforeMGet: true, wantData: "v2"},
	}
	for i, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			svcCtx, _ := newTestSvcCtx(t)
			ctx := context.Background()
			playerID := uint64(90 + i)
			setupPlayer(t, svcCtx, playerID, 1)
			firstVersion := mustSave(t, svcCtx, playerID, map[string][]byte{"bag": []byte("v1")})

			racingSave := func() {
				mustSave(t, svcCtx, playerID, map[string][]byte{"bag": []byte("v2"), "mail": []byte("new")})
			}
			h := &mgetHook{}
			if tc.beforeMGet {
				h.beforeMGet = racingSave
			} else {
				h.afterMGet = racingSave
			}
			installMGetHook(t, svcCtx, playerID, h)

			resp, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: playerID, Fields: tc.fields})
			require.NoError(t, err)
			require.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
			require.Nil(t, h.beforeMGet, "racing save must have run")
			require.Nil(t, h.afterMGet, "racing save must have run")
			assert.Equal(t, []byte(tc.wantData), resp.Data["bag"])
			// 三种缝里,返回的版本都应是首次存盘的版本:前两种它与数据同一瞬间,
			// 第三种数据更新,但 SCAN 可能漏掉新字段,只能回 SCAN 之前的版本。
			assert.Equal(t, firstVersion, resp.Version)
			requireLoadedVersionFencesLaterWrite(t, svcCtx, playerID, resp.Version)
		})
	}
}

// TestLoadPlayerData_UncontendedVersionAcceptedByCAS:没有并发写时,Load 回的版本必须能直接 CAS 成功,
// 防止「先读版本」的兜底把正常路径也变成伪冲突。
func TestLoadPlayerData_UncontendedVersionAcceptedByCAS(t *testing.T) {
	for i, fields := range [][]string{nil, {"bag"}} {
		svcCtx, _ := newTestSvcCtx(t)
		ctx := context.Background()
		playerID := uint64(95 + i)
		setupPlayer(t, svcCtx, playerID, 1)
		mustSave(t, svcCtx, playerID, map[string][]byte{"bag": []byte("v1")})
		latest := mustSave(t, svcCtx, playerID, map[string][]byte{"bag": []byte("v2")})

		loaded, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: playerID, Fields: fields})
		require.NoError(t, err)
		require.Equal(t, constants.ErrCodeOK, loaded.ErrorCode)
		assert.Equal(t, latest, loaded.Version, "fields=%v", fields)
		assert.Equal(t, []byte("v2"), loaded.Data["bag"], "fields=%v", fields)

		saved, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
			PlayerID:        playerID,
			Data:            map[string][]byte{"bag": []byte("v3")},
			ExpectedVersion: loaded.Version,
		})
		require.NoError(t, err)
		assert.Equal(t, constants.ErrCodeOK, saved.ErrorCode, "fields=%v", fields)
	}
}

// TestLoadPlayerData_CorruptVersionFailsClosed:版本值损坏时两个分支都回 ErrCodeRedis,不能当成 0(= 跳过 CAS)。
func TestLoadPlayerData_CorruptVersionFailsClosed(t *testing.T) {
	for i, fields := range [][]string{nil, {"bag"}} {
		svcCtx, mr := newTestSvcCtx(t)
		playerID := uint64(97 + i)
		setupPlayer(t, svcCtx, playerID, 1)
		mustSave(t, svcCtx, playerID, map[string][]byte{"bag": []byte("v1")})
		require.NoError(t, mr.Set(versionKey(playerID), "not-a-number"))

		resp, err := LoadPlayerData(context.Background(), svcCtx, &LoadPlayerDataReq{PlayerID: playerID, Fields: fields})
		assert.Error(t, err, "fields=%v", fields)
		assert.Equal(t, constants.ErrCodeRedis, resp.ErrorCode, "fields=%v", fields)
	}
}

// ── SavePlayerData ─────────────────────────────────────────────

func TestSavePlayerData_Basic(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 10, 1)

	resp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 10,
		Data:     map[string][]byte{"hp": []byte("100")},
	})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	assert.Equal(t, uint64(1), resp.NewVersion)

	// Save again, version increments
	resp2, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 10,
		Data:     map[string][]byte{"hp": []byte("200")},
	})
	assert.NoError(t, err)
	assert.Equal(t, uint64(2), resp2.NewVersion)
}

func TestSavePlayerData_VersionCheck_OK(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 20, 1)

	resp, _ := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 20,
		Data:     map[string][]byte{"x": []byte("1")},
	})
	ver := resp.NewVersion

	resp2, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID:        20,
		Data:            map[string][]byte{"x": []byte("2")},
		ExpectedVersion: ver,
	})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp2.ErrorCode)
	assert.Equal(t, ver+1, resp2.NewVersion)
}

func TestSavePlayerData_VersionCheck_Mismatch(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 30, 1)

	SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 30,
		Data:     map[string][]byte{"x": []byte("1")},
	})

	resp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID:        30,
		Data:            map[string][]byte{"x": []byte("2")},
		ExpectedVersion: 999, // wrong version
	})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeVersionMismatch, resp.ErrorCode)
}

func TestSaveFieldsAtomic_ExpiredOwnerCannotWrite(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 31, 1)
	client, err := svcCtx.Router.ClientForPlayer(ctx, 31)
	require.NoError(t, err)

	staleToken, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, 31)
	require.NoError(t, err)
	require.True(t, locked)

	// 让双锁都过期，再由后来者取得同一玩家的锁。
	mr.FastForward(4 * time.Second)
	currentToken, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, 31)
	require.NoError(t, err)
	require.True(t, locked)
	t.Cleanup(func() { _ = svcCtx.Router.ReleasePlayerLock(ctx, client, 31, currentToken) })

	status, _, err := saveFieldsAtomic(ctx, client, 31, staleToken, 0,
		map[string][]byte{"hp": []byte("stale")})
	require.NoError(t, err)
	assert.Equal(t, saveFieldsLockLost, status)

	value, err := GetPlayerField(ctx, svcCtx, 31, "hp")
	require.NoError(t, err)
	assert.Nil(t, value, "TTL 后恢复的旧持锁者不得写入任何字段")

	status, _, err = saveFieldsAtomic(ctx, client, 31, currentToken, 0,
		map[string][]byte{"hp": []byte("current")})
	require.NoError(t, err)
	assert.Equal(t, saveFieldsOK, status)
}

func TestInternalLockFieldCannotBeReadOrOverwritten(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 32, 1)
	client, err := svcCtx.Router.ClientForPlayer(ctx, 32)
	require.NoError(t, err)
	token, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, 32)
	require.NoError(t, err)
	require.True(t, locked)
	t.Cleanup(func() { _ = svcCtx.Router.ReleasePlayerLock(ctx, client, 32, token) })

	loaded, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: 32})
	require.NoError(t, err)
	assert.NotContains(t, loaded.Data, "__lock", "distributed-lock token must never leak as player data")

	explicit, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: 32, Fields: []string{"__lock"}})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeInvalidRequest, explicit.ErrorCode)

	saveResp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 32,
		Data:     map[string][]byte{"__lock": []byte("attacker")},
	})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeInvalidRequest, saveResp.ErrorCode)
	assert.Equal(t, token, client.Get(ctx, routing.PlayerDataLockKey(32)).Val(),
		"reserved-field validation must leave the real lock token untouched")
}

// ── GetPlayerField / SetPlayerField ────────────────────────────

func TestGetSetPlayerField(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 40, 1)

	// Get non-existent field returns nil
	val, err := GetPlayerField(ctx, svcCtx, 40, "gold")
	assert.NoError(t, err)
	assert.Nil(t, val)

	// Set field
	resp, err := SetPlayerField(ctx, svcCtx, 40, "gold", []byte("9999"), 0)
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
	assert.True(t, resp.NewVersion > 0)

	// Get field
	val, err = GetPlayerField(ctx, svcCtx, 40, "gold")
	assert.NoError(t, err)
	assert.Equal(t, []byte("9999"), val)
}

func TestSetPlayerField_VersionMismatch(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 50, 1)

	// First set (creates version 1)
	SetPlayerField(ctx, svcCtx, 50, "level", []byte("1"), 0)

	// Set with wrong expected version
	resp, err := SetPlayerField(ctx, svcCtx, 50, "level", []byte("2"), 999)
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeVersionMismatch, resp.ErrorCode)
}

// ── DeletePlayerData ───────────────────────────────────────────

func TestDeletePlayerData(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 60, 1)

	// Save some data
	SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 60,
		Data: map[string][]byte{
			"bag":  []byte("items"),
			"attr": []byte("stats"),
		},
	})

	// Verify data exists
	resp, _ := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: 60})
	assert.True(t, len(resp.Data) > 0)

	// Delete without zone mapping
	delResp, err := DeletePlayerData(ctx, svcCtx, &DeletePlayerDataReq{
		PlayerID:          60,
		DeleteZoneMapping: false,
	})
	assert.NoError(t, err)
	assert.Equal(t, constants.ErrCodeOK, delResp.ErrorCode)
	assert.True(t, delResp.KeysDeleted > 0)

	// Zone mapping still exists
	zone, err := svcCtx.Router.GetPlayerHomeZone(ctx, 60)
	assert.NoError(t, err)
	assert.Equal(t, uint32(1), zone)
}

// TestDeletePlayerData_ZoneMappingBranchDisabled:DeleteZoneMapping=true 一律拒绝(W3,
// docs/design/player-route-mapping-durability-proposal.md §4.3),而且**零变更** —— 拒绝必须发生在删数据键之前,
// 否则留下"数据没了、映射还在"的半截操作。
func TestDeletePlayerData_ZoneMappingBranchDisabled(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 70, 2)

	_, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 70,
		Data:     map[string][]byte{"x": []byte("y")},
	})
	require.NoError(t, err)

	delResp, err := DeletePlayerData(ctx, svcCtx, &DeletePlayerDataReq{
		PlayerID:          70,
		DeleteZoneMapping: true,
	})
	require.Error(t, err)
	assert.ErrorIs(t, err, ErrZoneMappingDeletionDisabled)
	assert.Equal(t, constants.ErrCodeNotImplemented, delResp.ErrorCode)
	assert.Zero(t, delResp.KeysDeleted)

	// 数据键与映射都还在。
	value, err := GetPlayerField(ctx, svcCtx, 70, "x")
	require.NoError(t, err)
	assert.Equal(t, []byte("y"), value, "被拒绝的删除不得先删掉数据键")
	zone, err := svcCtx.Router.GetPlayerHomeZone(ctx, 70)
	require.NoError(t, err)
	assert.Equal(t, uint32(2), zone, "映射不得被删")
}

func TestDeletePlayerData_ExpiredOwnerCannotDeleteDataOrMapping(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	ctx := context.Background()
	setupPlayer(t, svcCtx, 71, 2)

	_, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID: 71,
		Data:     map[string][]byte{"inventory": []byte("current")},
	})
	require.NoError(t, err)
	client, err := svcCtx.Router.ClientForPlayer(ctx, 71)
	require.NoError(t, err)

	staleToken, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, 71)
	require.NoError(t, err)
	require.True(t, locked)
	mr.FastForward(4 * time.Second)
	currentToken, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, 71)
	require.NoError(t, err)
	require.True(t, locked)
	t.Cleanup(func() { _ = svcCtx.Router.ReleasePlayerLock(ctx, client, 71, currentToken) })

	owned, err := deletePlayerKeysAtomic(ctx, client, routing.PlayerDataLockKey(71), staleToken,
		[]string{playerField(71, "inventory")})
	require.NoError(t, err)
	assert.False(t, owned)

	owned, _, err = svcCtx.Router.DeletePlayerZoneIfLocked(ctx, 71, staleToken)
	require.NoError(t, err)
	assert.False(t, owned)

	value, err := GetPlayerField(ctx, svcCtx, 71, "inventory")
	require.NoError(t, err)
	assert.Equal(t, []byte("current"), value)
	zone, err := svcCtx.Router.GetPlayerHomeZone(ctx, 71)
	require.NoError(t, err)
	assert.Equal(t, uint32(2), zone)
}
