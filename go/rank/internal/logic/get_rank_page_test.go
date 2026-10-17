package logic

import (
	"context"
	"errors"
	"math"
	"testing"
	"time"

	"rank/internal/constants"
	"rank/internal/data"
	"rank/internal/metrics"
	"rank/internal/session"

	base "proto/common/base"
	dspb "proto/data_service"
	rankpb "proto/rank"

	"github.com/alicebob/miniredis/v2"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/grpc"
)

// fakeBoard 记下调用参数并返回预设结果(T7:无会话时调用计数必须为 0)。
type fakeBoard struct {
	page  data.Page
	err   error
	calls int
	got   struct {
		page, pageSize uint32
		selfID         uint64
	}
}

func (b *fakeBoard) ReadPage(_ context.Context, page, pageSize uint32, selfID uint64) (data.Page, error) {
	b.calls++
	b.got.page, b.got.pageSize, b.got.selfID = page, pageSize, selfID
	return b.page, b.err
}

// fakeNames 记下调用并返回预设名字。
type fakeNames struct {
	names map[uint64]string
	calls int
	ids   []uint64
}

func (n *fakeNames) BatchResolve(_ context.Context, ids []uint64) map[uint64]string {
	n.calls++
	n.ids = append([]uint64(nil), ids...)
	out := make(map[uint64]string)
	for _, id := range ids {
		if name, ok := n.names[id]; ok {
			out[id] = name
		}
	}
	return out
}

func sessionCtx(playerID uint64) context.Context {
	return session.WithDetails(context.Background(), &base.SessionDetails{SessionId: 1, PlayerId: playerID})
}

func matchBoardReq(page, size uint32) *rankpb.GetRankPageRequest {
	return &rankpb.GetRankPageRequest{Board: rankpb.RankBoard_RANK_BOARD_MATCH_RATING, Page: page, PageSize: size}
}

// T7:无会话 → in-band kInvalidParameter、entries 为空,**不查 Redis、不取名**。
func TestGetRankPageWithoutSessionDoesNotTouchRedis(t *testing.T) {
	board, names := &fakeBoard{}, &fakeNames{}
	resp, err := NewRankLogic(board, names, time.Second).GetRankPage(context.Background(), matchBoardReq(1, 20))
	require.NoError(t, err, "业务拒绝走 in-band,不回 gRPC error")
	require.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId())
	require.Empty(t, resp.GetEntries())
	require.Zero(t, board.calls, "先查后拒的实现也会回同一个码:必须断言没碰榜")
	require.Zero(t, names.calls)
}

// 未知榜类型(含 UNSPECIFIED)→ kInvalidParameter,不查 Redis。
func TestGetRankPageRejectsUnknownBoard(t *testing.T) {
	for _, b := range []rankpb.RankBoard{rankpb.RankBoard_RANK_BOARD_UNSPECIFIED, rankpb.RankBoard(99)} {
		board := &fakeBoard{}
		resp, err := NewRankLogic(board, nil, time.Second).GetRankPage(sessionCtx(42), &rankpb.GetRankPageRequest{Board: b})
		require.NoError(t, err)
		require.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId(), "board=%d", b)
		require.Zero(t, board.calls)
	}
}

func TestClampPage(t *testing.T) {
	for _, tc := range []struct{ page, size, wantPage, wantSize uint32 }{
		{0, 0, 1, DefaultPageSize},
		{3, 0, 3, DefaultPageSize},
		{1, MaxPageSize, 1, MaxPageSize},
		{1, MaxPageSize + 1, 1, MaxPageSize},
		{math.MaxUint32, math.MaxUint32, math.MaxUint32, MaxPageSize},
	} {
		gotPage, gotSize := ClampPage(tc.page, tc.size)
		require.Equal(t, tc.wantPage, gotPage, "page=%d size=%d", tc.page, tc.size)
		require.Equal(t, tc.wantSize, gotSize, "page=%d size=%d", tc.page, tc.size)
	}
}

