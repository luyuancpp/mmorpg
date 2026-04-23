package logic

import (
	"context"
	"fmt"
	"sort"
	"strconv"

	"scene_manager/internal/constants"
	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// rebalanceReason describes why a channel is being moved. Used only for logs
// so operators can distinguish "the old node went away" from "we just found
// a better home".
type rebalanceReason string

const (
	reasonNodeGone   rebalanceReason = "node_gone"   // old node is dead / no longer world-hosting
	reasonBetterHome rebalanceReason = "better_home" // opportunistic; old node still alive
)

// channelMigration describes one proposed channel move. Exposed for
// observability and for tests that want to assert the planner's output
// without spinning up a real gRPC scene-node fleet.
type channelMigration struct {
	ConfId  uint64
	SceneId uint64
	OldNode string
	NewNode string
	Reason  rebalanceReason
}

// RebalanceWorldChannelsForZone redistributes empty world channels after a
// world-hosting node joins or leaves the pool. It is safe to call repeatedly:
// each invocation recomputes the expected (confId, sceneId) -> node mapping
// from scratch, then moves channels that diverge from the expected mapping
// subject to two safety rules:
//
//  1. Never migrate a channel with online players. Live migration would
//     require cross-node state transfer which this codebase does not have.
//     Hot channels stay put and migrate naturally as players drain.
//  2. Respect MaxRebalanceMigrationsPerTick so a big scale event (say, +4
//     pods) does not trigger N*channels simultaneous CreateScene/DestroyScene
//     RPCs and knock the cluster over.
//
// Channels currently mapped to a DEAD or non-world-hosting node are always
// prioritised (those mappings are already broken for new players); empty
// channels on still-live nodes are only moved if the budget allows.
func RebalanceWorldChannelsForZone(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) {
	urgent, opportunistic, budget := PlanWorldChannelRebalance(svcCtx, zoneId, confIds)
	if budget == 0 {
		return // explicitly disabled
	}
	if len(urgent) == 0 && len(opportunistic) == 0 {
		return
	}

	migrated, skipped := 0, 0
	run := func(list []channelMigration) {
		for _, c := range list {
			if migrated >= budget {
				return
			}
			if migrateWorldChannel(ctx, svcCtx, zoneId, c.ConfId, c.SceneId, c.OldNode, c.NewNode, c.Reason) {
				migrated++
			} else {
				skipped++
			}
		}
	}
	run(urgent)
	run(opportunistic)

	pending := len(urgent) + len(opportunistic) - migrated - skipped
	if migrated > 0 || len(urgent) > 0 {
		logx.Infof("[Rebalance] zone=%d urgent=%d opportunistic=%d migrated=%d skipped=%d pending=%d budget=%d",
			zoneId, len(urgent), len(opportunistic), migrated, skipped, pending, budget)
	}
}

// PlanWorldChannelRebalance returns the urgent and opportunistic migration
// lists that RebalanceWorldChannelsForZone would act on, plus the effective
// per-tick budget. Split out from the action path so tests can assert the
// planner in isolation (and so /metrics or an admin RPC can expose pending
// migration pressure).
//
// The returned slices are ordered for deterministic execution: urgent
// before opportunistic, within each list by (confId, sceneId).
func PlanWorldChannelRebalance(svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) (urgent, opportunistic []channelMigration, budget int) {
	if len(confIds) == 0 {
		return nil, nil, 0
	}
	budget = svcCtx.Config.MaxRebalanceMigrationsPerTick
	if budget == 0 {
		return nil, nil, 0
	}
	if budget < 0 {
		budget = 10
	}

	nodes := getNodesForPurpose(svcCtx, zoneId, constants.NodePurposeWorld)
	if len(nodes) == 0 {
		return nil, nil, budget
	}
	sort.Strings(nodes)
	liveSet := make(map[string]struct{}, len(nodes))
	for _, n := range nodes {
		liveSet[n] = struct{}{}
	}

	for _, confId := range confIds {
		members, err := svcCtx.Redis.Smembers(worldChannelsKey(zoneId, confId))
		if err != nil {
			continue
		}
		// Sort members so tests (and logs) see deterministic order within
		// a single confId. Smembers order is implementation-defined.
		sort.Strings(members)
		for _, m := range members {
			sceneId, _ := strconv.ParseUint(m, 10, 64)
			if sceneId == 0 {
				continue
			}
			curNode, _ := svcCtx.Redis.Get(fmt.Sprintf("scene:%d:node", sceneId))
			target := assignNodeByHash(sceneId, nodes)
			if curNode == target {
				continue
			}
			if _, alive := liveSet[curNode]; !alive {
				urgent = append(urgent, channelMigration{
					ConfId: confId, SceneId: sceneId,
					OldNode: curNode, NewNode: target, Reason: reasonNodeGone,
				})
				continue
			}
			if count := readInt64(svcCtx, fmt.Sprintf(InstancePlayerCountKey, sceneId)); count > 0 {
				continue
			}
			opportunistic = append(opportunistic, channelMigration{
				ConfId: confId, SceneId: sceneId,
				OldNode: curNode, NewNode: target, Reason: reasonBetterHome,
			})
		}
	}
	return urgent, opportunistic, budget
}

// migrateWorldChannel performs the create-swap-destroy dance for a single
// channel. Steps:
//
//  1. CreateScene on target node (C++ is idempotent by sceneId).
//  2. Rewrite scene:{sid}:node in Redis. Any EnterScene that races this
//     write sees either oldNode (still valid, scene still exists) or
//     newNode (also valid after step 1) — both serve the same sceneId.
//  3. Update per-node scene_count counters.
//  4. Best-effort DestroyScene on the old node. A failure here is logged
//     but not fatal: the old scene will be cleaned up on the next node
//     restart or by the lifecycle manager's stale-scene sweep.
//
// Returns true if steps 1-3 succeeded (i.e. the channel is now reachable
// via newNode). Step 4 failure does not flip the return.
func migrateWorldChannel(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32,
	confId, sceneId uint64, oldNode, newNode string, reason rebalanceReason) bool {

	if newNode == "" || newNode == oldNode {
		return false
	}

	if _, err := RequestNodeCreateScene(ctx, svcCtx, newNode, uint32(confId), sceneId); err != nil {
		logx.Errorf("[Rebalance] zone=%d conf=%d scene=%d: CreateScene on new node %s failed (%s): %v",
			zoneId, confId, sceneId, newNode, reason, err)
		return false
	}

	nodeKey := fmt.Sprintf("scene:%d:node", sceneId)
	if err := svcCtx.Redis.Set(nodeKey, newNode); err != nil {
		logx.Errorf("[Rebalance] zone=%d conf=%d scene=%d: failed to update scene->node mapping: %v",
			zoneId, confId, sceneId, err)
		// The ECS entity on newNode is effectively orphaned until the
		// next rebalance tick retries. Since CreateScene is idempotent
		// this is safe; we just wasted an RPC.
		return false
	}

	if oldNode != "" {
		if _, err := svcCtx.Redis.Decr(fmt.Sprintf(NodeSceneCountKey, oldNode)); err != nil {
			logx.Errorf("[Rebalance] failed to decrement scene_count on old node %s: %v", oldNode, err)
		}
	}
	if _, err := svcCtx.Redis.Incr(fmt.Sprintf(NodeSceneCountKey, newNode)); err != nil {
		logx.Errorf("[Rebalance] failed to increment scene_count on new node %s: %v", newNode, err)
	}

	// Best-effort cleanup of the old scene entity. Skip when the old node
	// is already gone — nothing to clean up — or when oldNode appears dead
	// to avoid long RPC timeouts during the urgent path.
	if oldNode != "" && IsNodeAlive(svcCtx, zoneId, oldNode) {
		if err := RequestNodeDestroyScene(ctx, svcCtx, oldNode, sceneId); err != nil {
			logx.Infof("[Rebalance] zone=%d conf=%d scene=%d: DestroyScene on old node %s failed (%s, ignored): %v",
				zoneId, confId, sceneId, oldNode, reason, err)
		}
	}

	logx.Infof("[Rebalance] zone=%d conf=%d scene=%d migrated %s -> %s (%s)",
		zoneId, confId, sceneId, oldNode, newNode, reason)
	return true
}
