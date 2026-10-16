package kafka

// 消费组交接闸(group_handoff.go)的单测:用内存里的 fakeCluster 代表 Kafka 的组状态与已提交位点,
// fakeInspector 读它;不连真实 Kafka。等待用注入的 sleep / now 推进,不依赖真实墙钟。

import (
	"bytes"
	"context"
	db_config "db/internal/config"
	"db/internal/metrics"
	"encoding/binary"
	"errors"
	"fmt"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/IBM/sarama"
	"github.com/alicebob/miniredis/v2"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

const (
	testZoneGroup    = "db_rpc_consumer_group_z101"
	testLegacyGroup  = "db_rpc_consumer_group"
	testHandoffTopic = "db_task_zone_101"
)

// ---------------------------------------------------------------------------
// fake Kafka
// ---------------------------------------------------------------------------

type fakeGroup struct {
	state     string
	held      []int32         // 成员分配里属于本 topic 的分区
	committed map[int32]int64 // 本 topic 上的已提交位点;不在 map 里 = 没有位点
}

// fakeCluster 是若干消费组的内存状态,外加按组注入的查询错误与调用计数。不存在的组按 Dead、无位点回答。
type fakeCluster struct {
	mu             sync.Mutex
	groups         map[string]*fakeGroup
	describeErr    map[string]error
	committedErr   map[string]error
	describeCalls  map[string]int
	committedCalls map[string]int
	committedAsked map[string][][]int32
}

func newFakeCluster() *fakeCluster {
	c := &fakeCluster{groups: map[string]*fakeGroup{}, describeErr: map[string]error{}, committedErr: map[string]error{}}
	c.resetCalls()
	return c
}

func (c *fakeCluster) setGroup(name, state string, held []int32, committed map[int32]int64) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.groups[name] = &fakeGroup{state: state, held: held, committed: committed}
}

func (c *fakeCluster) setState(name, state string, held []int32) {
	c.mu.Lock()
	defer c.mu.Unlock()
	g := c.groups[name]
	if g == nil {
		g = &fakeGroup{}
		c.groups[name] = g
	}
	g.state, g.held = state, held
}

func (c *fakeCluster) setCommitted(name string, committed map[int32]int64) {
	c.mu.Lock()
	defer c.mu.Unlock()
	g := c.groups[name]
	if g == nil {
		g = &fakeGroup{state: groupStateEmpty}
		c.groups[name] = g
	}
	g.committed = committed
}

func (c *fakeCluster) failDescribe(name string, err error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.describeErr[name] = err
}

func (c *fakeCluster) failCommitted(name string, err error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.committedErr[name] = err
}

func (c *fakeCluster) resetCalls() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.describeCalls = map[string]int{}
	c.committedCalls = map[string]int{}
	c.committedAsked = map[string][][]int32{}
}

func (c *fakeCluster) totalCalls() int {
	c.mu.Lock()
	defer c.mu.Unlock()
	n := 0
	for _, v := range c.describeCalls {
		n += v
	}
	for _, v := range c.committedCalls {
		n += v
	}
	return n
}

func (c *fakeCluster) committedCallsOf(name string) int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.committedCalls[name]
}

func (c *fakeCluster) committedAskedOf(name string) [][]int32 {
	c.mu.Lock()
	defer c.mu.Unlock()
	return append([][]int32(nil), c.committedAsked[name]...)
}

func (c *fakeCluster) describe(group string) (peerGroupView, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.describeCalls[group]++
	if err := c.describeErr[group]; err != nil {
		return peerGroupView{}, err
	}
	held := map[int32]struct{}{}
	g := c.groups[group]
	if g == nil {
		return peerGroupView{State: groupStateDead, Held: held}, nil
	}
	for _, p := range g.held {
		held[p] = struct{}{}
	}
	return peerGroupView{State: g.state, Held: held}, nil
}

func (c *fakeCluster) committed(group string, partitions []int32) (map[int32]int64, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.committedCalls[group]++
	c.committedAsked[group] = append(c.committedAsked[group], append([]int32(nil), partitions...))
	if err := c.committedErr[group]; err != nil {
		return nil, err
	}
	out := make(map[int32]int64, len(partitions))
	g := c.groups[group]
	for _, p := range partitions {
		out[p] = -1
		if g != nil {
			if o, ok := g.committed[p]; ok {
				out[p] = o
			}
		}
	}
	return out, nil
}

// fakeInspector 读 fakeCluster;err 非 nil 时每个调用都返回它(模拟这条 admin 连接坏了)。
type fakeInspector struct {
	cluster *fakeCluster
	err     error
	mu      sync.Mutex
	closed  int
}

func (f *fakeInspector) DescribeGroup(group, _ string) (peerGroupView, error) {
	if f.err != nil {
		return peerGroupView{}, f.err
	}
	return f.cluster.describe(group)
}

func (f *fakeInspector) CommittedOffsets(group, _ string, partitions []int32) (map[int32]int64, error) {
	if f.err != nil {
		return nil, f.err
	}
	return f.cluster.committed(group, partitions)
}

func (f *fakeInspector) Close() error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.closed++
	return nil
}

func (f *fakeInspector) closeCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.closed
}

// fakeInspectorFactory 是 groupHandoff 的 inspector 工厂:第 i 次建出的 inspector 带 broken[i] 错误(越界 = 健康)。
type fakeInspectorFactory struct {
	mu       sync.Mutex
	cluster  *fakeCluster
	broken   []error
	buildErr error
	built    []*fakeInspector
}

func (f *fakeInspectorFactory) build() (groupInspector, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.buildErr != nil {
		return nil, f.buildErr
	}
	insp := &fakeInspector{cluster: f.cluster}
	if i := len(f.built); i < len(f.broken) {
		insp.err = f.broken[i]
	}
	f.built = append(f.built, insp)
	return insp, nil
}

