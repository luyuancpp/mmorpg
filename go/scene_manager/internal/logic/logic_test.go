package logic

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"

	"proto/scene_manager"
	"scene_manager/internal/config"
	"scene_manager/internal/svc"
	"shared/snowflake"
)

// nowUnix returns the current Unix timestamp for test helpers that need to
// backdate sorted-set scores to drive the idle-instance cleanup pass.
func nowUnix() int64 { return time.Now().Unix() }

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
		Config:     c,
		Redis:      rds,
		SceneIDGen: snowflake.NewNode(0),
	}, mr
}

// ---------------------------------------------------------------------------
// PlayerLocation tests
// ---------------------------------------------------------------------------

func TestUpdateAndGetPlayerLocation(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-1")
	ctx := context.Background()

	err := UpdatePlayerLocation(ctx, sc, 1001, 42, "scene-node-1", testZoneId)
	require.NoError(t, err)

	loc, err := GetPlayerLocation(ctx, sc, 1001)
	require.NoError(t, err)
	require.NotNil(t, loc)

	assert.Equal(t, uint64(42), loc.SceneId)
	assert.Equal(t, "scene-node-1", loc.NodeId)
	assert.True(t, loc.UpdateTime > 0)
	assert.Equal(t, testZoneId, loc.ZoneId)
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
	err := UpdatePlayerLocation(ctx, sc, 2001, 100, "node-a", testZoneId)
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
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 3001, 10, "node-a", testZoneId))

	// Overwrite with new location (player changed scene)
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 3001, 20, "node-b", testZoneId))

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
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 100, 1, "node-a", testZoneId))
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 200, 2, "node-b", testZoneId))
	require.NoError(t, UpdatePlayerLocation(ctx, sc, 300, 1, "node-a", testZoneId))

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
// World scene helpers
// ---------------------------------------------------------------------------

func newTestSvcCtxWithWorldScenes(t *testing.T) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})

	c := config.Config{}
	c.WorldChannelCount = 1
	c.InstanceIdleTimeoutSeconds = 300
	c.InstanceCheckIntervalSeconds = 10

	return &svc.ServiceContext{
		Config:     c,
		Redis:      rds,
		SceneIDGen: snowflake.NewNode(0),
	}, mr
}

func TestIsWorldConf(t *testing.T) {
	// IsWorldConf reads from the World table, which is empty in unit tests.
	assert.False(t, IsWorldConf(1001))
	assert.False(t, IsWorldConf(0))
}

func TestGetBestWorldChannel_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	id, nodeId, _ := GetBestWorldChannel(ctx, sc, 9999, testZoneId)
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
	sc, _ := newTestSvcCtxWithWorldScenes(t)

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
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	// Register a node so GetBestNode works.
	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	// First create — triggers on-demand channel init.
	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)
	assert.NotEqual(t, uint64(0), resp1.SceneId)

	// Second create for same confId — should return same scene (1 channel).
	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})
	require.NoError(t, err)
	assert.Equal(t, resp1.SceneId, resp2.SceneId, "single-channel main scene should be idempotent")
	assert.Equal(t, resp1.NodeId, resp2.NodeId)
}

