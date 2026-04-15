package logic

import (
	"context"
	"fmt"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"

	"proto/scene_manager"
	"scene_manager/internal/config"
	"scene_manager/internal/svc"
)

// testZoneId is the zone ID used across all tests.
const testZoneId uint32 = 1

// testLoadKey returns the Redis load-set key for the test zone.
func testLoadKey() string {
	return nodeLoadKey(testZoneId)
}

// ---------------------------------------------------------------------------
// Test helper
// ---------------------------------------------------------------------------

func newTestSvcCtx(t *testing.T, nodeID string) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)

	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	t.Cleanup(func() {
		// go-zero Redis doesn't expose Close, but miniredis shuts down on test end
	})

	c := config.Config{}
	c.NodeID = nodeID

	return &svc.ServiceContext{
		Config: c,
		Redis:  rds,
	}, mr
}

// ---------------------------------------------------------------------------
// PlayerLocation tests
// ---------------------------------------------------------------------------

func TestUpdateAndGetPlayerLocation(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	err := UpdatePlayerLocation(ctx, sc, 1001, 42, "scene-node-1")
	require.NoError(t, err)

	loc, err := GetPlayerLocation(ctx, sc, 1001)
	require.NoError(t, err)
	require.NotNil(t, loc)

	assert.Equal(t, uint64(42), loc.SceneId)
	assert.Equal(t, "scene-node-1", loc.NodeId)
	assert.True(t, loc.UpdateTime > 0)
}

func TestGetPlayerLocation_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	loc, err := GetPlayerLocation(ctx, sc, 9999)
	// go-zero Redis.Get returns redis.Nil as error for missing keys
	// The function may return error or nil depending on error handling
	if err != nil {
		// Acceptable: returns error for missing key
		return
	}
	assert.Nil(t, loc)
}

func TestDeletePlayerLocation(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	// Create
	err := UpdatePlayerLocation(ctx, sc, 2001, 100, "node-a")
	require.NoError(t, err)

	// Verify exists
	loc, err := GetPlayerLocation(ctx, sc, 2001)
	require.NoError(t, err)
	require.NotNil(t, loc)

	// Delete
	err = DeletePlayerLocation(ctx, sc, 2001)
	require.NoError(t, err)

	// Verify gone
	loc, err = GetPlayerLocation(ctx, sc, 2001)
	if err == nil {
		assert.Nil(t, loc)
	}
}

func TestUpdatePlayerLocation_Overwrite(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	// First location
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 3001, 10, "node-a"))

	// Overwrite with new location (player changed scene)
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 3001, 20, "node-b"))

	loc, err := GetPlayerLocation(ctx, sc, 3001)
	require.NoError(t, err)
	assert.Equal(t, uint64(20), loc.SceneId)
	assert.Equal(t, "node-b", loc.NodeId)
}

// ---------------------------------------------------------------------------
// Load balancing tests
// ---------------------------------------------------------------------------

func TestGetBestNode_SelectsLowestLoad(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "node-self")
	ctx := context.Background()

	// Register 3 nodes with different loads (numeric IDs)
	mr.ZAdd(testLoadKey(), 50, "3")
	mr.ZAdd(testLoadKey(), 5, "1")
	mr.ZAdd(testLoadKey(), 20, "2")

	best, err := GetBestNode(ctx, sc, testZoneId)
	require.NoError(t, err)
	assert.Equal(t, "1", best)
}

func TestGetBestNode_NoNodes_ReturnsError(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-fallback")
	ctx := context.Background()

	// No nodes registered in the ZSet
	_, err := GetBestNode(ctx, sc, testZoneId)
	assert.Error(t, err)
}

func TestGetBestNode_TiedLoad(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "node-self")
	ctx := context.Background()

	// All nodes have same load (numeric IDs)
	mr.ZAdd(testLoadKey(), 10, "10")
	mr.ZAdd(testLoadKey(), 10, "20")
	mr.ZAdd(testLoadKey(), 10, "30")

	best, err := GetBestNode(ctx, sc, testZoneId)
	require.NoError(t, err)
	// Should return one of the tied nodes (Redis ZRANGE returns lexicographic order on tie)
	assert.Contains(t, []string{"10", "20", "30"}, best)
}

