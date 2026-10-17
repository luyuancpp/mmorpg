package kafka

import (
	"context"
	"errors"
	"testing"

	kafkapb "proto/contracts/kafka"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// 快照消息:key = player_id 十进制串(同一玩家有序),value 是事件本体(不套信封),topic 由参数给。
func TestBuildRatingSnapshotMessages(t *testing.T) {
	events := []*kafkapb.PlayerRatingSnapshotEvent{
		{PlayerId: 42, RatingCenti: 151629, Games: 3, UpdatedAtMs: 1_800_000_000_123, RatingGen: 1_800_000_000_000, BattleId: 7, FinishedAtMs: 1_800_000_000_100},
		{PlayerId: 18446744073709551615, RatingCenti: 0, Games: 1, UpdatedAtMs: 1, RatingGen: 1, BattleId: 7},
	}
	msgs, err := BuildRatingSnapshotMessages("match-rating-snapshot", events)
	require.NoError(t, err)
	require.Len(t, msgs, 2)
	require.Equal(t, "42", string(msgs[0].Key))
	require.Equal(t, "18446744073709551615", string(msgs[1].Key), "uint64 全范围按十进制无符号写 key")
	for i, msg := range msgs {
		require.Equal(t, "match-rating-snapshot", msg.Topic)
		got := &kafkapb.PlayerRatingSnapshotEvent{}
		require.NoError(t, proto.Unmarshal(msg.Value, got))
		require.True(t, proto.Equal(events[i], got), "value 是事件本体")
	}
}

func TestBuildRatingSnapshotMessagesRejectsBadInput(t *testing.T) {
	_, err := BuildRatingSnapshotMessages("", []*kafkapb.PlayerRatingSnapshotEvent{{PlayerId: 1}})
	require.Error(t, err, "topic 为空")
	_, err = BuildRatingSnapshotMessages("t", []*kafkapb.PlayerRatingSnapshotEvent{{PlayerId: 1}, {PlayerId: 0}})
	require.Error(t, err, "player_id 为 0 整批报错,不悄悄丢一条")
	msgs, err := BuildRatingSnapshotMessages("t", nil)
	require.NoError(t, err)
	require.Empty(t, msgs)
}

// recordingWriter 记下 WriteMessages 收到的消息。
type recordingWriter struct {
	calls int
	msgs  []kafkago.Message
	err   error
}

func (w *recordingWriter) WriteMessages(_ context.Context, msgs ...kafkago.Message) error {
	w.calls++
	w.msgs = append(w.msgs, msgs...)
	return w.err
}

// 生产路径的写入:**一次**批量写,写到参数给的 topic(logic 层传的是 Config.RatingSnapshotTopic);空批不碰 writer;
// writer 报错原样带出(调用方据此保持标记 applying 重试)。
func TestWriteRatingSnapshotsBatchesToGivenTopic(t *testing.T) {
	w := &recordingWriter{}
	events := []*kafkapb.PlayerRatingSnapshotEvent{{PlayerId: 1, Games: 1, RatingGen: 1}, {PlayerId: 2, Games: 1, RatingGen: 1}}
	require.NoError(t, WriteRatingSnapshots(context.Background(), w, "custom-snapshot-topic", events))
	require.Equal(t, 1, w.calls, "一局一次批量写,不是逐人写")
	require.Len(t, w.msgs, 2)
	for _, msg := range w.msgs {
		require.Equal(t, "custom-snapshot-topic", msg.Topic)
	}

	empty := &recordingWriter{}
	require.NoError(t, WriteRatingSnapshots(context.Background(), empty, "t", nil))
	require.Zero(t, empty.calls, "空批不碰 writer")

	failing := &recordingWriter{err: errors.New("broker down")}
	require.Error(t, WriteRatingSnapshots(context.Background(), failing, "t", events))
}

// 参数守卫按哨兵错误断言(不是"有错就行"):守卫被删时,下游 sarama 拨不通也会报错,只断言 Error 会空转通过。
// 保留期 ≤ 0 尤其要拦:下游 EnsureTopics 只在 RetentionMs > 0 时下发 retention.ms,0 / -1 会被静默接受成 broker 默认。
func TestRatingSnapshotGuards(t *testing.T) {
	require.ErrorIs(t, WriteRatingSnapshots(context.Background(), nil, "t", []*kafkapb.PlayerRatingSnapshotEvent{{PlayerId: 1}}), ErrNoSnapshotWriter)
	require.ErrorIs(t, EnsureRatingSnapshotTopic(nil, "t", 3, 1), ErrNoSnapshotBrokers)
	for name, args := range map[string]struct {
		topic       string
		partitions  int32
		retentionMs int64
	}{
		"topic 为空":     {"", 3, 1},
		"分区为 0":        {"t", 0, 1},
		"分区为负":         {"t", -1, 1},
		"保留期为 0":       {"t", 3, 0},
		"保留期为 -1(不改写)": {"t", 3, -1},
	} {
		err := EnsureRatingSnapshotTopic([]string{"127.0.0.1:1"}, args.topic, args.partitions, args.retentionMs)
		require.ErrorIs(t, err, ErrInvalidSnapshotTopicSpec, name)
	}
}