// 正常路径:钳制后的页码 / 页长与会话里的 player_id 交给榜;一页 + self 一次批量取名;字段逐个映射。
func TestGetRankPageMapsPageAndSelf(t *testing.T) {
	board := &fakeBoard{page: data.Page{
		Total: 3, Page: 1, PageSize: 20, PageCount: 1,
		Entries: []data.Entry{
			{Rank: 1, PlayerID: 7, RatingCenti: 160000, Games: 9},
			{Rank: 2, PlayerID: 42, RatingCenti: 151629, Games: 3},
			{Rank: 3, PlayerID: 8, RatingCenti: 150000, Games: 1},
		},
		Self: data.Entry{Rank: 2, PlayerID: 42, RatingCenti: 151629, Games: 3},
	}}
	names := &fakeNames{names: map[uint64]string{7: "甲", 42: "乙"}}
	resp, err := NewRankLogic(board, names, time.Second).GetRankPage(sessionCtx(42), matchBoardReq(0, 0))
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	require.Equal(t, uint32(1), board.got.page, "page 0 视为 1")
	require.Equal(t, DefaultPageSize, board.got.pageSize, "page_size 0 取默认")
	require.Equal(t, uint64(42), board.got.selfID, "self 用会话里的 player_id(D-9)")
	require.Equal(t, 1, names.calls, "一页 + self 只取一次名")

	require.Equal(t, uint32(3), resp.GetTotalCount())
	require.Equal(t, uint32(1), resp.GetPage())
	require.Equal(t, uint32(20), resp.GetPageSize())
	require.Equal(t, uint32(1), resp.GetPageCount())
	require.Len(t, resp.GetEntries(), 3)
	require.Equal(t, &rankpb.RankEntry{Rank: 1, PlayerId: 7, Name: "甲", RatingCenti: 160000, Games: 9}, resp.GetEntries()[0])
	require.Equal(t, "", resp.GetEntries()[2].GetName(), "查不到名字留空")
	require.Equal(t, uint32(2), resp.GetSelf().GetRank())
	require.Equal(t, "乙", resp.GetSelf().GetName())
}

// T6:自己确定不在榜上 → self.rank = 0、error_message 为空,entries 照常返回;不为自己取名。
func TestGetRankPageSelfNotRanked(t *testing.T) {
	board := &fakeBoard{page: data.Page{
		Total: 1, Page: 1, PageSize: 20, PageCount: 1,
		Entries: []data.Entry{{Rank: 1, PlayerID: 7, RatingCenti: 160000, Games: 9}},
	}}
	names := &fakeNames{}
	resp, err := NewRankLogic(board, names, time.Second).GetRankPage(sessionCtx(99), matchBoardReq(1, 20))
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage(), "未上榜不是错误")
	require.Len(t, resp.GetEntries(), 1)
	require.NotNil(t, resp.GetSelf())
	require.Zero(t, resp.GetSelf().GetRank())
	require.Zero(t, resp.GetSelf().GetPlayerId(), "未上榜时 self 其余字段为空")
	require.Equal(t, []uint64{7}, names.ids, "未上榜不必查自己的名字")
}

// 读榜失败 → in-band kServiceUnavailable(fault),不取名。
func TestGetRankPageBoardFailureIsFault(t *testing.T) {
	board := &fakeBoard{err: errors.New("redis: connection refused")}
	names := &fakeNames{}
	resp, err := NewRankLogic(board, names, time.Second).GetRankPage(sessionCtx(42), matchBoardReq(1, 20))
	require.NoError(t, err)
	require.Equal(t, constants.ErrServiceUnavailable, resp.GetErrorMessage().GetId())
	require.Empty(t, resp.GetEntries())
	require.Zero(t, names.calls)
	for _, p := range resp.GetErrorMessage().GetParameters() {
		require.NotContains(t, p, "connection refused", "原始错误只进日志,不出信封")
	}
}

// 空榜:page_count = 1、page = 1、entries 为空,不是错误。
func TestGetRankPageEmptyBoard(t *testing.T) {
	board := &fakeBoard{page: data.Page{Total: 0, Page: 1, PageSize: 20, PageCount: 1}}
	resp, err := NewRankLogic(board, &fakeNames{}, time.Second).GetRankPage(sessionCtx(42), matchBoardReq(5, 20))
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	require.Empty(t, resp.GetEntries())
	require.Equal(t, uint32(1), resp.GetPageCount())
	require.Equal(t, uint32(1), resp.GetPage())
}

