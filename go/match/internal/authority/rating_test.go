package authority

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
	kafkapb "proto/contracts/kafka"
	rankpb "proto/rank"
)

type rankReceiver struct {
	rankpb.RankAdminClient
	batches     map[uint64]*rankpb.SubmitRankScoresRequest
	unknownOnce bool
}

func (r *rankReceiver) OpenRankGeneration(context.Context, *rankpb.OpenRankGenerationRequest, ...grpc.CallOption) (*rankpb.OpenRankGenerationResponse, error) {
	return &rankpb.OpenRankGenerationResponse{BoardId: 10}, nil
}
func (r *rankReceiver) SubmitRankScores(_ context.Context, request *rankpb.SubmitRankScoresRequest, _ ...grpc.CallOption) (*rankpb.SubmitRankScoresResponse, error) {
	if old := r.batches[request.SourceCursor]; old != nil {
		a, b := proto.Clone(old).(*rankpb.SubmitRankScoresRequest), proto.Clone(request).(*rankpb.SubmitRankScoresRequest)
		a.Auth = nil
		b.Auth = nil
		if !proto.Equal(a, b) {
			return nil, errors.New("重放载荷不同")
		}
	} else {
		r.batches[request.SourceCursor] = proto.Clone(request).(*rankpb.SubmitRankScoresRequest)
	}
	if r.unknownOnce {
		r.unknownOnce = false
		return nil, context.DeadlineExceeded
	}
	return &rankpb.SubmitRankScoresResponse{CommittedCursor: request.SourceCursor}, nil
}
func TestRatingTiDBTransactionOutboxReplayAndStopBarrier(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	event := &kafkapb.BattleResultEvent{BattleId: 100, MatchMode: 3}
	applied, e := s.ApplyRating(ctx, 100, event, 3, []uint64{1}, []uint64{2}, 1)
	require.NoError(t, e)
	require.True(t, applied)
	rating, e := s.Rating(ctx, 1)
	require.NoError(t, e)
	require.Equal(t, int64(151600), rating)
	applied, e = s.ApplyRating(ctx, 100, event, 3, []uint64{1}, []uint64{2}, 1)
	require.NoError(t, e)
	require.False(t, applied)
	changed := proto.Clone(event).(*kafkapb.BattleResultEvent)
	changed.BattleConfigId = 7
	_, e = s.ApplyRating(ctx, 100, changed, 3, []uint64{1}, []uint64{2}, 1)
	require.Error(t, e)
	receiver := &rankReceiver{batches: map[uint64]*rankpb.SubmitRankScoresRequest{}, unknownOnce: true}
	const key = "fixture-only-rank-producer-key-0000"
	require.ErrorIs(t, s.PublishRankOnce(ctx, receiver, key), context.DeadlineExceeded)
	var committed uint64
	require.NoError(t, s.DB.QueryRow("SELECT committed_cursor FROM match_rank_source WHERE source_epoch=1").Scan(&committed))
	require.Zero(t, committed)
	require.NoError(t, s.PublishRankOnce(ctx, receiver, key))
	require.Len(t, receiver.batches, 1)
	require.NoError(t, s.DB.QueryRow("SELECT committed_cursor FROM match_rank_source WHERE source_epoch=1").Scan(&committed))
	require.Equal(t, uint64(1), committed)
	final, e := s.StopRankSource(ctx, 1)
	require.NoError(t, e)
	require.Equal(t, uint64(1), final)
	_, e = s.ApplyRating(ctx, 101, &kafkapb.BattleResultEvent{BattleId: 101, MatchMode: 3}, 3, []uint64{1}, []uint64{2}, 1)
	require.Error(t, e)
	rating, e = s.Rating(ctx, 1)
	require.NoError(t, e)
	require.Equal(t, int64(151600), rating, "停写拒绝必须回滚评分和入账标记")
	var count int
	require.NoError(t, s.DB.QueryRow("SELECT COUNT(*) FROM match_rating_applied WHERE battle_id=101").Scan(&count))
	require.Zero(t, count)
}
