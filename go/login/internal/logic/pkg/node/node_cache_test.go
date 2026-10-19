package node

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	login_proto "proto/common/base"
)

// fakeClock 是手动时钟:只在测试里前进,不碰真实墙钟。watch 循环与测试 goroutine 会并发读写,所以加锁。
type fakeClock struct {
	mu sync.Mutex
	t  time.Time
}

func newFakeClock() *fakeClock { return &fakeClock{t: time.Unix(1_800_000_000, 0)} }

func (c *fakeClock) now() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.t
}

func (c *fakeClock) advance(d time.Duration) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.t = c.t.Add(d)
}

func gateJSON(nodeID, playerCount uint32) []byte {
	return []byte(fmt.Sprintf(`{"nodeId":%d,"zoneId":1,"playerCount":%d,"endpoint":{"ip":"10.0.0.%d","port":9000}}`,
		nodeID, playerCount, nodeID))
}

func put(key string, nodeID, playerCount uint32) nodeEvent {
	return nodeEvent{key: key, value: gateJSON(nodeID, playerCount)}
}

func nodeIDs(nodes []*login_proto.NodeInfo) []uint32 {
	out := make([]uint32, 0, len(nodes))
	for _, n := range nodes {
		out = append(out, n.GetNodeId())
	}
	return out
}

func newTestCache() (*nodeCache, *fakeClock) {
	clk := newFakeClock()
	return newNodeCache("test", clk.now), clk
}

func mustView(t *testing.T, c *nodeCache) []*login_proto.NodeInfo {
	t.Helper()
	nodes, ok, source := c.view()
	if !ok || source != readSourceCache {
		t.Fatalf("view = (ok=%v, source=%q), want a fresh cache view", ok, source)
	}
	return nodes
}

// 第一次全量同步之前镜像不可信,读路径必须回落直读;silentFor 也不触发重同步。
func TestNodeCache_UnsyncedIsNotServed(t *testing.T) {
	c, _ := newTestCache()
	if nodes, ok, source := c.view(); ok || nodes != nil || source != readSourceUnsynced {
		t.Fatalf("view before sync = (%v, %v, %q), want (nil, false, %q)", nodes, ok, source, readSourceUnsynced)
	}
	if silent, _ := c.silentFor(); silent != 0 {
		t.Fatalf("silentFor before sync = %s, want 0", silent)
	}
}

// 全量同步:坏记录丢弃(与直读跳过坏记录同口径),输出按 key 排序、与 etcd Range 顺序一致。
func TestNodeCache_ReplaceAllSkipsInvalidAndSortsByKey(t *testing.T) {
	c, _ := newTestCache()
	prev, first := c.replaceAll([]nodeEvent{
		put("gate/3", 3, 30),
		{key: "gate/2", value: []byte("{not json")},
		put("gate/1", 1, 10),
	})
	if prev != 0 || !first {
		t.Fatalf("replaceAll = (prev=%d, first=%v), want (0, true)", prev, first)
	}
	if got := nodeIDs(mustView(t, c)); fmt.Sprint(got) != "[1 3]" {
		t.Fatalf("nodes = %v, want [1 3]", got)
	}
	if prev, first := c.replaceAll([]nodeEvent{put("gate/1", 1, 11)}); prev != 2 || first {
		t.Fatalf("second replaceAll = (prev=%d, first=%v), want (2, false)", prev, first)
	}
	if got := nodeIDs(mustView(t, c)); fmt.Sprint(got) != "[1]" {
		t.Fatalf("nodes after resync = %v, want [1] (keys missing from the new range are dropped)", got)
	}
}

// 增量:PUT 新增 / 覆盖,DELETE 删除,PUT 一条坏值等同删除(直读同样会跳过它)。
func TestNodeCache_ApplyEvents(t *testing.T) {
	c, _ := newTestCache()
	c.replaceAll([]nodeEvent{put("gate/1", 1, 10), put("gate/2", 2, 20)})

	c.apply([]nodeEvent{
		put("gate/3", 3, 30),
		put("gate/1", 1, 15),
		{key: "gate/2", deleted: true},
	})
	nodes := mustView(t, c)
	if got := nodeIDs(nodes); fmt.Sprint(got) != "[1 3]" {
		t.Fatalf("nodes = %v, want [1 3]", got)
	}
	if nodes[0].GetPlayerCount() != 15 {
		t.Fatalf("gate 1 player_count = %d, want the updated 15", nodes[0].GetPlayerCount())
	}

	c.apply([]nodeEvent{{key: "gate/3", value: []byte("garbage")}})
	if got := nodeIDs(mustView(t, c)); fmt.Sprint(got) != "[1]" {
		t.Fatalf("nodes after invalid PUT = %v, want [1]", got)
	}
}