func TestGetBestNode_SingleNode(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "node-self")
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 99, "42")

	best, err := GetBestNode(ctx, sc, testZoneId)
	require.NoError(t, err)
	assert.Equal(t, "42", best)
}

// ---------------------------------------------------------------------------
// Key format test
// ---------------------------------------------------------------------------

func TestPlayerLocationKeyFormat(t *testing.T) {
	key := getPlayerLocationKey(12345)
	assert.Equal(t, "player:12345:location", key)
}

// ---------------------------------------------------------------------------
// Multiple players coexist
// ---------------------------------------------------------------------------

func TestMultiplePlayersLocation(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	// Place multiple players
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 100, 1, "node-a"))
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 200, 2, "node-b"))
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 300, 1, "node-a"))

	// Each player has their own location
	loc1, _ := GetPlayerLocation(ctx, sc, 100)
	loc2, _ := GetPlayerLocation(ctx, sc, 200)
	loc3, _ := GetPlayerLocation(ctx, sc, 300)

	assert.Equal(t, uint64(1), loc1.SceneId)
	assert.Equal(t, "node-a", loc1.NodeId)

	assert.Equal(t, uint64(2), loc2.SceneId)
	assert.Equal(t, "node-b", loc2.NodeId)

	assert.Equal(t, uint64(1), loc3.SceneId)
	assert.Equal(t, "node-a", loc3.NodeId)

	// Delete one, others unaffected
	require.NoError(t, DeletePlayerLocation(ctx, sc, 200))
	loc1After, _ := GetPlayerLocation(ctx, sc, 100)
	assert.NotNil(t, loc1After)
}

// ---------------------------------------------------------------------------
// Main scene helpers
// ---------------------------------------------------------------------------

func newTestSvcCtxWithMainScenes(t *testing.T, mainConfIds []uint64) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})

	c := config.Config{}
	c.MainSceneChannelCount = 1
	c.InstanceIdleTimeoutSeconds = 300
	c.InstanceCheckIntervalSeconds = 10

	return &svc.ServiceContext{
		Config:           c,
		Redis:            rds,
		MainSceneConfIds: mainConfIds,
	}, mr
}

func TestIsMainSceneConf(t *testing.T) {
	sc, _ := newTestSvcCtxWithMainScenes(t, []uint64{1001, 1002, 1003})

	assert.True(t, IsMainSceneConf(sc, 1001))
	assert.True(t, IsMainSceneConf(sc, 1003))
	assert.False(t, IsMainSceneConf(sc, 2001))
	assert.False(t, IsMainSceneConf(sc, 0))
}

func TestGetBestMainSceneChannel_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	id, nodeId, _ := GetBestMainSceneChannel(ctx, sc, 9999, testZoneId)
	assert.Equal(t, uint64(0), id)
	assert.Equal(t, "", nodeId)
}

func TestAssignNodeByHash_Deterministic(t *testing.T) {
	nodes := []string{"1", "2", "3", "4"}

	result1 := assignNodeByHash(1001, nodes)
	result2 := assignNodeByHash(1001, nodes)
	assert.Equal(t, result1, result2, "Same confId should always map to same node")

	// Different confIds may map to different nodes (not guaranteed, but likely with 4 nodes)
	results := make(map[string]bool)
	for confId := uint64(1); confId <= 100; confId++ {
		results[assignNodeByHash(confId, nodes)] = true
	}
	assert.Greater(t, len(results), 1, "100 different confIds should map to more than 1 node")
}

// ---------------------------------------------------------------------------
// Instance lifecycle tests
// ---------------------------------------------------------------------------