func (f *fakeInspectorFactory) builds() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.built)
}

func (f *fakeInspectorFactory) inspector(i int) *fakeInspector {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.built[i]
}

// fakeClaim 实现 sarama.ConsumerGroupClaim。
type fakeClaim struct {
	topic     string
	partition int32
	initial   int64
	hwm       int64
	messages  chan *sarama.ConsumerMessage
}

func (c *fakeClaim) Topic() string                            { return c.topic }
func (c *fakeClaim) Partition() int32                         { return c.partition }
func (c *fakeClaim) InitialOffset() int64                     { return c.initial }
func (c *fakeClaim) HighWaterMarkOffset() int64               { return c.hwm }
func (c *fakeClaim) Messages() <-chan *sarama.ConsumerMessage { return c.messages }

// encodeAssignment 按 Kafka ConsumerProtocolAssignment 的大端格式手工编码成员分配:
// int16 版本号、int32 topic 数、(int16 长度 + topic 名、int32 分区数 + 各分区)×N、int32 -1 表示 nil userData。
func encodeAssignment(t *testing.T, topics map[string][]int32) []byte {
	t.Helper()
	var buf bytes.Buffer
	write := func(v any) { require.NoError(t, binary.Write(&buf, binary.BigEndian, v)) }
	write(int16(0))
	write(int32(len(topics)))
	for name, partitions := range topics {
		write(int16(len(name)))
		buf.WriteString(name)
		write(int32(len(partitions)))
		for _, p := range partitions {
			write(p)
		}
	}
	write(int32(-1))
	return buf.Bytes()
}

// ---------------------------------------------------------------------------
// harness
// ---------------------------------------------------------------------------

type handoffHarness struct {
	t       *testing.T
	cluster *fakeCluster
	factory *fakeInspectorFactory
	h       *groupHandoff

	mu      sync.Mutex
	clock   time.Time
	sleeps  int
	onSleep func(n int) // 第 n 次 sleep 之后调用(可改 cluster 推进剧情)
}

func newHandoffHarness(t *testing.T, group, peer string, partitions int32) *handoffHarness {
	return newHandoffHarnessOn(t, newFakeCluster(), group, peer, partitions)
}

// newHandoffHarnessOn 让两个交接闸(正向 + 反向)共用同一个 fakeCluster。
func newHandoffHarnessOn(t *testing.T, cluster *fakeCluster, group, peer string, partitions int32) *handoffHarness {
	t.Helper()
	factory := &fakeInspectorFactory{cluster: cluster}
	h, err := newGroupHandoff(factory.build, group, peer, testHandoffTopic, partitions)
	require.NoError(t, err)
	hh := &handoffHarness{t: t, cluster: cluster, factory: factory, h: h,
		clock: time.Date(2026, 10, 15, 0, 0, 0, 0, time.UTC)}
	h.now = hh.now
	h.sleep = hh.sleep
	return hh
}

func (hh *handoffHarness) now() time.Time {
	hh.mu.Lock()
	defer hh.mu.Unlock()
	return hh.clock
}

// sleep 推进假时钟并调用 onSleep;闸 100 次都没放行时判失败并退出等待,防止实现出错时用例挂死。
func (hh *handoffHarness) sleep(ctx context.Context, d time.Duration) bool {
	if ctx.Err() != nil {
		return false
	}
	hh.mu.Lock()
	hh.sleeps++
	n := hh.sleeps
	hh.clock = hh.clock.Add(d)
	hook := hh.onSleep
	hh.mu.Unlock()
	if n > 100 {
		hh.t.Errorf("group handoff gate did not release after %d polls", n)
		return false
	}
	if hook != nil {
		hook(n)
	}
	return ctx.Err() == nil
}

func (hh *handoffHarness) sleepCount() int {
	hh.mu.Lock()
	defer hh.mu.Unlock()
	return hh.sleeps
}

func (hh *handoffHarness) released() bool {
	hh.h.mu.Lock()
	defer hh.h.mu.Unlock()
	return hh.h.released
}

func (hh *handoffHarness) settled() map[int32]struct{} {
	hh.h.mu.Lock()
	defer hh.h.mu.Unlock()
	out := map[int32]struct{}{}
	for p := range hh.h.settled {
		out[p] = struct{}{}
	}
	return out
}

// forceRelease 直接置放行(settled 为空),用来单独测 Setup 播种,不经过 await。
func (hh *handoffHarness) forceRelease() {
	hh.h.mu.Lock()
	defer hh.h.mu.Unlock()
	hh.h.released = true
	hh.h.settled = nil
}

// handoffCounter 从默认注册表读一个交接闸 counter 的当前值(label 为空 = 无 label 的 counter)。
func handoffCounter(t *testing.T, name, label, value string) float64 {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, family := range families {
		if family.GetName() != name {
			continue
		}
		for _, m := range family.GetMetric() {
			if label == "" {
				return m.GetCounter().GetValue()
			}
			for _, l := range m.GetLabel() {
				if l.GetName() == label && l.GetValue() == value {
					return m.GetCounter().GetValue()
				}
			}
		}
	}
	return 0
}

func gateCount(t *testing.T, outcome string) float64 {
	return handoffCounter(t, "db_consumer_group_handoff_gate_total", "outcome", outcome)
}

func seedCount(t *testing.T, result string) float64 {
	return handoffCounter(t, "db_consumer_group_handoff_total", "result", result)
}

func conflictCount(t *testing.T) float64 {
	return handoffCounter(t, "db_consumer_group_handoff_conflict_total", "", "")
}

func set(ps ...int32) map[int32]struct{} {
	out := map[int32]struct{}{}
	for _, p := range ps {
		out[p] = struct{}{}
	}
	return out
}