func TestCreateScene_Instance_UniquIds(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
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
	sc, mr := newTestSvcCtxWithWorldScenes(t)
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
	sc, mr := newTestSvcCtxWithWorldScenes(t)
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
	sc, mr := newTestSvcCtxWithWorldScenes(t)
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

func TestInitWorldScenes_Idempotent_NoDuplicateRedisEntries(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	confIds := []uint64{1001, 1002}
	ctx := context.Background()

	// Register a node in the load set.
	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	// First init: allocates scene IDs in Redis.
	initWorldScenesForZone(ctx, sc, testZoneId, confIds)

	lines1, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	lines2, _ := GetAllWorldChannels(ctx, sc, 1002, testZoneId)
	assert.Len(t, lines1, 1, "conf 1001 should have 1 channel")
	assert.Len(t, lines2, 1, "conf 1002 should have 1 channel")
	assert.NotEqual(t, lines1[0], lines2[0], "different configs should get different scene IDs")

	// Second init (simulates node re-appearance): must NOT allocate new IDs.
	initWorldScenesForZone(ctx, sc, testZoneId, confIds)

	lines1After, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	lines2After, _ := GetAllWorldChannels(ctx, sc, 1002, testZoneId)
	assert.Equal(t, lines1, lines1After, "re-init must not change scene IDs for conf 1001")
	assert.Equal(t, lines2, lines2After, "re-init must not change scene IDs for conf 1002")
}

// ---------------------------------------------------------------------------
// Main scene channel (分线) tests
// ---------------------------------------------------------------------------

func TestInitWorldScenes_MultipleChannels(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	confIds := []uint64{1001}
	sc.Config.WorldChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")
	mr.ZAdd(nodeLoadKey(testZoneId), 0, "20")

	initWorldScenesForZone(ctx, sc, testZoneId, confIds)

	lines, err := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.Len(t, lines, 3, "should create 3 channels")

	// All scene IDs should be unique.
	seen := make(map[uint64]bool)
	for _, id := range lines {
		assert.False(t, seen[id], "duplicate scene ID %d", id)
		seen[id] = true
	}
}

func TestInitWorldScenes_MultipleChannels_Idempotent(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	confIds := []uint64{1001}
	sc.Config.WorldChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	// First init.
	initWorldScenesForZone(ctx, sc, testZoneId, confIds)
	linesBefore, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, linesBefore, 3)

	// Second init — must not add extra channels.
	initWorldScenesForZone(ctx, sc, testZoneId, confIds)
	linesAfter, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, linesAfter, 3, "re-init must not create extra channels")
}

func TestGetBestWorldChannel_SelectsLowestPlayerCount(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	confIds := []uint64{1001}
	sc.Config.WorldChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	initWorldScenesForZone(ctx, sc, testZoneId, confIds)

	lines, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	require.Len(t, lines, 3)

	// Set different player counts: lines[0]=10, lines[1]=2, lines[2]=5.
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[0]), "10")
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[1]), "2")
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, lines[2]), "5")

	bestId, bestNode, err := GetBestWorldChannel(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.Equal(t, lines[1], bestId, "should pick channel with lowest player count")
	assert.NotEmpty(t, bestNode)
}

func TestGetBestWorldChannel_AllEmpty(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	confIds := []uint64{1001}
	sc.Config.WorldChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(nodeLoadKey(testZoneId), 0, "10")

	initWorldScenesForZone(ctx, sc, testZoneId, confIds)

	// All channels have player_count = 0 (initialized by init).
	bestId, _, err := GetBestWorldChannel(ctx, sc, 1001, testZoneId)
	require.NoError(t, err)
	assert.NotEqual(t, uint64(0), bestId, "should return some channel even when all empty")
}

func TestCreateScene_MainWorld_MultipleChannels_ReturnsLeastLoaded(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	sc.Config.WorldChannelCount = 3
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	// First call creates channels on demand, returns best.
	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)

	// 3 channels should now exist.
	lines, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, lines, 3)

	// Simulate load: put 100 players on the channel that was returned.
	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, resp1.SceneId), "100")

	// Second call should pick a different (less loaded) channel.
	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})
	require.NoError(t, err)
	assert.NotEqual(t, resp1.SceneId, resp2.SceneId, "should pick a less loaded channel")
}

func TestCreateScene_MainWorld_ChannelCountDefault1_BackwardCompat(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	// WorldChannelCount defaults to 1 via newTestSvcCtxWithWorldScenes.
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	resp1, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})
	resp2, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
		ZoneId:      testZoneId,
		SceneType:   1,
	})

	assert.Equal(t, resp1.SceneId, resp2.SceneId, "ChannelCount=1 -> same scene every time")

	lines, _ := GetAllWorldChannels(ctx, sc, 1001, testZoneId)
	assert.Len(t, lines, 1, "should have exactly 1 channel")
}

