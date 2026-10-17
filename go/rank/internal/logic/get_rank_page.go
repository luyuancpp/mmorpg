// Package logic 是 rank 的业务层:读路径 GetRankPage(设计 docs/design/leaderboard-system.md §4)与名字补全。
// 写路径在 internal/kafka(消费 match 的评分快照),不经这里。
package logic

import (
	"context"
	"fmt"
	"math"
	"time"

	"rank/internal/constants"
	"rank/internal/data"
	"rank/internal/metrics"
	"rank/internal/session"

	base "proto/common/base"
	rankpb "proto/rank"

	"github.com/zeromicro/go-zero/core/logx"
)

// 页长口径(§4.2,照 jubaozhai BrowseListingsRequest):0 取默认,超过上限按上限钳制、不报错。
const (
	DefaultPageSize uint32 = 20
	MaxPageSize     uint32 = 50
)

// BoardReader 是读路径对榜的依赖。*data.Board 满足它;单测用替身计调用次数(T7:无会话时**不查 Redis**)。
type BoardReader interface {
	ReadPage(ctx context.Context, page, pageSize uint32, selfID uint64) (data.Page, error)
}

// RankLogic 是 ClientPlayerRank 的业务实现。构造后只读,可并发调用。
type RankLogic struct {
	board BoardReader
	names NameResolver
	// budget 是一次请求内全部 I/O(读榜 + 取名)共用的预算 = zrpc Timeout − 回包余量(config.RequestBudget)。
	budget time.Duration
	now    func() time.Time
}

// NewRankLogic。names 可为 nil(名字全部留空);budget <= 0 时不另设预算(只靠调用方 ctx)。
func NewRankLogic(board BoardReader, names NameResolver, budget time.Duration) *RankLogic {
	return &RankLogic{board: board, names: names, budget: budget, now: time.Now}
}

// ClampPage 把请求的页码 / 页长钳到合法区间:page 0 → 1;page_size 0 → DefaultPageSize、> MaxPageSize → MaxPageSize。
// 超过末页的页码由读 Lua 按榜长钳到末页(§4.2:越界钳到末页,不是返回空页),响应里回的是钳制后的值。
func ClampPage(page, pageSize uint32) (uint32, uint32) {
	if page == 0 {
		page = 1
	}
	switch {
	case pageSize == 0:
		pageSize = DefaultPageSize
	case pageSize > MaxPageSize:
		pageSize = MaxPageSize
	}
	return page, pageSize
}

// GetRankPage 拉一页 + 调用者自己的名次(§4.2 合成一个 RPC 的理由:客户端回包按 message_id FIFO 匹配,同功能单请求在途)。
//
// 结果分类(与 rank_read_total{outcome} 一一对应):
//   - 没有会话身份 / 榜类型不认识 → in-band kInvalidParameter(rejected),**不查 Redis**(T7);
//   - 读榜失败 → in-band kServiceUnavailable(fault,Tip.xlsx 的 fault 列);
//   - 其余 → ok。自己未上榜时 self.rank = 0、error_message 为空 —— 不是错误(§4.2)。
//
// 名字补全失败不影响结果(fail-open,§4.3),只是名字留空。
func (l *RankLogic) GetRankPage(ctx context.Context, req *rankpb.GetRankPageRequest) (*rankpb.GetRankPageResponse, error) {
	start := l.now()
	outcome := metrics.ReadOK
	defer func() { metrics.ObserveRead(metrics.MethodGetRankPage, outcome, l.now().Sub(start)) }()
	reject := func(readOutcome string, code uint32, msg string) (*rankpb.GetRankPageResponse, error) {
		outcome = readOutcome
		return &rankpb.GetRankPageResponse{ErrorMessage: tipErr(code, msg)}, nil
	}

	// D-9:"我是谁"只从 gate 注入的会话取,请求体里根本没有 player_id。没有会话 = 内部调用打到了只给客户端的方法。
	me, ok := session.ClientPlayerID(ctx)
	if !ok {
		logx.WithContext(ctx).Errorf("[rank] GetRankPage missing session identity: method accepts only client requests forwarded through gate -> router (D-9)")
		return reject(metrics.ReadRejected, constants.ErrInvalidParameter, "missing session identity")
	}
	if req.GetBoard() != rankpb.RankBoard_RANK_BOARD_MATCH_RATING {
		return reject(metrics.ReadRejected, constants.ErrInvalidParameter, fmt.Sprintf("unknown board %d", int32(req.GetBoard())))
	}
	page, pageSize := ClampPage(req.GetPage(), req.GetPageSize())

	ctx, cancel := l.withBudget(ctx)
	defer cancel()
	p, err := l.board.ReadPage(ctx, page, pageSize, me)
	if err != nil {
		// 原始 err 只进日志;回给客户端的只有码与一句固定文案(Redis 地址、Lua 细节不出信封,AGENTS §11.3)。
		logx.WithContext(ctx).Errorf("[rank] GetRankPage board read failed page=%d page_size=%d: %v", page, pageSize, err)
		return reject(metrics.ReadFault, constants.ErrServiceUnavailable, "leaderboard unavailable")
	}

	// 一页 + self 一次批量取名;自己未上榜时 self 其余字段为空,不必查自己的名字。
	ids := make([]uint64, 0, len(p.Entries)+1)
	for _, e := range p.Entries {
		ids = append(ids, e.PlayerID)
	}
	if p.Self.Rank != 0 {
		ids = append(ids, p.Self.PlayerID)
	}
	var names map[uint64]string
	if l.names != nil && len(ids) > 0 {
		names = l.names.BatchResolve(ctx, ids)
	}

	resp := &rankpb.GetRankPageResponse{
		Entries:    make([]*rankpb.RankEntry, 0, len(p.Entries)),
		TotalCount: clampUint32(p.Total),
		Page:       p.Page,
		PageSize:   p.PageSize,
		PageCount:  p.PageCount,
		Self:       &rankpb.RankEntry{},
	}
	for _, e := range p.Entries {
		resp.Entries = append(resp.Entries, toProtoEntry(e, names))
	}
	if p.Self.Rank != 0 {
		resp.Self = toProtoEntry(p.Self, names)
	}
	return resp, nil
}

// withBudget 给一次请求套上整请求预算;budget <= 0 时只沿用调用方 ctx(配置校验保证生产路径恒为正)。
func (l *RankLogic) withBudget(ctx context.Context) (context.Context, context.CancelFunc) {
	if l.budget <= 0 {
		return ctx, func() {}
	}
	return context.WithTimeout(ctx, l.budget)
}

// toProtoEntry 把一行榜数据转成协议条目;names 为 nil 时名字留空(对 nil map 取值得到 "")。
func toProtoEntry(e data.Entry, names map[uint64]string) *rankpb.RankEntry {
	return &rankpb.RankEntry{
		Rank:        e.Rank,
		PlayerId:    e.PlayerID,
		Name:        names[e.PlayerID],
		RatingCenti: e.RatingCenti,
		Games:       e.Games,
	}
}

// clampUint32 把榜长转成协议的 uint32(榜长到不了 2^32,钳制只是不让回卷)。
func clampUint32(v uint64) uint32 {
	if v > math.MaxUint32 {
		return math.MaxUint32
	}
	return uint32(v)
}

// tipErr 构造 in-band 错误(形状同 mail / friend:码 + 一句给日志 / 排障看的英文参数)。
func tipErr(id uint32, msg string) *base.TipInfoMessage {
	return &base.TipInfoMessage{Id: id, Parameters: []string{msg}}
}