// ---------------------------------------------------------------------------
// 闸前判定(await / evaluate)
// ---------------------------------------------------------------------------

// T1a:对方 Stable 且持有本 topic 时一直等;对方释放(Stable 但不再持有)后放行,fake sleep 次数等于等待轮数。
func TestGroupHandoffGate_BlocksWhilePeerHoldsTopicThenReleases(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
	hh.cluster.setGroup(testLegacyGroup, groupStateStable, []int32{0, 1}, map[int32]int64{0: 100, 1: 200})
	hh.onSleep = func(n int) {
		if n == 2 {
			// 本 zone 最后一个旧 Pod 退出:legacy 组仍 Stable(别的 zone 的成员还在),但不再持有本 topic。
			hh.cluster.setState(testLegacyGroup, groupStateStable, nil)
		}
	}
	holdingBefore := gateCount(t, metrics.HandoffGateWaitPeerHolding)
	releasedBefore := gateCount(t, metrics.HandoffGatePeerReleased)

	require.NoError(t, hh.h.await(context.Background()))
	assert.Equal(t, 2, hh.sleepCount(), "two polls blocked on the holding peer")
	assert.True(t, hh.released())
	assert.Equal(t, holdingBefore+2, gateCount(t, metrics.HandoffGateWaitPeerHolding))
	assert.Equal(t, releasedBefore+1, gateCount(t, metrics.HandoffGatePeerReleased))
}

// T2:对方在再平衡中(Held 为空但不可信)且有位点时等待。
func TestGroupHandoffGate_WaitsWhilePeerRebalancingWithOffsets(t *testing.T) {
	for _, state := range []string{groupStatePreparingRebalance, groupStateCompletingRebalance} {
		t.Run(state, func(t *testing.T) {
			hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
			hh.cluster.setGroup(testLegacyGroup, state, nil, map[int32]int64{0: 5})
			d := hh.h.evaluate()
			assert.False(t, d.pass)
			assert.Equal(t, metrics.HandoffGateWaitPeerRebalancing, d.outcome)
		})
	}
}

// 修订稿 R12:状态为空串或未知时等待(只凭正面证据放行),不当成已释放。
func TestGroupHandoffGate_PeerStateUnknownWaits(t *testing.T) {
	for _, state := range []string{"", "Bogus"} {
		t.Run(fmt.Sprintf("state=%q", state), func(t *testing.T) {
			hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
			hh.cluster.setGroup(testLegacyGroup, state, nil, map[int32]int64{0: 5})
			d := hh.h.evaluate()
			assert.False(t, d.pass)
			assert.Equal(t, metrics.HandoffGateWaitPeerStateUnknown, d.outcome)
			assert.Contains(t, d.detail, fmt.Sprintf("%q", state))
		})
	}
}

// R7(B2):两组在同一分区都有位点且对方更大时拒绝放行;反过来(本组更大)放行。
func TestGroupHandoffGate_RefusesWhenPeerAheadOfOwn(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 10})
	hh.cluster.setGroup(testLegacyGroup, groupStateStable, nil, map[int32]int64{0: 20})
	d := hh.h.evaluate()
	assert.False(t, d.pass)
	assert.Equal(t, metrics.HandoffGateWaitOwnBehindPeer, d.outcome)
	assert.Contains(t, d.detail, "partition=0 own=10 peer=20")

	hh.cluster.setCommitted(testZoneGroup, map[int32]int64{0: 20})
	hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 10})
	d = hh.h.evaluate()
	assert.True(t, d.pass, "own ahead of the peer is the normal post-handoff state")
	assert.Equal(t, metrics.HandoffGatePeerReleased, d.outcome)
}

// T6(修订稿 R5):对方查询失败时一律等待,不管本组是否完整(不再有 peer_unreachable_own_complete);
// 本组查询失败或 inspector 建不出来时等待 wait_own_unavailable。
func TestGroupHandoffGate_AdminErrors(t *testing.T) {
	t.Run("对方位点查询失败、本组完整也等待", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
		hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 5, 1: 6})
		hh.cluster.failCommitted(testLegacyGroup, errors.New("coordinator not available"))
		d := hh.h.evaluate()
		assert.False(t, d.pass)
		assert.Equal(t, metrics.HandoffGateWaitPeerUnavailable, d.outcome)
	})
	t.Run("对方 DescribeGroups 失败", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
		hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 5})
		hh.cluster.failDescribe(testLegacyGroup, errors.New("broker down"))
		d := hh.h.evaluate()
		assert.False(t, d.pass)
		assert.Equal(t, metrics.HandoffGateWaitPeerUnavailable, d.outcome)
	})
	t.Run("本组位点查询失败", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
		hh.cluster.failCommitted(testZoneGroup, errors.New("not coordinator"))
		d := hh.h.evaluate()
		assert.False(t, d.pass)
		assert.Equal(t, metrics.HandoffGateWaitOwnUnavailable, d.outcome)
	})
	t.Run("inspector 建不出来", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
		hh.factory.buildErr = errors.New("dial tcp: connection refused")
		d := hh.h.evaluate()
		assert.False(t, d.pass)
		assert.Equal(t, metrics.HandoffGateWaitOwnUnavailable, d.outcome)
		assert.Contains(t, d.detail, "group handoff inspector")
	})
}

// F3 快路径:对方在本 topic 上没有任何位点、也不持有分区时放行,不管它处于什么状态(全新 topic 首启)。
func TestGroupHandoffGate_FastPathWhenPeerHasNoOffsets(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 3)
	hh.cluster.setGroup(testLegacyGroup, groupStatePreparingRebalance, nil, nil)
	d := hh.h.evaluate()
	assert.True(t, d.pass)
	assert.Equal(t, metrics.HandoffGateNoPeerOffsets, d.outcome)
}