// ---------------------------------------------------------------------------
// Mirror co-location — additional edge cases
// ---------------------------------------------------------------------------
//
// The happy path, dead-source fallback, overload fallback, and explicit
// TargetNodeId override are covered above. These extra cases pin:
//   - single-node deployments (trivial co-location candidate),
//   - missing scene->node mapping (different from "node dead"),
//   - under-cap still co-locates (guards against the inequality flipping),
//   - non-mirror requests do NOT accidentally consume source data.

func TestCreateScene_Mirror_SingleNode_Trivial(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	// Only one node in the zone. Co-location is trivially satisfied; this
	// test makes sure the single-node path doesn't regress when we evolve
	// the decision logic later.
	mr.ZAdd(testLoadKey(), 0, "7")
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(55555)), "7")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  55555,
		MirrorConfigId: 9002,
	})
	require.NoError(t, err)
	assert.Equal(t, "7", resp.NodeId)
}

func TestCreateScene_Mirror_NoSourceMapping_FallsBackToBestNode(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	// Two live nodes; SourceSceneId has NO scene:<id>:node key at all
	// (different from "node is dead"). Expect clean fallback to GetBestNode.
	mr.ZAdd(testLoadKey(), 5, "1")
	mr.ZAdd(testLoadKey(), 50, "2")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  999999,
		MirrorConfigId: 9003,
	})
	require.NoError(t, err)
	assert.Equal(t, "1", resp.NodeId, "no source mapping -> fall back to GetBestNode")
}

func TestCreateScene_Mirror_SourceUnderLoadCap_StillColocates(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	sc.Config.MirrorSourceNodeLoadCap = 100
	ctx := context.Background()

	// Source node "hot" is within the cap (scene_count=3 < cap=100), so
	// co-location still wins even though "cool" has a lower best-node score.
	mr.ZAdd(testLoadKey(), 100, "hot")
	mr.ZAdd(testLoadKey(), 0, "cool")
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(33334)), "hot")
	sc.Redis.Set(fmt.Sprintf(NodeSceneCountKey, "hot"), "3")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  33334,
		MirrorConfigId: 9006,
	})
	require.NoError(t, err)
	assert.Equal(t, "hot", resp.NodeId, "source under cap -> still co-locate")
}

func TestCreateScene_Instance_NonMirror_UsesGetBestNode(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	// Seed a scene->node mapping just to verify non-mirror requests do NOT
	// accidentally read it. SourceSceneId omitted, so GetBestNode wins.
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(11111)), "A")
	mr.ZAdd(testLoadKey(), 100, "A")
	mr.ZAdd(testLoadKey(), 0, "B")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
		ZoneId:      testZoneId,
		SceneType:   2,
	})
	require.NoError(t, err)
	assert.Equal(t, "B", resp.NodeId, "non-mirror path must use GetBestNode")
}

// ---------------------------------------------------------------------------
// Mirror lifecycle — idle mirrors get destroyed faster than regular instances
// ---------------------------------------------------------------------------
//
// Mirrors re-initialize NPCs on every entry, so keeping an empty mirror
// around wastes memory. These tests pin the per-type idle timeouts and the
// mirror-flag bookkeeping that drives that behavior.

func TestCreateScene_Mirror_SetsMirrorFlag(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(77777)), "10")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  77777,
		MirrorConfigId: 9010,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp.ErrorCode)

	flag, _ := sc.Redis.Get(sceneMirrorFlagKey(resp.SceneId))
	assert.Equal(t, "1", flag, "mirror scene must carry the mirror flag")
}

func TestCreateScene_NonMirror_NoMirrorFlag(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
		ZoneId:      testZoneId,
		SceneType:   2,
	})
	require.NoError(t, err)

	flag, err := sc.Redis.Get(sceneMirrorFlagKey(resp.SceneId))
	// miniredis returns "" with no error for missing keys.
	assert.Empty(t, flag, "non-mirror must not set mirror flag (got %q, err=%v)", flag, err)
}

