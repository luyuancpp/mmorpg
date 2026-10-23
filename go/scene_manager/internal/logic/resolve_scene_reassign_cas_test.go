package logic

// 回归:resolveScene 的死节点自愈(现 reassignSceneOffDeadNode)曾无条件 SET scene:{id}:node,不经
// reassignSceneNode 的 CAS。别的写者 —— 另一个副本上的 EnterScene,以及世界频道的 rebalance / init /
// 懒改派 —— 在它「读到属主是死节点」与「写入」之间先改派了同一个场景时,它照样覆盖成自己挑的节点并在
// 那里 CreateScene:先到者那份实体成了孤儿,同一场景的玩家被派到两个节点上。SET 也不动反向索引与
// scene_count:新属主死时收尾快照里没有这个场景(实例泄漏),场景销毁时又从新属主的计数里减掉一个
// 从没加过的数。
//
// 交错用确定的顺序复现(与 reassign_cas_test.go 单元层同一手法):先让「另一写者」用 reassignSceneNode
// 提交,再拿陈旧的属主观察(dead)调用 reassignSceneOffDeadNode —— 等价于 resolveScene 读完属主、提交
// 之前被另一写者插队。resolveScene 读属主与提交之间没有可注入的缝,所以这两个用例不经它的入口;
// 不需要交错的用例都从 resolveScene 进。
//
// 修复前红的取法:把 reassignSceneOffDeadNode 里的
//
//	if !reassignSceneNode(l.svcCtx, zoneId, sceneId, deadNode, newNodeId) { return l.adoptOwnerAfterLostReassign(...) }
//
// 临时换回修复前的 `l.svcCtx.Redis.Set(sceneNodeKey(sceneId), newNodeId)`(其余不动)再跑本文件:
// AnotherWriterFirst / ScriptFailure / MovesIndexAndSceneCount / AgonesRoom 四个用例红。
// SameTargetTopsUpCreateScene 钉的是落空之后的补建,不是 CAS 本身:删掉 adoptOwnerAfterLostReassign 里
// owner == triedNode 的那次 CreateScene 才红。BarrierPending 是挪动屏障判定后的行为守护,修复前后都绿。

import (
	"context"
	"errors"
	"strconv"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/require"

	"scene_manager/internal/constants"
	"scene_manager/internal/svc"
)

const resolveCASZone uint32 = 1

// 节点号不与别的用例重名,理由同 reassign_cas_test.go 的 casNode* 常量:再入屏障会读进程内的推迟摘除
// 队列,别的用例在 zone 1 留下的常用节点推迟态会把这里的改派静默挡下。
const (
	resolveNodeDead  = "8910"
	resolveNodeFirst = "8920" // 先提交的另一写者挑中的节点
	resolveNodeLate  = "8930" // 本次请求(拿着陈旧判断)挑中的节点
)

// 测试里 World 表为空,IsWorldConf 为 false,按实例用途选节点 —— 实例场景的改派者只有 EnterScene 这一类。
// newIntegrationSvcCtx 不开 StrictNodeTypeSeparation,世界类型的桩节点也在候选里。
const resolveCASConfID uint64 = 4791

type resolveDeadOwnerCase struct {
	sc          *svc.ServiceContext
	mr          *miniredis.Miniredis
	first, late *sceneNodeHarness
	logic       *EnterSceneLogic
	sceneID     uint64
}

// newResolveDeadOwnerCase 摆出现场:sceneID 归 dead 所有(属主映射、反向索引、scene_count=1 齐全),
// dead 已摘出负载集且没有 death_at(屏障放行);first、late 两个活节点,负载分让本次请求挑 late。
func newResolveDeadOwnerCase(t *testing.T, sceneID uint64) *resolveDeadOwnerCase {
	t.Helper()
	clearKnownNodesForTest()
	ResetNodeConnCacheForTest()
	t.Cleanup(clearKnownNodesForTest)

	sc, mr := newIntegrationSvcCtx(t)
	first := startFakeSceneNode(t, resolveNodeFirst)
	late := startFakeSceneNode(t, resolveNodeLate)
	installBufconnDialer(t, first, late)
	registerWorldNodeFromHarness(t, sc, resolveCASZone, first)
	registerWorldNodeFromHarness(t, sc, resolveCASZone, late)
	setResolveCASLoadScores(t, mr)

	require.NoError(t, mr.Set(sceneZoneKey(sceneID), strconv.FormatUint(uint64(resolveCASZone), 10)))
	seedSceneOwnedBy(t, mr, resolveCASZone, sceneID, resolveNodeDead)

	// 前提:dead 已判死、屏障放行,本次请求挑的是 late。
	require.False(t, IsNodeAlive(sc, resolveCASZone, resolveNodeDead))
	allowed, _ := CanReclaimDeadNode(sc, resolveCASZone, resolveNodeDead)
	require.True(t, allowed)
	best, err := GetBestNodeForPurpose(context.Background(), sc, resolveCASZone, constants.NodePurposeInstance)
	require.NoError(t, err)
	require.Equal(t, resolveNodeLate, best)

	return &resolveDeadOwnerCase{sc: sc, mr: mr, first: first, late: late,
		logic: NewEnterSceneLogic(context.Background(), sc), sceneID: sceneID}
}