// 修订稿 R2:inspector 出错就丢弃,下次查询用工厂重建(新 client 重新 FindCoordinator)。
func TestGroupHandoffInspector_RebuiltAfterError(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.factory.broken = []error{fmt.Errorf("fetch offsets: %w", sarama.ErrNotCoordinatorForConsumer)}
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 7})

	d := hh.h.evaluate()
	assert.False(t, d.pass)
	assert.Equal(t, metrics.HandoffGateWaitOwnUnavailable, d.outcome)
	require.Equal(t, 1, hh.factory.builds())
	assert.Equal(t, 1, hh.factory.inspector(0).closeCount(), "the failed inspector must be closed")

	d = hh.h.evaluate()
	assert.True(t, d.pass, "the rebuilt inspector must succeed")
	assert.Equal(t, 2, hh.factory.builds(), "the factory must be called again after the failure")
	assert.Equal(t, 0, hh.factory.inspector(1).closeCount(), "a healthy inspector is kept")
}

func TestGroupHandoffAwait_ReturnsOnContextCancel(t *testing.T) {
	t.Run("等待中取消", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
		hh.cluster.setGroup(testLegacyGroup, groupStateStable, []int32{0}, map[int32]int64{0: 9})
		ctx, cancel := context.WithCancel(context.Background())
		hh.onSleep = func(int) { cancel() }
		err := hh.h.await(ctx)
		assert.ErrorIs(t, err, context.Canceled)
		assert.False(t, hh.released())
	})
	t.Run("进入前已取消", func(t *testing.T) {
		hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
		ctx, cancel := context.WithCancel(context.Background())
		cancel()
		assert.ErrorIs(t, hh.h.await(ctx), context.Canceled)
		assert.Zero(t, hh.factory.builds(), "a canceled wait must not touch Kafka")
		assert.False(t, hh.released())
	})
}

// F3:放行之后再次 await(下一次再平衡)不再查询对方,也不重建 inspector。
func TestGroupHandoffAwait_OnlyOncePerProcess(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 1, 1: 2})
	require.NoError(t, hh.h.await(context.Background()))
	hh.cluster.resetCalls()
	builds := hh.factory.builds()

	// 对方又开始持有:已放行的进程不再看它(只闸一次)。
	hh.cluster.setState(testLegacyGroup, groupStateStable, []int32{0})
	require.NoError(t, hh.h.await(context.Background()))
	assert.Zero(t, hh.cluster.totalCalls(), "a released gate must not query Kafka again")
	assert.Equal(t, builds, hh.factory.builds())
	assert.Zero(t, hh.sleepCount())
}

// 修订稿 R2:放行时本组位点 ≥0 的分区直接算落定,Setup 只查其余分区。
func TestGroupHandoffGate_ReleaseSeedsSettledFromOwnOffsets(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 10})
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 5, 1: 7})
	require.NoError(t, hh.h.await(context.Background()))
	assert.Equal(t, set(0), hh.settled())

	hh.cluster.resetCalls()
	session := &recordingSession{}
	require.NoError(t, hh.h.seed(session, []int32{0, 1}))
	assert.Equal(t, [][]int32{{1}}, hh.cluster.committedAskedOf(testZoneGroup), "only the unsettled partition is queried")
	assert.Equal(t, []markOffsetCall{{topic: testHandoffTopic, partition: 1, offset: 7}}, session.markOffsetCallsSnapshot())
}

// ---------------------------------------------------------------------------
// Setup 播种(seed)
// ---------------------------------------------------------------------------

// T1b:本组没有位点、对方有 100:MarkOffset(topic, p, 100, "") 只调一次,原值不加 1,计 seeded。
func TestGroupHandoffSeed_MarksPeerCommittedWithoutPlusOne(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.forceRelease()
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 100})
	session := &recordingSession{}
	seededBefore := seedCount(t, metrics.HandoffSeedSeeded)

	require.NoError(t, hh.h.seed(session, []int32{0}))
	assert.Equal(t, []markOffsetCall{{topic: testHandoffTopic, partition: 0, offset: 100}}, session.markOffsetCallsSnapshot())
	assert.Equal(t, seededBefore+1, seedCount(t, metrics.HandoffSeedSeeded))
	assert.Empty(t, hh.settled(), "a freshly seeded partition is not settled until its own offset is committed")
}

// T3:本组已有位点 50:不调 MarkOffset,计 own_present,且不查询对方。
func TestGroupHandoffSeed_KeepsOwnOffsets(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.forceRelease()
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 50})
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 100})
	session := &recordingSession{}
	ownBefore := seedCount(t, metrics.HandoffSeedOwnPresent)

	require.NoError(t, hh.h.seed(session, []int32{0}))
	assert.Empty(t, session.markOffsetCallsSnapshot())
	assert.Equal(t, ownBefore+1, seedCount(t, metrics.HandoffSeedOwnPresent))
	assert.Zero(t, hh.cluster.committedCallsOf(testLegacyGroup), "the peer must not be queried when own offsets exist")
	assert.Equal(t, set(0), hh.settled())
}

// T4:两组都没有位点:不调 MarkOffset,计 no_offsets,分区进入 settled(之后不再查询)。
func TestGroupHandoffSeed_NoOffsetsAnywhere(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.forceRelease()
	session := &recordingSession{}
	noneBefore := seedCount(t, metrics.HandoffSeedNoOffsets)

	require.NoError(t, hh.h.seed(session, []int32{0}))
	assert.Empty(t, session.markOffsetCallsSnapshot())
	assert.Equal(t, noneBefore+1, seedCount(t, metrics.HandoffSeedNoOffsets))
	assert.Equal(t, set(0), hh.settled())
}

