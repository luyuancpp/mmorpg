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
	mr.ZAdd(NodeLoadKey, 50, "3")
	mr.ZAdd(NodeLoadKey, 5, "1")
	mr.ZAdd(NodeLoadKey, 20, "2")

	best, err := GetBestNode(ctx, sc)
	require.NoError(t, err)
	assert.Equal(t, "1", best)
}

func TestGetBestNode_NoNodes_ReturnsError(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "node-fallback")
	ctx := context.Background()

	// No nodes registered in the ZSet
	_, err := GetBestNode(ctx, sc)
	assert.Error(t, err)
}

func TestGetBestNode_TiedLoad(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "node-self")
	ctx := context.Background()

	// All nodes have same load (numeric IDs)
	mr.ZAdd(NodeLoadKey, 10, "10")
	mr.ZAdd(NodeLoadKey, 10, "20")
	mr.ZAdd(NodeLoadKey, 10, "30")

	best, err := GetBestNode(ctx, sc)
	require.NoError(t, err)
	// Should return one of the tied nodes (Redis ZRANGE returns lexicographic order on tie)
	assert.Contains(t, []string{"10", "20", "30"}, best)
}

func TestGetBestNode_SingleNode(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "node-self")
	ctx := context.Background()

	mr.ZAdd(NodeLoadKey, 99, "42")

	best, err := GetBestNode(ctx, sc)
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
	c.MainSceneConfIds = mainConfIds
	c.InstanceIdleTimeoutSeconds = 300
	c.InstanceCheckIntervalSeconds = 10

	return &svc.ServiceContext{
		Config: c,
		Redis:  rds,
	}, mr
}

func TestIsMainSceneConf(t *testing.T) {
	sc, _ := newTestSvcCtxWithMainScenes(t, []uint64{1001, 1002, 1003})

	assert.True(t, IsMainSceneConf(sc, 1001))
	assert.True(t, IsMainSceneConf(sc, 1003))
	assert.False(t, IsMainSceneConf(sc, 2001))
	assert.False(t, IsMainSceneConf(sc, 0))
}

func TestGetMainSceneId_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	id, _ := GetMainSceneId(ctx, sc, 9999)
	assert.Equal(t, uint64(0), id)
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
	mr.ZAdd(NodeLoadKey, 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	// First create.
	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)
	assert.NotEqual(t, uint64(0), resp1.SceneId)

	// Second create for same confId — should return same scene.
	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 1001,
	})
	require.NoError(t, err)
	assert.Equal(t, resp1.SceneId, resp2.SceneId, "main scene should be idempotent")
	assert.Equal(t, resp1.NodeId, resp2.NodeId)
}

func TestCreateScene_Instance_UniquIds(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	mr.ZAdd(NodeLoadKey, 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)

	resp1, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp1.ErrorCode)

	resp2, err := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
	})
	require.NoError(t, err)
	assert.Equal(t, uint32(0), resp2.ErrorCode)
	assert.NotEqual(t, resp1.SceneId, resp2.SceneId, "instances should get unique IDs")
}

func TestCreateScene_Instance_TrackedInActiveSet(t *testing.T) {
	sc, mr := newTestSvcCtxWithMainScenes(t, nil)
	ctx := context.Background()

	mr.ZAdd(NodeLoadKey, 0, "10")

	logic := NewCreateSceneLogic(ctx, sc)
	resp, _ := logic.CreateScene(&scene_manager.CreateSceneRequest{
		SceneConfId: 2001,
	})

	// Should be in the active instances sorted set.
	instKey := activeInstancesKey(sc.Config.ZoneID)
	members, err := sc.Redis.ZrangeWithScores(instKey, 0, -1)
	require.NoError(t, err)
	assert.Len(t, members, 1)
	assert.Equal(t, fmt.Sprintf("%d", resp.SceneId), members[0].Key)
}