// setResolveCASLoadScores 把两个活节点的负载分拉开:GetBestNodeForPurpose 挑 late,先提交的另一写者挑 first。
func setResolveCASLoadScores(t *testing.T, mr *miniredis.Miniredis) {
	t.Helper()
	_, err := mr.ZAdd(nodeLoadKey(resolveCASZone), 5, resolveNodeFirst)
	require.NoError(t, err)
	_, err = mr.ZAdd(nodeLoadKey(resolveCASZone), 0, resolveNodeLate)
	require.NoError(t, err)
}

// requireOwnedOnlyBy 断言场景的记账全部指向 owner,且只改了一次:属主映射、反向索引、两侧 scene_count;
// bystander 是没拿到归属的那个活节点,不得记账。
func (c *resolveDeadOwnerCase) requireOwnedOnlyBy(t *testing.T, owner, bystander string) {
	t.Helper()
	got, err := c.mr.Get(sceneNodeKey(c.sceneID))
	require.NoError(t, err)
	require.Equal(t, owner, got)
	requireSceneIndexedOnlyOn(t, c.sc, resolveCASZone, c.sceneID, owner, resolveNodeDead, bystander)
	require.EqualValues(t, 1, readInt64(c.sc, nodeSceneCountKey(resolveCASZone, owner)), "属主的 scene_count 恰好加一次")
	require.EqualValues(t, 0, readInt64(c.sc, nodeSceneCountKey(resolveCASZone, resolveNodeDead)), "老属主的 scene_count 恰好减一次")
	require.EqualValues(t, 0, readInt64(c.sc, nodeSceneCountKey(resolveCASZone, bystander)), "没拿到归属的节点不得记账")
}

// 另一写者在本次请求「读到属主是 dead」之后先把场景改到了 first。本次的 CAS 落空,重读属主后直接用 first:
// 不覆盖、不在自己挑的 late 上建实体,也不替 first 补建(那是先提交者的职责)。
// 修复前:无条件覆盖成 late 并在 late 上 CreateScene,first 上的实体成了孤儿。
func TestResolveScene_DeadOwner_AnotherWriterFirst_AdoptsItsOwner(t *testing.T) {
	c := newResolveDeadOwnerCase(t, 89001)
	require.True(t, reassignSceneNode(c.sc, resolveCASZone, c.sceneID, resolveNodeDead, resolveNodeFirst))

	sid, nid, err := c.logic.reassignSceneOffDeadNode(c.sceneID, resolveCASConfID, resolveCASZone, resolveNodeDead)
	require.NoError(t, err, "落空后重读到活的新属主,本次进场照常放行")
	require.Equal(t, c.sceneID, sid)
	require.Equal(t, resolveNodeFirst, nid, "必须用先提交者的节点,而不是自己挑的 late")

	c.requireOwnedOnlyBy(t, resolveNodeFirst, resolveNodeLate)
	require.EqualValues(t, 0, c.late.fake.createCalls.Load(), "落空的一方不得在自己挑的节点上建实体")
	require.EqualValues(t, 0, c.first.fake.createCalls.Load(), "first 上的实体由先提交的写者负责,本次不补发")
}