// 修订稿 R2 / R4:落定的分区不再查 admin;刚播种的分区下次 Setup 只查一次本组,查到 ≥0 才落定。
func TestGroupHandoffSeed_SkipsSettledPartitions(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 4)
	hh.forceRelease()
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 10, 1: 11})
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 5, 1: 6, 3: 30})

	// 第一次 Setup:{0,1} 本组已有,{2} 两边都没有,{3} 播种。
	first := &recordingSession{}
	require.NoError(t, hh.h.seed(first, []int32{0, 1, 2, 3}))
	assert.Equal(t, []markOffsetCall{{topic: testHandoffTopic, partition: 3, offset: 30}}, first.markOffsetCallsSnapshot())
	assert.Equal(t, set(0, 1, 2), hh.settled())

	// 第二次 Setup 分到 {0,1,2}:全部落定,不碰 admin。
	hh.cluster.resetCalls()
	builds := hh.factory.builds()
	second := &recordingSession{}
	require.NoError(t, hh.h.seed(second, []int32{0, 1, 2}))
	assert.Zero(t, hh.cluster.totalCalls())
	assert.Equal(t, builds, hh.factory.builds())
	assert.Empty(t, second.markOffsetCallsSnapshot())

	// 分到 {3}:播种值已提交,只查一次本组,查到 ≥0 后落定、不再查对方。
	hh.cluster.setCommitted(testZoneGroup, map[int32]int64{0: 10, 1: 11, 3: 30})
	third := &recordingSession{}
	require.NoError(t, hh.h.seed(third, []int32{3}))
	assert.Equal(t, 1, hh.cluster.committedCallsOf(testZoneGroup))
	assert.Zero(t, hh.cluster.committedCallsOf(testLegacyGroup))
	assert.Empty(t, third.markOffsetCallsSnapshot())
	assert.Equal(t, set(0, 1, 2, 3), hh.settled())

	hh.cluster.resetCalls()
	require.NoError(t, hh.h.seed(&recordingSession{}, []int32{3}))
	assert.Zero(t, hh.cluster.totalCalls())
}

// 修订稿 R2:播种查询失败时每个待查分区计 aborted、返回 error,不调 MarkOffset;inspector 被丢弃。
func TestGroupHandoffSeed_AbortsOnQueryFailure(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
	hh.forceRelease()
	hh.cluster.failCommitted(testLegacyGroup, errors.New("coordinator moved"))
	session := &recordingSession{}
	abortedBefore := seedCount(t, metrics.HandoffSeedAborted)

	err := hh.h.seed(session, []int32{0, 1})
	require.Error(t, err)
	assert.Empty(t, session.markOffsetCallsSnapshot())
	assert.Equal(t, abortedBefore+2, seedCount(t, metrics.HandoffSeedAborted))
	assert.Equal(t, 1, hh.factory.inspector(0).closeCount())
	assert.Empty(t, hh.settled())
}

// T5:只统计本 topic 的分区;成员分配解码失败 fail-closed。
func TestHeldPartitions_OnlyCountsThisTopic(t *testing.T) {
	desc := &sarama.GroupDescription{
		GroupId: testLegacyGroup,
		State:   groupStateStable,
		Members: map[string]*sarama.GroupMemberDescription{
			"other-zone": {MemberAssignment: encodeAssignment(t, map[string][]int32{"db_task_zone_7": {0, 1}})},
			"no-assign":  {},
		},
	}
	held, err := heldPartitions(desc, testHandoffTopic)
	require.NoError(t, err)
	assert.Empty(t, held, "members consuming other zones' topics do not hold this topic")

	desc.Members["this-zone"] = &sarama.GroupMemberDescription{
		MemberAssignment: encodeAssignment(t, map[string][]int32{testHandoffTopic: {3, 4}, "db_task_zone_7": {2}}),
	}
	held, err = heldPartitions(desc, testHandoffTopic)
	require.NoError(t, err)
	assert.Equal(t, set(3, 4), held)

	desc.Members["corrupt"] = &sarama.GroupMemberDescription{MemberAssignment: []byte{0x00, 0x01, 0x02}}
	_, err = heldPartitions(desc, testHandoffTopic)
	assert.Error(t, err, "an undecodable assignment must not be read as 'not holding'")
}

func TestPeerViewFromDescription_ErrorMapping(t *testing.T) {
	view, err := peerViewFromDescription(&sarama.GroupDescription{GroupId: "g", Err: sarama.ErrGroupIDNotFound}, testHandoffTopic)
	require.NoError(t, err)
	assert.Equal(t, groupStateDead, view.State)
	assert.Empty(t, view.Held)

	_, err = peerViewFromDescription(&sarama.GroupDescription{GroupId: "g", Err: sarama.ErrNotCoordinatorForConsumer}, testHandoffTopic)
	assert.ErrorIs(t, err, sarama.ErrNotCoordinatorForConsumer)
}

// ---------------------------------------------------------------------------
// 守望(watch / watchOnce)与让出
// ---------------------------------------------------------------------------

// releasedHarness 建一个已放行(对方 Empty)的交接闸,并登记一个计数的让出函数。
func releasedHarness(t *testing.T, partitions int32, peerCommitted map[int32]int64) (*handoffHarness, *int) {
	t.Helper()
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, partitions)
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, peerCommitted)
	require.NoError(t, hh.h.await(context.Background()))
	yields := 0
	require.True(t, hh.h.setSessionYield(func() { yields++ }))
	return hh, &yields
}