func TestInstancePlayerCount_IncrDecr(t *testing.T) {
	sc, _ := newTestSvcCtxWithMainScenes(t, nil)

	sceneId := uint64(42)
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, sceneId), "0")

	IncrInstancePlayerCount(sc, sceneId)
	IncrInstancePlayerCount(sc, sceneId)

	val, err := sc.Redis.Get(fmt.Sprintf(InstancePlayerCountKey, sceneId))
	require.NoError(t, err)
	assert.Equal(t, "2", val)

	DecrInstancePlayerCount(sc, sceneId)
	val, _ = sc.Redis.Get(fmt.Sprintf(InstancePlayerCountKey, sceneId))
	assert.Equal(t, "1", val)
}

func TestCreateScene_MainWorld_Idempotent(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	ctx := context.Background()

	// Register a node so GetBestNode works.
	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	// First create — triggers on-demand channel init.
	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)
	assert.NotEqual(t, uint64(0), resp1.SceneId)

	// Second create for same confId — should return same scene (1 channel).
	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.Equal(t, resp1.SceneId, resp2.SceneId, "single-channel main scene should be idempotent")
	assert.Equal(t, resp1.NodeId, resp2.NodeId)
}

func TestCreateScene_Instance_UniquIds(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)

	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp2.ErrorCode)
	assert.NotEqual(t, resp1.SceneId, resp2.SceneId, "instances should get unique IDs")
}

func TestCreateScene_Instance_TrackedInActiveSet(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
		ZoneId:      testZoneId,
	})

	// Should be in the active instances sorted set.
	instKey := activeInstancesKey(testZoneId)
	members, err := sc.Redis.ZrangeWithScores(instKey, 0, -1)
	require.NoError(t, err)
	assert.Len(t, members, 1)
	assert.Equal(t, fmt.Sprintf("%d", resp.SceneId), members[0].Key)
}

// ---------------------------------------------------------------------------
// EnterScene / LeaveScene player-count tests
// ---------------------------------------------------------------------------

func TestEnterScene_IncrementsPlayerCount(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	// Setup: create an instance scene so there's a scene->node mapping.
	mr.ZAdd(testLoadKey(), 0, "10")
	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{SceneConfId: 2001, ZoneId: testZoneId})
	require.NoError(t, err)
	sceneId := resp.SceneId

	// Player count should start at 0.
	countKey := fmt.Sprintf(InstancePlayerCountKey, sceneId)
	val, _ := sc.Redis.Get(countKey)
	assert.Equal(t, "0", val)

	// EnterScene should increment.
	enterLogic := NewEnterSceneLogic(ctx, sc)
	enterResp, err := enterLogic.EnterScene(&scene_manager.EnterSceneRequest{
		PlayerId: 1001,
		SceneId:  sceneId,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), enterResp.ErrorCode)

	val, _ = sc.Redis.Get(countKey)
	assert.Equal(t, "1", val)
}

func TestLeaveScene_DecrementsPlayerCount(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")
	logic := NewCreateSceneLogic(ctx, sc)
	resp, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{SceneConfId: 2001, ZoneId: testZoneId})
	sceneId := resp.SceneId

	// Enter first.
	enterLogic := NewEnterSceneLogic(ctx, sc)
	enterLogic.EnterScene(&scene_manager.EnterSceneRequest{
		PlayerId: 1001,
		SceneId:  sceneId,
	})

	// Leave.
	leaveLogic := NewLeaveSceneLogic(ctx, sc)
	leaveLogic.LeaveScene(&scene_manager.LeaveSceneRequest{
		PlayerId: 1001,
		SceneId:  sceneId,
	})

	countKey := fmt.Sprintf(InstancePlayerCountKey, sceneId)
	val, _ := sc.Redis.Get(countKey)
	assert.Equal(t, "0", val)
}

func TestDecrPlayerCount_NeverGoesNegative(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")

	// Decrement without any prior increment.
	DecrInstancePlayerCount(sc, 999)

	countKey := fmt.Sprintf(InstancePlayerCountKey, 999)
	val, _ := sc.Redis.Get(countKey)
	assert.Equal(t, "0", val, "player count should be clamped to 0, not negative")
}