// 先提交的写者恰好挑了同一个节点(并发的另一个 EnterScene 挑中同一个最低负载节点;Redis 报错而本次脚本
// 其实已执行,在本次看来也是这个现场)。属主就是 late:补发一次 CreateScene(按 scene_id 幂等),计数只记一次。
func TestResolveScene_DeadOwner_SameTargetTopsUpCreateScene(t *testing.T) {
	c := newResolveDeadOwnerCase(t, 89002)
	require.True(t, reassignSceneNode(c.sc, resolveCASZone, c.sceneID, resolveNodeDead, resolveNodeLate))

	sid, nid, err := c.logic.reassignSceneOffDeadNode(c.sceneID, resolveCASConfID, resolveCASZone, resolveNodeDead)
	require.NoError(t, err)
	require.Equal(t, c.sceneID, sid)
	require.Equal(t, resolveNodeLate, nid)

	c.requireOwnedOnlyBy(t, resolveNodeLate, resolveNodeFirst)
	require.EqualValues(t, 1, c.late.fake.createCalls.Load(), "属主就是本次挑中的节点时补发一次 CreateScene,不能指望别人建")
	require.Equal(t, c.sceneID, c.late.fake.lastCreateScene.Load())
	require.Equal(t, resolveCASConfID, c.late.fake.lastCreateConf.Load())
	require.EqualValues(t, 0, c.first.fake.createCalls.Load())
}

// late 的计数键损坏,脚本在 INCR 处报错:CAS 没被抢,是没跑成,归属一个字节都没改。本次请求拒绝(不是
// 屏障拒绝),哪个节点都不建实体。修复前:SET 不经脚本,照样改成 late 并 CreateScene。
func TestResolveScene_DeadOwner_ScriptFailureChangesNothing(t *testing.T) {
	c := newResolveDeadOwnerCase(t, 89003)
	require.NoError(t, c.mr.Set(nodeSceneCountKey(resolveCASZone, resolveNodeLate), "not-a-number"))

	_, _, err := c.logic.resolveScene(c.sceneID, resolveCASConfID, resolveCASZone)
	require.Error(t, err)
	require.False(t, errors.Is(err, ErrReentryBarrierPending), "屏障早已放行,这不是屏障拒绝")

	owner, err := c.mr.Get(sceneNodeKey(c.sceneID))
	require.NoError(t, err)
	require.Equal(t, resolveNodeDead, owner, "脚本报错时归属一个字节都不改")
	requireSceneIndexedOnlyOn(t, c.sc, resolveCASZone, c.sceneID, resolveNodeDead, resolveNodeFirst, resolveNodeLate)
	require.EqualValues(t, 1, readInt64(c.sc, nodeSceneCountKey(resolveCASZone, resolveNodeDead)))
	require.EqualValues(t, 0, c.late.fake.createCalls.Load(), "归属没改到 late,不得在 late 上建实体")
	require.EqualValues(t, 0, c.first.fake.createCalls.Load())
}

// 没有竞争写者时从 resolveScene 入口走完整次改派:属主映射、两侧反向索引与 scene_count 同批改到 late,
// 记账口径与 createscenelogic.go allocateScene 一致;late 上建一次实体。
// 修复前:只 SET 属主映射,late 的反向索引与 scene_count 都没有,dead 的还挂着这个场景。
func TestResolveScene_DeadOwner_MovesIndexAndSceneCount(t *testing.T) {
	c := newResolveDeadOwnerCase(t, 89004)

	sid, nid, err := c.logic.resolveScene(c.sceneID, resolveCASConfID, resolveCASZone)
	require.NoError(t, err)
	require.Equal(t, c.sceneID, sid)
	require.Equal(t, resolveNodeLate, nid)

	c.requireOwnedOnlyBy(t, resolveNodeLate, resolveNodeFirst)
	require.EqualValues(t, 1, c.late.fake.createCalls.Load())
	require.Equal(t, c.sceneID, c.late.fake.lastCreateScene.Load())
	require.Equal(t, resolveCASConfID, c.late.fake.lastCreateConf.Load())
	require.EqualValues(t, 0, c.first.fake.createCalls.Load())
}

// 屏障判定挪进 reassignSceneOffDeadNode 之后行为不变:老属主刚判死时回可重试的 ErrReentryBarrierPending,
// 一个字节都不改,哪个节点都不建实体。
func TestResolveScene_DeadOwner_BarrierPendingChangesNothing(t *testing.T) {
	c := newResolveDeadOwnerCase(t, 89005)
	require.NoError(t, markNodeDeath(c.sc, resolveCASZone, resolveNodeDead))

	_, _, err := c.logic.resolveScene(c.sceneID, resolveCASConfID, resolveCASZone)
	require.ErrorIs(t, err, ErrReentryBarrierPending)

	owner, err := c.mr.Get(sceneNodeKey(c.sceneID))
	require.NoError(t, err)
	require.Equal(t, resolveNodeDead, owner)
	requireSceneIndexedOnlyOn(t, c.sc, resolveCASZone, c.sceneID, resolveNodeDead, resolveNodeFirst, resolveNodeLate)
	require.EqualValues(t, 1, readInt64(c.sc, nodeSceneCountKey(resolveCASZone, resolveNodeDead)))
	require.EqualValues(t, 0, c.late.fake.createCalls.Load())
	require.EqualValues(t, 0, c.first.fake.createCalls.Load())
}