// T7(F1):第 1 拍建基线;第 2 拍对方 Stable、持有、位点不变 → 不让出;第 3 拍位点前进 → 让出。
func TestGroupHandoffWatch_YieldsOnlyWhenPeerHoldsAndAdvances(t *testing.T) {
	hh, yields := releasedHarness(t, 2, map[int32]int64{0: 100, 1: 200})
	conflictBefore := conflictCount(t)

	hh.cluster.setState(testLegacyGroup, groupStateStable, []int32{0})
	hh.h.watchOnce()
	assert.Zero(t, *yields, "the first observation only records a baseline")
	hh.h.watchOnce()
	assert.Zero(t, *yields, "holding without progress is not positive evidence of consumption")
	assert.True(t, hh.released())

	hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 101, 1: 200})
	hh.h.watchOnce()
	assert.Equal(t, 1, *yields)
	assert.False(t, hh.released(), "a yield must send the next Consume back through the gate")
	assert.Equal(t, conflictBefore+1, conflictCount(t))
}

// F1 / B1:被闸住的对方不入组、不持有分配,所以绝不触发让出;查询出错也不让出。
// 两侧都配 GroupHandoffFrom 时(正向的 Z 在跑、反向的 L 刚起)双方各自前进,不互等。
func TestGroupHandoffWatch_DoesNotYieldToGatedPeer(t *testing.T) {
	t.Run("对方 Empty,位点前进也不让出", func(t *testing.T) {
		hh, yields := releasedHarness(t, 1, map[int32]int64{0: 100})
		hh.h.watchOnce()
		hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 150})
		hh.h.watchOnce()
		assert.Zero(t, *yields)
		assert.True(t, hh.released())
	})
	t.Run("对方 Stable 但不持有本 topic(别的 zone 的成员)", func(t *testing.T) {
		hh, yields := releasedHarness(t, 1, map[int32]int64{0: 100})
		hh.cluster.setState(testLegacyGroup, groupStateStable, nil)
		hh.h.watchOnce()
		hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 150})
		hh.h.watchOnce()
		assert.Zero(t, *yields)
	})
	t.Run("查询出错不让出", func(t *testing.T) {
		hh, yields := releasedHarness(t, 1, map[int32]int64{0: 100})
		hh.cluster.setState(testLegacyGroup, groupStateStable, []int32{0})
		hh.h.watchOnce()
		hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 150})
		hh.cluster.failDescribe(testLegacyGroup, errors.New("broker down"))
		hh.h.watchOnce()
		assert.Zero(t, *yields, "only positive evidence may trigger a yield")
		assert.True(t, hh.released())
	})
	t.Run("两侧都配 GroupHandoffFrom:正向与反向不互等", func(t *testing.T) {
		cluster := newFakeCluster()
		// 正向的 Z 已放行、在消费本 topic;legacy 组在本 topic 上的位点已由 FinalizeHandoff 删掉,
		// 组里还有别的 zone 的成员(Stable,但不持有本 topic)。
		cluster.setGroup(testZoneGroup, groupStateStable, []int32{0, 1}, map[int32]int64{0: 100, 1: 200})
		cluster.setGroup(testLegacyGroup, groupStateStable, nil, nil)
		forward := newHandoffHarnessOn(t, cluster, testZoneGroup, testLegacyGroup, 2)
		require.NoError(t, forward.h.await(context.Background()))
		forwardYields := 0
		require.True(t, forward.h.setSessionYield(func() { forwardYields++ }))

		// 反向的 L Pod 起来:Z 仍持有本 topic → 闸住,不入组。
		reverse := newHandoffHarnessOn(t, cluster, testLegacyGroup, testZoneGroup, 2)
		d := reverse.h.evaluate()
		require.False(t, d.pass)
		assert.Equal(t, metrics.HandoffGateWaitPeerHolding, d.outcome)

		// 被闸住的 L 不持有本 topic、位点不动:正向的守望不让出,Z 照常消费。
		forward.h.watchOnce()
		cluster.setCommitted(testZoneGroup, map[int32]int64{0: 150, 1: 250})
		forward.h.watchOnce()
		assert.Zero(t, forwardYields, "a gated peer must never make the running side yield")

		// Z 的 Pod 全部退出:L 立即放行(不等 Z 的守望),并从 Z 的位点续接。
		cluster.setState(testZoneGroup, groupStateEmpty, nil)
		require.NoError(t, reverse.h.await(context.Background()))
		assert.Zero(t, reverse.sleepCount(), "the reverse side must not wait once the forward side has left")
		session := &recordingSession{}
		require.NoError(t, reverse.h.seed(session, []int32{0, 1}))
		assert.ElementsMatch(t, []markOffsetCall{
			{topic: testHandoffTopic, partition: 0, offset: 150},
			{topic: testHandoffTopic, partition: 1, offset: 250},
		}, session.markOffsetCallsSnapshot())
	})
}

// 守望在 Setup 播种之后、登记让出函数之前已经让出:登记返回 false,调用方须立即取消本 session。
func TestGroupHandoffSetSessionYield_CancelsWhenAlreadyYielded(t *testing.T) {
	hh, _ := releasedHarness(t, 1, map[int32]int64{0: 1})
	hh.h.yield()
	assert.False(t, hh.h.setSessionYield(func() {}))
}

// 修订稿 R2:让出清空 settled,下次放行后的 Setup 重新确认本组位点。
func TestGroupHandoffYield_ClearsSettled(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.forceRelease()
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 40})
	require.NoError(t, hh.h.seed(&recordingSession{}, []int32{0}))
	require.Equal(t, set(0), hh.settled())

	hh.h.yield()
	assert.Empty(t, hh.settled())
	assert.False(t, hh.released())

	hh.forceRelease()
	hh.cluster.resetCalls()
	require.NoError(t, hh.h.seed(&recordingSession{}, []int32{0}))
	assert.Equal(t, 1, hh.cluster.committedCallsOf(testZoneGroup), "after a yield the partition must be re-checked")
}

// ---------------------------------------------------------------------------
// 构造与小工具
// ---------------------------------------------------------------------------