// 交出去的快照之后不再变:后续事件只换新切片,不原地改旧切片或旧元素。
func TestNodeCache_ViewIsImmutableSnapshot(t *testing.T) {
	c, _ := newTestCache()
	c.replaceAll([]nodeEvent{put("gate/1", 1, 10), put("gate/2", 2, 20)})
	before := mustView(t, c)

	c.apply([]nodeEvent{put("gate/1", 1, 99), {key: "gate/2", deleted: true}})

	if len(before) != 2 || before[0].GetPlayerCount() != 10 || before[1].GetNodeId() != 2 {
		t.Fatalf("earlier snapshot changed after apply: %v", before)
	}
}

// 过期:超过 cacheStaleAfter 没有任何刷新就不再提供镜像;一条事件或一次全量同步即恢复。
func TestNodeCache_StaleAfterSilenceAndRecovers(t *testing.T) {
	c, clk := newTestCache()
	c.replaceAll([]nodeEvent{put("gate/1", 1, 10)})

	clk.advance(cacheStaleAfter)
	mustView(t, c) // 恰好等于阈值仍算新鲜

	clk.advance(time.Millisecond)
	if _, ok, source := c.view(); ok || source != readSourceStale {
		t.Fatalf("view after silence = (ok=%v, source=%q), want stale", ok, source)
	}
	if silent, count := c.silentFor(); silent <= cacheResyncAfter || count != 1 {
		t.Fatalf("silentFor = (%s, %d), want > %s with 1 node", silent, count, cacheResyncAfter)
	}

	c.apply([]nodeEvent{put("gate/1", 1, 11)})
	mustView(t, c)

	clk.advance(cacheStaleAfter + time.Second)
	c.replaceAll(nil) // 空 zone 的周期性重同步同样刷新基准
	if nodes := mustView(t, c); len(nodes) != 0 {
		t.Fatalf("nodes after empty resync = %v, want none", nodes)
	}
}

// 回归(评审 A1):新鲜度按每台 gate 计。两台 gate 时,一台不停上报不能替另一台作证:
// 没刷新的那台一超过 cacheResyncAfter 就要触发重同步,超过 cacheStaleAfter 就不再提供镜像。
// 修复前整个前缀共用一个时间戳,下面的断言全部不成立。
func TestNodeCache_FreshnessIsPerNode(t *testing.T) {
	c, clk := newTestCache()
	c.replaceAll([]nodeEvent{put("gate/1", 1, 10), put("gate/2", 2, 20)})

	// gate/1 每秒都上报,gate/2 一直不来。
	for elapsed := time.Duration(0); elapsed <= cacheResyncAfter; elapsed += time.Second {
		clk.advance(time.Second)
		c.apply([]nodeEvent{put("gate/1", 1, 10)})
	}
	if silent, count := c.silentFor(); silent <= cacheResyncAfter || count != 2 {
		t.Fatalf("silentFor = (%s, %d), want > %s: gate/2 has not been refreshed", silent, count, cacheResyncAfter)
	}
	for elapsed := cacheResyncAfter; elapsed <= cacheStaleAfter; elapsed += time.Second {
		clk.advance(time.Second)
		c.apply([]nodeEvent{put("gate/1", 1, 10)})
	}
	if _, ok, source := c.view(); ok || source != readSourceStale {
		t.Fatalf("view = (ok=%v, source=%q), want stale while gate/2 is not refreshed", ok, source)
	}

	// gate/2 被删除(进程退出撤租约)后,剩下的都新鲜。
	c.apply([]nodeEvent{{key: "gate/2", deleted: true}})
	if got := nodeIDs(mustView(t, c)); fmt.Sprint(got) != "[1]" {
		t.Fatalf("nodes = %v, want [1]", got)
	}
}

// 两条时限的先后关系是契约:重同步要先于读路径判过期发生,否则空 zone 会周期性落到逐请求直读。
func TestNodeCache_ResyncFiresBeforeReadsGoStale(t *testing.T) {
	if cacheResyncAfter+silenceCheckEvery >= cacheStaleAfter {
		t.Fatalf("cacheResyncAfter(%s)+silenceCheckEvery(%s) must be < cacheStaleAfter(%s)",
			cacheResyncAfter, silenceCheckEvery, cacheStaleAfter)
	}
	if cacheResyncAfter <= gateReportPeriod {
		t.Fatalf("cacheResyncAfter(%s) must exceed the gate report period (%s)", cacheResyncAfter, gateReportPeriod)
	}
}

// Nodes 在镜像新鲜时直接返回镜像,不碰 etcd(kv 为 nil 也不会被用到)。
func TestNodeWatcher_NodesServesFreshMirrorWithoutEtcd(t *testing.T) {
	c, _ := newTestCache()
	c.replaceAll([]nodeEvent{put("gate/1", 1, 10)})
	nw := &NodeWatcher{prefix: "test", cache: c}

	nodes, err := nw.Nodes(context.Background())
	if err != nil || fmt.Sprint(nodeIDs(nodes)) != "[1]" {
		t.Fatalf("Nodes = (%v, %v), want gate 1 from the mirror", nodeIDs(nodes), err)
	}
}