// Agones 名额只跟着提交成功的改派走,与 initWorldScenesForZone 的死节点改派同一口径:改派成功时在新节点
// 占一份、还掉死节点 GameServer 上的那份;CAS 落空时一个名额都不动(改派成功的写者自己负责转移,
// 这里再转一次就是在新节点上多占一份)。修复前:不转移名额,新节点不记账;落空也不存在。
//
// Agones 的节点 → Pod 映射要 knownNodes 里有独立的 Pod IP,所以本用例不用 bufconn 桩节点,
// CreateScene 落到 TestMain 的共享桩上。
func TestResolveScene_DeadOwner_AgonesRoomMovesOnlyWithCommittedReassign(t *testing.T) {
	clearKnownNodesForTest()
	ResetNodeConnCacheForTest()
	t.Cleanup(clearKnownNodesForTest)

	sc, mr := newIntegrationSvcCtx(t)
	installDefaultFakeSceneNode(t)
	alloc := newFakeAllocator(
		&fakeGameServer{name: "gs-dead", podIP: "10.0.89.10", state: "Allocated", count: 2, capacity: 4},
		&fakeGameServer{name: "gs-first", podIP: "10.0.89.20", state: "Ready", capacity: 4},
		&fakeGameServer{name: "gs-late", podIP: "10.0.89.30", state: "Ready", capacity: 4},
	)
	withAgones(t, sc, alloc, true)
	bindPodIP(t, resolveNodeFirst, "10.0.89.20", resolveCASZone, constants.SceneNodeTypeInstance)
	bindPodIP(t, resolveNodeLate, "10.0.89.30", resolveCASZone, constants.SceneNodeTypeInstance)
	setResolveCASLoadScores(t, mr)

	const lostScene, wonScene uint64 = 89006, 89007
	for _, id := range []uint64{lostScene, wonScene} {
		require.NoError(t, mr.Set(sceneZoneKey(id), strconv.FormatUint(uint64(resolveCASZone), 10)))
		seedSceneOwnedBy(t, mr, resolveCASZone, id, resolveNodeDead)
		require.NoError(t, mr.Set(sceneAgonesGsKey(id), "gs-dead"))
	}
	logic := NewEnterSceneLogic(context.Background(), sc)

	// 落空:另一写者先把 lostScene 改到 first。
	require.True(t, reassignSceneNode(sc, resolveCASZone, lostScene, resolveNodeDead, resolveNodeFirst))
	_, nid, err := logic.reassignSceneOffDeadNode(lostScene, resolveCASConfID, resolveCASZone, resolveNodeDead)
	require.NoError(t, err)
	require.Equal(t, resolveNodeFirst, nid)
	_, release, acquireOnGs := alloc.stats()
	require.Zero(t, acquireOnGs, "CAS 落空不得在任何节点上占名额")
	require.Zero(t, release, "CAS 落空不得归还死节点的名额")
	gs, err := mr.Get(sceneAgonesGsKey(lostScene))
	require.NoError(t, err)
	require.Equal(t, "gs-dead", gs)

	// 改派成功:名额跟到 late,死节点 GameServer 上只还 wonScene 那一份。
	_, nid, err = logic.resolveScene(wonScene, resolveCASConfID, resolveCASZone)
	require.NoError(t, err)
	require.Equal(t, resolveNodeLate, nid)
	require.EqualValues(t, 1, alloc.roomsOf("gs-late"), "新节点必须记账,否则 rooms Counter 少算")
	require.EqualValues(t, 1, alloc.roomsOf("gs-dead"), "只还 wonScene 那一份")
	require.EqualValues(t, 0, alloc.roomsOf("gs-first"))
	gs, err = mr.Get(sceneAgonesGsKey(wonScene))
	require.NoError(t, err)
	require.Equal(t, "gs-late", gs)
}