// T8:没有 GroupHandoffFrom(最终形状)时不建交接闸、不调用工厂;有时也只保存工厂(懒建)。
func TestNewGroupHandoffFromConfig_NoInspectorWithoutPeer(t *testing.T) {
	calls := 0
	factory := func() (groupInspector, error) {
		calls++
		return nil, errors.New("must not be called")
	}
	hf, err := newGroupHandoffFromConfig(db_config.KafkaConfig{GroupID: testZoneGroup, Topic: testHandoffTopic, PartitionCnt: 10},
		testZoneGroup, factory)
	require.NoError(t, err)
	assert.Nil(t, hf)
	assert.Zero(t, calls)

	hf, err = newGroupHandoffFromConfig(db_config.KafkaConfig{ConsumerGroup: testZoneGroup, GroupHandoffFrom: testLegacyGroup,
		Topic: testHandoffTopic, PartitionCnt: 10}, testZoneGroup, factory)
	require.NoError(t, err)
	require.NotNil(t, hf)
	assert.Zero(t, calls, "the inspector is built lazily on the first query")
	assert.Equal(t, testLegacyGroup, hf.peer)
	assert.Len(t, hf.partitions, 10)
}

// T9。
func TestNewGroupHandoff_RejectsPeerEqualGroup(t *testing.T) {
	factory := (&fakeInspectorFactory{cluster: newFakeCluster()}).build
	_, err := newGroupHandoff(factory, testZoneGroup, testZoneGroup, testHandoffTopic, 10)
	assert.Error(t, err)
	_, err = newGroupHandoff(factory, testZoneGroup, "", testHandoffTopic, 10)
	assert.Error(t, err)
	_, err = newGroupHandoff(factory, testZoneGroup, testLegacyGroup, testHandoffTopic, 0)
	assert.Error(t, err)
	_, err = newGroupHandoff(nil, testZoneGroup, testLegacyGroup, testHandoffTopic, 10)
	assert.Error(t, err)
	_, err = newGroupHandoffFromConfig(db_config.KafkaConfig{ConsumerGroup: testZoneGroup, GroupHandoffFrom: testZoneGroup,
		Topic: testHandoffTopic, PartitionCnt: 10}, testZoneGroup, factory)
	assert.Error(t, err)
}

// T10。
func TestRebalanceGapTracker(t *testing.T) {
	var g rebalanceGapTracker
	base := time.Date(2026, 10, 15, 12, 0, 0, 0, time.UTC)
	_, ok := g.sessionStarted(base)
	assert.False(t, ok, "the first session of a process has no predecessor")

	g.sessionEnded(base)
	d, ok := g.sessionStarted(base.Add(3 * time.Second))
	assert.True(t, ok)
	assert.Equal(t, 3*time.Second, d)

	_, ok = g.sessionStarted(base.Add(4 * time.Second))
	assert.False(t, ok, "each session end is consumed once")
}

func TestConsumerClientID_Sanitizes(t *testing.T) {
	assert.Equal(t, "db-z7-pod-a", consumerClientID(7, "pod-a"))
	assert.Equal(t, "db-z7-pod-a-b-c", consumerClientID(7, "pod a/b:c"))
	assert.Equal(t, "db-z7", consumerClientID(7, ""))
	assert.Equal(t, "db-z7", consumerClientID(7, "--"))
	assert.Equal(t, "db-z7", consumerClientID(7, "数据库"))
	assert.Equal(t, "db-z7-x", consumerClientID(7, "-x-"))
	long := consumerClientID(7, strings.Repeat("a", 500))
	assert.Len(t, long, consumerClientIDMaxLen)
	assert.True(t, strings.HasPrefix(long, "db-z7-a"))
}

// ---------------------------------------------------------------------------
// 接到消费者上(Setup / Cleanup / ConsumeClaim)
// ---------------------------------------------------------------------------

// Setup 只对 claims 里的分区播种;之后守望让出时,handler 的 yieldCtx 被取消。
func TestConsumerSetup_SeedsClaimedPartitionsAndArmsYield(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 4)
	hh.cluster.setGroup(testLegacyGroup, groupStateEmpty, nil, map[int32]int64{0: 10, 1: 11, 2: 12, 3: 13})
	require.NoError(t, hh.h.await(context.Background()))

	c := &KeyOrderedKafkaConsumer{topic: testHandoffTopic, groupID: testZoneGroup, partitionCount: 4, handoff: hh.h}
	handler := &consumerGroupHandler{consumer: c}
	sessionCtx, cancelSession := context.WithCancel(context.Background())
	defer cancelSession()
	session := &recordingSession{ctx: sessionCtx, claims: map[string][]int32{testHandoffTopic: {1, 3}}}

	require.NoError(t, handler.Setup(session))
	assert.ElementsMatch(t, []markOffsetCall{
		{topic: testHandoffTopic, partition: 1, offset: 11},
		{topic: testHandoffTopic, partition: 3, offset: 13},
	}, session.markOffsetCallsSnapshot())
	parent, yieldDone := handler.sessionScope(session)
	require.NotNil(t, yieldDone)
	require.NoError(t, parent.Err())

	// 误部署的旧发布物回到 legacy 组、拿走本 topic 的分区并推进位点 → 守望让出。
	hh.cluster.setState(testLegacyGroup, groupStateStable, []int32{0})
	hh.h.watchOnce()
	hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 20, 1: 11, 2: 12, 3: 13})
	hh.h.watchOnce()
	select {
	case <-yieldDone:
	default:
		t.Fatal("a watcher yield must cancel the session scope")
	}
	assert.True(t, handler.yielded())
	assert.NoError(t, sessionCtx.Err(), "a yield cancels only the handler scope, not the sarama session")
	assert.NotPanics(t, func() { require.NoError(t, handler.Cleanup(session)) })
}

