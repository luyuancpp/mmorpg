package kafka

// 排行榜评分快照的 Kafka 出口(docs/design/leaderboard-system.md §2.4):match 在一局评分入账后,
// 把全员的当前评分快照(contracts.kafka.PlayerRatingSnapshotEvent)一次批量写到快照 topic,rank 消费它写榜。
//
// 本文件只做 proto 构造与以参数接收 writer 的写入,**不 import svc / logic**(svc → kafka → logic → svc 会成环,
// 见 result_consumer.go 顶部);发布调用点在 logic.ApplyBattleResult,经包级接缝 ratingSnapshotPublishFn 调到这里。

import (
	"context"
	"errors"
	"fmt"
	"strconv"

	kafkapb "proto/contracts/kafka"

	"shared/kafkautil"

	kafkago "github.com/segmentio/kafka-go"
	"google.golang.org/protobuf/proto"
)

// BuildRatingSnapshotMessages 把快照组成 Kafka 消息:key = player_id 十进制串(AGENTS §7 #3:同一玩家有序,
// writer 的 Hash 回落按 key 分区),value = 事件的 proto 字节(不套信封,照 match-results)。
// player_id 为 0 的事件是调用方缺陷,整批报错而不是悄悄丢掉一条。
func BuildRatingSnapshotMessages(topic string, events []*kafkapb.PlayerRatingSnapshotEvent) ([]kafkago.Message, error) {
	if topic == "" {
		return nil, errors.New("评分快照 topic 为空")
	}
	msgs := make([]kafkago.Message, 0, len(events))
	for _, ev := range events {
		if ev.GetPlayerId() == 0 {
			return nil, fmt.Errorf("评分快照 player_id 为 0 battle=%d", ev.GetBattleId())
		}
		value, err := proto.Marshal(ev)
		if err != nil {
			return nil, fmt.Errorf("序列化评分快照失败 player=%d battle=%d: %w", ev.GetPlayerId(), ev.GetBattleId(), err)
		}
		msgs = append(msgs, kafkago.Message{
			Topic: topic,
			Key:   []byte(strconv.FormatUint(ev.GetPlayerId(), 10)),
			Value: value,
		})
	}
	return msgs, nil
}

// 参数非法的哨兵错误:调用方(与测试)按 errors.Is 区分"参数不对"与"网络 / broker 失败"。
var (
	// ErrNoSnapshotWriter:writer 为空(Kafka.Brokers 没配)。
	ErrNoSnapshotWriter = errors.New("Kafka writer 为空")
	// ErrNoSnapshotBrokers:声明 topic 时 Kafka.Brokers 为空。
	ErrNoSnapshotBrokers = errors.New("Kafka.Brokers 为空,无法声明评分快照 topic")
	// ErrInvalidSnapshotTopicSpec:topic 名为空、分区数 ≤ 0 或保留期 ≤ 0。保留期 ≤ 0 时下游 EnsureTopics 会**静默不设**
	// retention.ms(只在 RetentionMs > 0 时下发),所以这条守卫是"保留期只由 match 显式声明"的唯一防线。
	ErrInvalidSnapshotTopicSpec = errors.New("评分快照 topic 参数非法")
)

// SnapshotWriter 是批量写的最小接口(*kafkago.Writer 满足它),测试用替身核对写到了哪个 topic。
type SnapshotWriter interface {
	WriteMessages(ctx context.Context, msgs ...kafkago.Message) error
}

// WriteRatingSnapshots 一次批量写一局的全部快照(同步;生产 writer 是 svcCtx.Kafka:RequireOne、MaxAttempts=1)。
// 批量而不是逐人:5V5 逐人同步写最坏 10 × 5s,会把 Kafka 延迟整个塞进评分循环(设计 §2.4)。
// w 为接口:调用方传 *kafkago.Writer 前必须自己判 nil(装进接口的 nil 指针 != nil,这里拦不住)。
func WriteRatingSnapshots(ctx context.Context, w SnapshotWriter, topic string, events []*kafkapb.PlayerRatingSnapshotEvent) error {
	if w == nil {
		return ErrNoSnapshotWriter
	}
	msgs, err := BuildRatingSnapshotMessages(topic, events)
	if err != nil {
		return err
	}
	if len(msgs) == 0 {
		return nil
	}
	if err := w.WriteMessages(ctx, msgs...); err != nil {
		return fmt.Errorf("写评分快照到 %s 失败(%d 条): %w", topic, len(msgs), err)
	}
	return nil
}

// EnsureRatingSnapshotTopic 声明快照 topic 的分区契约与保留期。**保留期只由 match 声明**(rank 侧 Ensure 传
// RetentionMs: -1 不改写);分区数与 broker 现状不一致时报错,让部署问题在启动时暴露。
// writer 开着 AllowAutoTopicCreation:没人 Ensure 的话 broker 会按默认分区数自动建 topic,破坏契约,所以 match 启动时必须调它。
func EnsureRatingSnapshotTopic(brokers []string, topic string, partitions int32, retentionMs int64) error {
	if len(brokers) == 0 {
		return ErrNoSnapshotBrokers
	}
	if topic == "" || partitions <= 0 || retentionMs <= 0 {
		return fmt.Errorf("%w: topic=%q partitions=%d retentionMs=%d", ErrInvalidSnapshotTopicSpec, topic, partitions, retentionMs)
	}
	if err := kafkautil.EnsureTopics(brokers, []kafkautil.TopicSpec{{
		Name:        topic,
		Partitions:  partitions,
		RetentionMs: retentionMs,
	}}); err != nil {
		return fmt.Errorf("确保评分快照 topic %s 存在失败: %w", topic, err)
	}
	return nil
}