// T5(经真 Lua):越界页码与溢出输入都钳到末页,响应 page 是钳制后的值。
func TestGetRankPageClampsToLastPageOnRealBoard(t *testing.T) {
	mr := miniredis.RunT(t)
	board := data.NewRatingBoard(redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"}))
	for i := uint64(1); i <= 45; i++ {
		_, err := board.Write(context.Background(), data.Snapshot{PlayerID: i, RatingCenti: uint32(100000 + i), Games: 1,
			UpdatedAtMs: 1_800_000_000_000 + i*1000, RatingGen: 1})
		require.NoError(t, err)
	}
	l := NewRankLogic(board, nil, time.Second)
	for _, tc := range []struct{ page, size uint32 }{{214748366, 20}, {math.MaxUint32, math.MaxUint32}, {4, 20}} {
		resp, err := l.GetRankPage(sessionCtx(1), matchBoardReq(tc.page, tc.size))
		require.NoError(t, err)
		require.Nil(t, resp.GetErrorMessage())
		_, size := ClampPage(tc.page, tc.size)
		wantPageCount := uint32((45 + size - 1) / size)
		require.Equal(t, wantPageCount, resp.GetPageCount(), "page=%d size=%d", tc.page, tc.size)
		require.Equal(t, wantPageCount, resp.GetPage(), "越界钳到末页 page=%d size=%d", tc.page, tc.size)
		require.NotEmpty(t, resp.GetEntries(), "末页有数据")
		require.Equal(t, uint32(45), resp.GetEntries()[len(resp.GetEntries())-1].GetRank(), "末页最后一名是第 45 名")
	}
	// 自己(player 1,分最低)排第 45。
	resp, err := l.GetRankPage(sessionCtx(1), matchBoardReq(1, 5))
	require.NoError(t, err)
	require.Equal(t, uint32(45), resp.GetSelf().GetRank())
	require.Equal(t, uint32(45), resp.GetTotalCount())
}

// ── 名字补全(T8)──────────────────────────────────────────────

// fakeDataService 只实现 BatchGetPlayerName(内嵌接口满足其余方法;调到别的方法会 nil panic,即测试缺陷)。
type fakeDataService struct {
	dspb.DataServiceClient
	resp  *dspb.BatchGetPlayerNameResponse
	err   error
	calls int
	ids   []uint64
}

func (f *fakeDataService) BatchGetPlayerName(_ context.Context, in *dspb.BatchGetPlayerNameRequest, _ ...grpc.CallOption) (*dspb.BatchGetPlayerNameResponse, error) {
	f.calls++
	f.ids = append([]uint64(nil), in.GetPlayerIds()...)
	return f.resp, f.err
}

func nameLookupCount(t *testing.T, outcome string) float64 {
	t.Helper()
	metrics.Register()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, f := range families {
		if f.GetName() != "rank_name_lookup_total" {
			continue
		}
		for _, m := range f.GetMetric() {
			for _, lp := range m.GetLabel() {
				if lp.GetName() == "outcome" && lp.GetValue() == outcome {
					return m.GetCounter().GetValue()
				}
			}
		}
	}
	t.Fatalf("rank_name_lookup_total{outcome=%q} not found", outcome)
	return 0
}

// T8:取名失败 → 空 map(不报错),error 计数 +1;经 GetRankPage 时榜照返回、名字留空。
func TestNameLookupFailureIsFailOpen(t *testing.T) {
	before := nameLookupCount(t, metrics.NameLookupError)
	ds := &fakeDataService{err: errors.New("data_service unavailable")}
	names := NewDataServiceNames(ds, 0)
	require.Empty(t, names.BatchResolve(context.Background(), []uint64{7, 8}))
	require.Equal(t, before+1, nameLookupCount(t, metrics.NameLookupError))

	board := &fakeBoard{page: data.Page{Total: 1, Page: 1, PageSize: 20, PageCount: 1,
		Entries: []data.Entry{{Rank: 1, PlayerID: 7, RatingCenti: 1, Games: 1}}}}
	resp, err := NewRankLogic(board, names, time.Second).GetRankPage(sessionCtx(42), matchBoardReq(1, 20))
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage(), "名字是展示数据,取名失败不升级成读榜失败")
	require.Len(t, resp.GetEntries(), 1)
	require.Equal(t, "", resp.GetEntries()[0].GetName())
	require.Equal(t, before+2, nameLookupCount(t, metrics.NameLookupError))
}

// 去重、丢 0、空串名字不进结果;成功计 ok。
func TestNameLookupDedupesAndDropsEmpty(t *testing.T) {
	before := nameLookupCount(t, metrics.NameLookupOK)
	ds := &fakeDataService{resp: &dspb.BatchGetPlayerNameResponse{Names: map[uint64]string{7: "甲", 8: ""}}}
	got := NewDataServiceNames(ds, 0).BatchResolve(context.Background(), []uint64{7, 0, 8, 7})
	require.Equal(t, []uint64{7, 8}, ds.ids, "去重、丢 0、保持首次出现顺序")
	require.Equal(t, map[uint64]string{7: "甲"}, got, "空串与没登记同义")
	require.Equal(t, before+1, nameLookupCount(t, metrics.NameLookupOK))
}

// 请求 ctx 已结束:不发 RPC,也不计 error(那不是 data_service 的问题)。
func TestNameLookupSkipsWhenContextDone(t *testing.T) {
	before := nameLookupCount(t, metrics.NameLookupError)
	ds := &fakeDataService{err: errors.New("should not be called")}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	require.Empty(t, NewDataServiceNames(ds, 0).BatchResolve(ctx, []uint64{7}))
	require.Zero(t, ds.calls)
	require.Equal(t, before, nameLookupCount(t, metrics.NameLookupError))

	var nilResolver *DataServiceNames
	require.NotNil(t, nilResolver.BatchResolve(context.Background(), []uint64{7}), "nil 接收者也返回非 nil map")
}
