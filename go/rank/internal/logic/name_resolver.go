package logic

// 榜上展示名的批量补全(设计 docs/design/leaderboard-system.md §4.3;做法照 go/guild/internal/logic/player_name_resolver.go)。
//
// 两条纪律:
//
//  1. **名字是展示数据,读取侧 fail-open**。真源是 data_service 全局库的 player_name 表,rank 不落库、不缓存。
//     拿不到名字时榜照常返回、名字留空;把一次取名失败升级成 GetRankPage 失败,等于让纯展示依赖决定排行榜的可用性。
//     风险由 rank_name_lookup_total{outcome="error"} 承担(AGENTS §11.3:fail-open 必须配指标)。
//  2. **一页 + self 只发一次批量查询**,不在循环里逐个查(公会榜逐条 GetGuild 是 N+1)。

import (
	"context"
	"time"

	"rank/internal/metrics"

	dspb "proto/data_service"

	"github.com/zeromicro/go-zero/core/logx"
)

// NameResolver 批量取展示名。
//
// 契约(实现必须全部满足):
//   - fail-open:任何错误(RPC 失败、超时、依赖未接线)都返回空 map,**不返回 error**;
//   - 查不到名字的 id 不出现在返回的 map 里;
//   - 入参里的 0 与重复 id 由实现自己处理;
//   - 永远返回非 nil 的 map,返回值只读。
type NameResolver interface {
	BatchResolve(ctx context.Context, playerIDs []uint64) map[uint64]string
}

// DefaultNameLookupTimeout 是一次 BatchGetPlayerName 的预算(§4.3,与 guild 的 DefaultPlayerNameLookupTimeout 同值)。
// 这是上限不是保底:lookupCtx 派生自请求 ctx,实际最多等 min(800ms, 整请求剩余预算)。
const DefaultNameLookupTimeout = 800 * time.Millisecond

// nameBatchLimit 与 data_service 的 store.PlayerNameBatchLimit 同值(超限请求被回 InvalidArgument 而不是截断)。
// 客户端路径到不了:一页 ≤ MaxPageSize(50)+ self 1 个。
const nameBatchLimit = 500

// DataServiceNames 用 data_service.BatchGetPlayerName 实现 NameResolver。
type DataServiceNames struct {
	client  dspb.DataServiceClient
	timeout time.Duration
}

// NewDataServiceNames。timeout <= 0 用 DefaultNameLookupTimeout;client 为 nil 也能构造(BatchResolve 恒回空 map)。
func NewDataServiceNames(client dspb.DataServiceClient, timeout time.Duration) *DataServiceNames {
	if timeout <= 0 {
		timeout = DefaultNameLookupTimeout
	}
	return &DataServiceNames{client: client, timeout: timeout}
}

// BatchResolve 见 NameResolver 的契约。nil 接收者也安全(装进接口后 != nil,判空拦不住)。
func (r *DataServiceNames) BatchResolve(ctx context.Context, playerIDs []uint64) map[uint64]string {
	names := make(map[uint64]string, len(playerIDs))
	if r == nil || r.client == nil {
		return names
	}
	ids := uniqueNonZero(playerIDs)
	if len(ids) == 0 {
		return names
	}
	if len(ids) > nameBatchLimit {
		logx.Errorf("[rank] Player name lookup got %d distinct ids, over the batch limit %d; only the first %d are resolved",
			len(ids), nameBatchLimit, nameBatchLimit)
		ids = ids[:nameBatchLimit]
	}
	// 请求 ctx 已经结束就不发 RPC:玩家断线,或整请求预算被前面的读榜吃光。此时的"失败"与 data_service 无关,
	// 只记 Info、不计 rank_name_lookup_total{outcome="error"},保住该计数器"向 data_service 取名出了问题"的含义。
	if err := ctx.Err(); err != nil {
		logx.WithContext(ctx).Infof("[rank] Player name lookup skipped (n=%d): request context already done before the RPC: %v", len(ids), err)
		return names
	}

	lookupCtx, cancel := context.WithTimeout(ctx, r.timeout)
	defer cancel()
	resp, err := r.client.BatchGetPlayerName(lookupCtx, &dspb.BatchGetPlayerNameRequest{PlayerIds: ids})
	if err != nil {
		// 只记条数不记 id 列表(player_id 不进日志批量串,也不进指标 label)。
		logx.WithContext(ctx).Errorf("[rank] Player name lookup failed (n=%d); names are left empty: %v", len(ids), err)
		metrics.ObserveNameLookup(metrics.NameLookupError)
		return names
	}
	metrics.ObserveNameLookup(metrics.NameLookupOK)
	// 拷一份:返回值要非 nil,也不把 pb 内部的 map 交给调用方。空串与"没登记"同义,不放进结果。
	for id, name := range resp.GetNames() {
		if name != "" {
			names[id] = name
		}
	}
	return names
}

// uniqueNonZero 去重并丢掉 0,保持首次出现的顺序。
func uniqueNonZero(ids []uint64) []uint64 {
	seen := make(map[uint64]struct{}, len(ids))
	out := make([]uint64, 0, len(ids))
	for _, id := range ids {
		if id == 0 {
			continue
		}
		if _, dup := seen[id]; dup {
			continue
		}
		seen[id] = struct{}{}
		out = append(out, id)
	}
	return out
}
