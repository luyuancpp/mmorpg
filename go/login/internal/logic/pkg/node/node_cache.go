package node

import (
	"sort"
	"sync"
	"time"

	login_proto "proto/common/base"
	"shared/nodeinfo"

	"github.com/zeromicro/go-zero/core/logx"
)

// gateReportPeriod 是 C++ gate 往 etcd 上报 NodeInfo(含 player_count)的周期
// (cpp/nodes/gate/main.cpp 的 playerCountReportTimer,每次无条件 Put,值不变也产生 watch 事件)。
// 镜像的两条时限都按它推:健康的 watch 下,镜像里每台 gate 每个周期都会被刷新一次。
// 改 gate 上报周期必须同步改这里。
const gateReportPeriod = 10 * time.Second

// cacheResyncAfter:镜像里最久没被刷新的那台 gate 超过这么久,就当 watch 可能已经静默卡住,主动重新全量同步。
// 比一个上报周期多 2s 余量;某台 gate 漏一次上报(或还没开始上报)也只是多一次 Range。
// 镜像为空(本 zone 没有 gate)时按「距上次事件或同步」计,于是空 zone 按这个间隔周期性重同步,镜像保持新鲜。
const cacheResyncAfter = gateReportPeriod + 2*time.Second

// cacheStaleAfter:镜像里最久没被刷新的那台 gate 超过这么久,读路径就不再信镜像,回落到直读 etcd。
// 新鲜度按每台 gate 各自计时,而不是整个前缀共用一个时间戳:多 gate 时别的 gate 的事件不能替一台没刷新的
// gate 作证(评审 A1)。于是镜像里任一台 gate 的数据,最多比 etcd 里的落后约 cacheStaleAfter - gateReportPeriod = 5s
// (流卡住时,它错过的那次上报之后最多 5s 就被重同步补上或判过期)。
// 排队在途占位宽限 loginqueue.defaultHandoffGrace(20s)按「gate 人数上报滞后约 10s + 建连余量」定,
// 镜像让人数最坏再晚约 5s 出现,仍在宽限之内。改这里要同步核对那条宽限,以及
// tools/scripts/k8s_gate_drain.ps1 契约里的 LoginMirrorSettleSeconds(清排空标记前的等待,必须大于本值)。
const cacheStaleAfter = gateReportPeriod + 5*time.Second

// 读路径回落直读的原因,用作 login_gate_directory_reads_total 的 source 标签。
const (
	readSourceCache    = "cache"
	readSourceUnsynced = "etcd_unsynced" // 镜像还没完成第一次全量同步(起服、或 Run 没起)
	readSourceStale    = "etcd_stale"    // 镜像里有 gate 超过 cacheStaleAfter 没刷新(etcd 不可达、watch 卡住且重同步失败)
)

// nodeEvent 是一条 etcd 变更,与 clientv3 类型解耦,让镜像的状态机能脱离 etcd 单测。
type nodeEvent struct {
	key     string
	value   []byte
	deleted bool
}

// cachedNode 是镜像里的一台节点:解析后的 NodeInfo 与它最近一次被刷新(事件或全量同步)的时刻。
type cachedNode struct {
	info   *login_proto.NodeInfo
	seenAt time.Time
}

// nodeCache 是一个 etcd 前缀的进程内镜像:key → 解析后的 NodeInfo。
//
// 线程模型:写者只有 NodeWatcher.Run 所在的那一个 goroutine(replaceAll / apply),读者是任意多个
// 请求 goroutine(view)与 watch 循环自己(silentFor)。全部状态由 mu 保护。
//
// 所有权:view 返回的切片与其中的 *NodeInfo 在交出去之后不再被改 —— 每次变更都新建切片、
// 新解析一份 NodeInfo 替换旧指针,从不原地修改。调用方也只能读,不得修改。
type nodeCache struct {
	name string
	now  func() time.Time

	mu    sync.RWMutex
	byKey map[string]cachedNode
	nodes []*login_proto.NodeInfo // 按 key 排序的只读快照,byKey 每变一次重建一次
	// freshAt 是镜像整体的新鲜度基准:非空时取所有节点里最早的 seenAt,为空时取最近一次事件或全量同步的时刻。
	// 每次写入后重算,读路径只比一次时间。
	freshAt time.Time
	synced  bool // 第一次全量同步成功之后为 true,之后不回落
}