// 播种查询失败:Setup 返回 error、不调 MarkOffset,随后 sarama 调的 Cleanup 不 panic。
func TestConsumerSetup_SeedFailureFailsSession(t *testing.T) {
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 2)
	hh.forceRelease()
	hh.cluster.failCommitted(testLegacyGroup, errors.New("coordinator moved"))

	c := &KeyOrderedKafkaConsumer{topic: testHandoffTopic, groupID: testZoneGroup, partitionCount: 2, handoff: hh.h}
	handler := &consumerGroupHandler{consumer: c}
	session := &recordingSession{claims: map[string][]int32{testHandoffTopic: {0}}}

	err := handler.Setup(session)
	require.Error(t, err)
	assert.Contains(t, err.Error(), "group handoff seeding failed")
	assert.Empty(t, session.markOffsetCallsSnapshot())
	assert.Nil(t, handler.yieldCtx)
	assert.NotPanics(t, func() { require.NoError(t, handler.Cleanup(session)) })
}

// 先让出、再用空消息通道调 ConsumeClaim:应及时返回 nil,不向 worker 派发任何任务,也不把让出当成 session 结束。
func TestConsumeClaim_YieldReturnsNilAndStopsDispatch(t *testing.T) {
	w, _, _ := newTestWorker(t, 0)
	c := &KeyOrderedKafkaConsumer{topic: w.topic, partitionCount: 1, workers: map[int32]*worker{0: w}}
	sessionCtx, cancelSession := context.WithCancel(context.Background())
	defer cancelSession()
	session := &recordingSession{ctx: sessionCtx, claims: map[string][]int32{w.topic: {0}}}
	handler := &consumerGroupHandler{consumer: c}
	handler.yieldCtx, handler.yieldCancel = context.WithCancel(sessionCtx)
	handler.yieldCancel()

	claim := &fakeClaim{topic: w.topic, partition: 0, messages: make(chan *sarama.ConsumerMessage)}
	done := make(chan error, 1)
	go func() { done <- handler.ConsumeClaim(session, claim) }()
	select {
	case err := <-done:
		assert.NoError(t, err)
	case <-time.After(5 * time.Second):
		t.Fatal("ConsumeClaim must return promptly after a yield")
	}
	assert.Zero(t, len(w.taskCh), "nothing may be dispatched after a yield")
	assert.NoError(t, sessionCtx.Err())
	assert.Empty(t, session.marked())
}

// handoff 为 nil(最终形状)时:Setup 不播种、不建 yieldCtx,sessionScope 退回 session ctx 与 nil channel。
func TestConsumerSetup_NoHandoffKeepsLegacyBehavior(t *testing.T) {
	c := &KeyOrderedKafkaConsumer{topic: testHandoffTopic, groupID: testLegacyGroup, partitionCount: 2}
	handler := &consumerGroupHandler{consumer: c}
	sessionCtx, cancelSession := context.WithCancel(context.Background())
	defer cancelSession()
	session := &recordingSession{ctx: sessionCtx, claims: map[string][]int32{testHandoffTopic: {0, 1}}}

	require.NoError(t, handler.Setup(session))
	assert.Empty(t, session.markOffsetCallsSnapshot())
	assert.Nil(t, handler.yieldCtx)
	parent, yieldDone := handler.sessionScope(session)
	assert.True(t, parent == session.Context(), "without a handoff the dispatch scope is the session context itself")
	assert.Nil(t, yieldDone)
	assert.False(t, handler.yielded())
	require.NoError(t, handler.Cleanup(session))
}

// 消费组键的组合在连 Kafka 之前校验:GroupID + GroupHandoffFrom(旧二进制会错误接受的形状)直接拒绝,
// 错误文本是校验错误而不是「create consumer group failed」(broker 地址故意不可达)。
func TestNewKeyOrderedKafkaConsumer_RejectsInvalidConsumerGroupBeforeKafka(t *testing.T) {
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	cfg := db_config.Config{ZoneId: 101}
	cfg.ServerConfig.Kafka = db_config.KafkaConfig{
		Brokers:          []string{"127.0.0.1:1"},
		GroupID:          testZoneGroup,
		GroupHandoffFrom: testLegacyGroup,
		Topic:            testHandoffTopic,
		PartitionCnt:     10,
		RetryInstanceId:  "handoff-test",
	}
	_, err := NewKeyOrderedKafkaConsumer(cfg, rc, rc, newFakeStores())
	require.Error(t, err)
	assert.Contains(t, err.Error(), "invalid Kafka consumer group config")
	assert.Contains(t, err.Error(), "requires Kafka.ConsumerGroup")
}

// 再平衡空窗:第一个 session 不记;Cleanup 之后的下一次 Setup 记一次,含中间 Setup 失败的重试。
func TestConsumerSetup_ObservesRebalanceGap(t *testing.T) {
	now := time.Date(2026, 10, 15, 12, 0, 0, 0, time.UTC)
	c := &KeyOrderedKafkaConsumer{topic: testHandoffTopic, groupID: testLegacyGroup, partitionCount: 2,
		now: func() time.Time { return now }}
	session := &recordingSession{claims: map[string][]int32{testHandoffTopic: {0}}}

	first := &consumerGroupHandler{consumer: c}
	require.NoError(t, first.Setup(session))
	require.NoError(t, first.Cleanup(session))

	// 失败的 Setup(分区越界)之后的 Cleanup 不覆盖上一个 session 的结束时刻。
	now = now.Add(2 * time.Second)
	failed := &consumerGroupHandler{consumer: c}
	require.Error(t, failed.Setup(&recordingSession{claims: map[string][]int32{testHandoffTopic: {5}}}))
	require.NoError(t, failed.Cleanup(session))

	now = now.Add(3 * time.Second)
	d, ok := c.gap.sessionStarted(now)
	require.True(t, ok)
	assert.Equal(t, 5*time.Second, d, "the gap runs from the last real session end, across failed Setups")
}