// ---------------------------------------------------------------------------
// Idempotent re-init tests (node restart / re-appearance)
// ---------------------------------------------------------------------------

func TestInitMainScenes_Idempotent_NoDuplicateRedisEntries(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001, 1002})
	ctx := context.Background()

	// Register a node in the load set.
	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	// First init: allocates scene IDs in Redis.
	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)

	lines1, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	lines2, _ := GetAllMainSceneChannels(ctx, sc, 1002, testZoneId)
	assert.Len(t, lines1, 1, "conf 1001 should have 1 channel")
	assert.Len(t, lines2, 1, "conf 1002 should have 1 channel")
	assert.NotEqual(t, lines1[0], lines2[0], "different configs should get different scene IDs")

	// Second init (simulates node re-appearance): must NOT allocate new IDs.
	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)

	lines1After, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	lines2After, _ := GetAllMainSceneChannels(ctx, sc, 1002, testZoneId)
	assert.Equal(t, lines1, lines1After, "re-init must not change scene IDs for conf 1001")
	assert.Equal(t, lines2, lines2After, "re-init must not change scene IDs for conf 1002")
}

// ---------------------------------------------------------------------------
// Main scene channel (分线) tests
// ---------------------------------------------------------------------------

func TestInitMainScenes_MultipleChannels(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	sc.Config.MainSceneChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")
	mr.ZAdd(nodeLoadKey(testZoneId), 0, "20")

	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)

	lines, err := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.Len(t, lines, 3, "should create 3 channels")

	// All scene IDs should be unique.
	seen := make(map[uint64]bool)
	for _, id := range lines {
		assert.False(t, seen[id], "duplicate scene ID %d", id)
		seen[id] = true
	}
}

func TestInitMainScenes_MultipleChannels_Idempotent(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	sc.Config.MainSceneChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	// First init.
	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)
	linesBefore, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, linesBefore, 3)

	// Second init — must not add extra channels.
	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)
	linesAfter, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, linesAfter, 3, "re-init must not create extra channels")
}

func TestGetBestMainSceneChannel_SelectsLowestPlayerCount(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	sc.Config.MainSceneChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)

	lines, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	require.Len(t, lines, 3)

	// Set different player counts: lines[0]=10, lines[1]=2, lines[2]=5.
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[0]), "10")
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[1]), "2")
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[2]), "5")

	bestId, bestNode, err := GetBestMainSceneChannel(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.Equal(t, lines[1], bestId, "should pick channel with lowest player count")
	assert.NotEmpty(t, bestNode)
}

func TestGetBestMainSceneChannel_AllEmpty(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	sc.Config.MainSceneChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	initMainScenesForZone(ctx, sc, testZoneId, sc.MainSceneConfIds)

	// All channels have player_count = 0 (initialized by init).
	bestId, _, err := GetBestMainSceneChannel(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.NotEqual(t, uint64(0), bestId, "should return some channel even when all empty")
}

func TestCreateScene_MainWorld_MultipleChannels_ReturnsLeastLoaded(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	sc.Config.MainSceneChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	// First call creates channels on demand, returns best.
	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)

	// 3 channels should now exist.
	lines, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, lines, 3)

	// Simulate load: put 100 players on the channel that was returned.
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, resp1.SceneId), "100")

	// Second call should pick a different (less loaded) channel.
	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})
	require.NoError(t, err)
	assert.NotEqual(t, resp1.SceneId, resp2.SceneId, "should pick a less loaded channel")
}

func TestCreateScene_MainWorld_ChannelCountDefault1_BackwardCompat(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, []uint64{1001})
	// MainSceneChannelCount defaults to 1 via newTestSvcCtxWithMainScenes.
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	resp1, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})
	resp2, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
	})

	assert.Equal(t, resp1.SceneId, resp2.SceneId, "ChannelCount=1 -> same scene every time")

	lines, _ := GetAllMainSceneChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, lines, 1, "should have exactly 1 channel")
}
