package playerstore

import (
	"context"
	"errors"
	"fmt"
	"sort"
	"strings"
	"sync"

	"github.com/redis/go-redis/v9"
	"shared/redisslot"
)

// MGetBySlot 是跨玩家批量读的唯一正确写法:按 slot 分组,每组一条 MGET,整批走一个 pipeline
// (ClusterClient 会按节点拆批),结果按输入顺序还原。返回值与 MGet 同形:缺键为 nil,存在为 string。
//
// go-redis 的 ClusterClient 按第一个键路由 MGET,不会拆分跨槽的键,直接 MGET 会收到 CROSSSLOT。
// 任何一组失败都返回错误、不返回部分结果:调用方自行决定 fail-open 还是 fail-closed。
func MGetBySlot(ctx context.Context, c redis.UniversalClient, keys []string) ([]interface{}, error) {
	if len(keys) == 0 {
		return nil, nil
	}
	groups := make(map[uint16][]int)
	order := make([]uint16, 0)
	for i, key := range keys {
		slot := redisslot.HashSlot(key)
		if _, seen := groups[slot]; !seen {
			order = append(order, slot)
		}
		groups[slot] = append(groups[slot], i)
	}
	cmds := make([]*redis.SliceCmd, len(order))
	_, err := c.Pipelined(ctx, func(p redis.Pipeliner) error {
		for gi, slot := range order {
			idx := groups[slot]
			groupKeys := make([]string, len(idx))
			for j, i := range idx {
				groupKeys[j] = keys[i]
			}
			cmds[gi] = p.MGet(ctx, groupKeys...)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	out := make([]interface{}, len(keys))
	for gi, slot := range order {
		vals, cmdErr := cmds[gi].Result()
		if cmdErr != nil {
			return nil, cmdErr
		}
		idx := groups[slot]
		if len(vals) != len(idx) {
			return nil, fmt.Errorf("playerstore: MGET 返回 %d 个值,期望 %d 个", len(vals), len(idx))
		}
		for j, i := range idx {
			out[i] = vals[j]
		}
	}
	return out, nil
}

// ErrTopologyChanged:扫描期间集群拓扑(slot 归属)发生变化。迁移窗口内 SCAN 可能漏键或重复,
// 调用方必须把这次扫描当作不完整,不得据此断言「某键不存在」。
var ErrTopologyChanged = errors.New("playerstore: 扫描期间集群拓扑变化,结果不完整")

// ErrUnsupportedClient:不是单实例 Client,也不是 ClusterClient。
var ErrUnsupportedClient = errors.New("playerstore: 不支持的客户端类型")

// ScanEachMaster 逐主节点 SCAN 完整个玩家域,对每批键调用 fn。fn 串行调用(内部加锁),但各主节点的扫描并行进行,
// 所以 fn 看到的批次顺序不确定。
//
// fail-closed:任何一个节点出错、fn 返回错误、或扫描前后拓扑不一致(ErrTopologyChanged),整体返回错误,
// 调用方不得把它当作「已扫完」。单实例客户端就是一次普通 SCAN。
func ScanEachMaster(ctx context.Context, c redis.UniversalClient, match string, count int64, fn func(keys []string) error) error {
	var mu sync.Mutex
	serial := func(keys []string) error {
		mu.Lock()
		defer mu.Unlock()
		return fn(keys)
	}
	switch cc := c.(type) {
	case *redis.Client:
		return scanNode(ctx, cc, match, count, serial)
	case *redis.ClusterClient:
		before, err := topologyFingerprint(ctx, cc)
		if err != nil {
			return err
		}
		if err := cc.ForEachMaster(ctx, func(ctx context.Context, node *redis.Client) error {
			return scanNode(ctx, node, match, count, serial)
		}); err != nil {
			return err
		}
		after, err := topologyFingerprint(ctx, cc)
		if err != nil {
			return err
		}
		if before != after {
			return ErrTopologyChanged
		}
		return nil
	default:
		return ErrUnsupportedClient
	}
}

func scanNode(ctx context.Context, node *redis.Client, match string, count int64, fn func(keys []string) error) error {
	var cursor uint64
	for {
		keys, next, err := node.Scan(ctx, cursor, match, count).Result()
		if err != nil {
			return err
		}
		if len(keys) > 0 {
			if err := fn(keys); err != nil {
				return err
			}
		}
		if next == 0 {
			return nil
		}
		cursor = next
	}
}

// topologyFingerprint 把 CLUSTER SLOTS 规约成「slot 段 → 主节点 ID」的有序串,用于比较扫描前后的拓扑。
func topologyFingerprint(ctx context.Context, cc *redis.ClusterClient) (string, error) {
	slots, err := cc.ClusterSlots(ctx).Result()
	if err != nil {
		return "", err
	}
	parts := make([]string, 0, len(slots))
	for _, s := range slots {
		master := ""
		if len(s.Nodes) > 0 {
			master = s.Nodes[0].ID
			if master == "" {
				master = s.Nodes[0].Addr
			}
		}
		parts = append(parts, fmt.Sprintf("%05d-%05d=%s", s.Start, s.End, master))
	}
	sort.Strings(parts)
	return strings.Join(parts, ","), nil
}