func newNodeCache(name string, now func() time.Time) *nodeCache {
	return &nodeCache{name: name, now: now, byKey: make(map[string]cachedNode)}
}

// replaceAll 用一次全量 Range 的结果整体替换镜像,所有节点的刷新时刻记为本次同步。解析失败的记录丢弃,
// 与直读路径(FetchAllNodesCtx)跳过坏记录的口径一致。返回替换前的节点数与这是不是第一次同步,供调用方决定要不要打日志。
func (c *nodeCache) replaceAll(kvs []nodeEvent) (prevCount int, first bool) {
	now := c.now()
	next := make(map[string]cachedNode, len(kvs))
	for _, kv := range kvs {
		if info, ok := c.parse(kv); ok {
			next[kv.key] = cachedNode{info: info, seenAt: now}
		}
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	prevCount, first = len(c.byKey), !c.synced
	c.byKey = next
	c.rebuildLocked(now)
	c.synced = true
	return prevCount, first
}

// apply 应用一批 watch 事件,被 PUT 的节点刷新时刻记为现在。PUT 一条解析不了的值等同于删除:
// 直读路径同样会跳过它,镜像不能比直读多留一台 gate。
func (c *nodeCache) apply(events []nodeEvent) {
	now := c.now()
	c.mu.Lock()
	defer c.mu.Unlock()
	for _, ev := range events {
		if ev.deleted {
			delete(c.byKey, ev.key)
			continue
		}
		if info, ok := c.parse(ev); ok {
			c.byKey[ev.key] = cachedNode{info: info, seenAt: now}
		} else {
			delete(c.byKey, ev.key)
		}
	}
	c.rebuildLocked(now)
}

// rebuildLocked 重建排序快照与新鲜度基准。调用方持有写锁;now 是本次写入的时刻(镜像为空时的基准)。
func (c *nodeCache) rebuildLocked(now time.Time) {
	keys := make([]string, 0, len(c.byKey))
	for k := range c.byKey {
		keys = append(keys, k)
	}
	// 按 key 排序输出,与 etcd 前缀 Range 的返回顺序一致,换读法不改变下游看到的顺序。
	sort.Strings(keys)
	nodes := make([]*login_proto.NodeInfo, 0, len(keys))
	freshAt := now
	for _, k := range keys {
		n := c.byKey[k]
		nodes = append(nodes, n.info)
		if n.seenAt.Before(freshAt) {
			freshAt = n.seenAt
		}
	}
	c.nodes = nodes
	c.freshAt = freshAt
}

// silentFor 返回镜像里最久没被刷新的那台节点已经多久没刷新(镜像为空时是距上次事件或同步多久),
// 以及当前节点数;还没同步过时时长为 0(由全量同步的重试负责,不在这里判)。
func (c *nodeCache) silentFor() (time.Duration, int) {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if !c.synced {
		return 0, len(c.byKey)
	}
	return c.now().Sub(c.freshAt), len(c.byKey)
}

// view 返回镜像快照。ok=false 时调用方必须回落直读,source 说明原因;ok=true 时 source 为 cache。
func (c *nodeCache) view() (nodes []*login_proto.NodeInfo, ok bool, source string) {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if !c.synced {
		return nil, false, readSourceUnsynced
	}
	if c.now().Sub(c.freshAt) > cacheStaleAfter {
		return nil, false, readSourceStale
	}
	return c.nodes, true, readSourceCache
}

func (c *nodeCache) parse(kv nodeEvent) (*login_proto.NodeInfo, bool) {
	info := &login_proto.NodeInfo{}
	// 宽松解析(忽略未知字段,D77):新版本 gate 写入的新字段不能让整台 gate 从候选里消失。
	if err := nodeinfo.Unmarshal(kv.value, info); err != nil {
		logx.Errorf("[NodeWatcher:%s] Invalid NodeInfo JSON for key=%s: %v", c.name, kv.key, err)
		return nil, false
	}
	return info, true
}