func TestCleanupIdleInstances_MirrorDestroyedFaster(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")
	logic := NewCreateSceneLogic(ctx, sc)

	// Seed the source scene so mirror co-location picks node "10".
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(88888)), "10")

	mirrorResp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  88888,
		MirrorConfigId: 9011,
	})
	require.NoError(t, err)
	instanceResp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2002,
		ZoneId:      testZoneId,
		SceneType:   2,
	})
	require.NoError(t, err)

	// Backdate both to 100s ago so they're idle.
	instKey := activeInstancesKey(testZoneId)
	oldScore := nowUnix() - 100
	sc.Redis.Zadd(instKey, oldScore, fmt.Sprintf("%d", mirrorResp.SceneId))
	sc.Redis.Zadd(instKey, oldScore, fmt.Sprintf("%d", instanceResp.SceneId))

	// Mirror timeout 30s < 100s idle -> destroy.
	// Instance timeout 300s > 100s idle -> keep.
	cleanupZoneIdleInstances(ctx, sc, testZoneId, 300, 30)

	members, _ := sc.Redis.ZrangeWithScores(instKey, 0, -1)
	keys := make([]string, 0, len(members))
	for _, p := range members {
		keys = append(keys, p.Key)
	}
	assert.NotContains(t, keys, fmt.Sprintf("%d", mirrorResp.SceneId), "idle mirror must be destroyed")
	assert.Contains(t, keys, fmt.Sprintf("%d", instanceResp.SceneId), "idle instance under its own cap must survive")

	// Mirror flag also cleaned up.
	flag, _ := sc.Redis.Get(sceneMirrorFlagKey(mirrorResp.SceneId))
	assert.Empty(t, flag, "mirror flag must be cleared on destroy")
}

func TestCleanupIdleInstances_MirrorWithPlayers_NotDestroyed(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(88889)), "10")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  88889,
		MirrorConfigId: 9012,
	})
	require.NoError(t, err)

	sc.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, resp.SceneId), "1")

	instKey := activeInstancesKey(testZoneId)
	sc.Redis.Zadd(instKey, nowUnix()-100, fmt.Sprintf("%d", resp.SceneId))

	cleanupZoneIdleInstances(ctx, sc, testZoneId, 300, 30)

	members, _ := sc.Redis.ZrangeWithScores(instKey, 0, -1)
	require.Len(t, members, 1)
	assert.Equal(t, fmt.Sprintf("%d", resp.SceneId), members[0].Key,
		"mirror with active player must NOT be destroyed, even past timeout")
}

func TestDestroyScene_ClearsMirrorFlag(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	ctx := context.Background()

	mr.ZAdd(testLoadKey(), 0, "10")
	sc.Redis.Set(fmt.Sprintf(SceneNodeKeyFmt, uint64(99990)), "10")

	createLogic := NewCreateSceneLogic(ctx, sc)
	resp, err := createLogic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId:    2001,
		ZoneId:         testZoneId,
		SceneType:      2,
		SourceSceneId:  99990,
		MirrorConfigId: 9013,
	})
	require.NoError(t, err)

	flag, _ := sc.Redis.Get(sceneMirrorFlagKey(resp.SceneId))
	require.Equal(t, "1", flag)

	destroyLogic := NewDestroySceneLogic(ctx, sc)
	_, err = destroyLogic.DestroyScene(&scene_manager.DestroySceneRequest{
		SceneId: resp.SceneId,
		ZoneId:  testZoneId,
	})
	require.NoError(t, err)

	flag, _ = sc.Redis.Get(sceneMirrorFlagKey(resp.SceneId))
	assert.Empty(t, flag, "explicit DestroyScene must also clear the mirror flag")
}

func TestResolveMirrorTimeout_FallsBackToInstance(t *testing.T) {
	sc, _ := newTestSvcCtxWithWorldScenes(t)

	sc.Config.MirrorIdleTimeoutSeconds = 0
	sc.Config.InstanceIdleTimeoutSeconds = 300
	assert.Equal(t, int64(300), resolveMirrorTimeout(sc), "0 must fall back to instance timeout")

	sc.Config.MirrorIdleTimeoutSeconds = 15
	assert.Equal(t, int64(15), resolveMirrorTimeout(sc), "positive value wins")
}
