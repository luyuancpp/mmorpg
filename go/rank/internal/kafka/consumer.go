// Package kafka 是 rank 的写入口:消费 match 发布的评分快照(contracts.kafka.PlayerRatingSnapshotEvent),写进榜
// (设计 docs/design/leaderboard-system.md §2.4 / §3.2)。客户端没有任何写入口,写分只走这里。
//
// 语义:事件是**快照**不是增量,写榜按 (rating_gen, games) 字典序取新(data.Board.Write 的 Lua 原子判定),
// 所以重复投递、再均衡重放、乱序都只会落成 stale,不会把榜写旧。
//
// 多实例:同一消费组 rank-rating 分摊分区;topic key = player_id,同一玩家落在同一分区、同一实例内串行处理。
package kafka

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"time"

	"rank/internal/data"
	"rank/internal/metrics"

	kafkapb "proto/contracts/kafka"

	"shared/kafkautil"
	"shared/safego"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// ConsumerConfig 是消费者参数(来自 config.Config,由 main 装配)。
type ConsumerConfig struct {
	Brokers []string
	Topic   string
	GroupID string
	// Partitions 是与 match 共享的不可变分区契约(两边 yaml 同值);rank 只校验、不扩分区。
	Partitions int32
}

// SnapshotWriter 是消费者对榜的依赖。*data.Board 满足它;单测用替身注入各类错误。
type SnapshotWriter interface {
	Write(ctx context.Context, s data.Snapshot) (data.WriteOutcome, error)
}

// messageSource 是 *kafkago.Reader 里消费循环用到的那部分,单测用替身(不连 Kafka)。
type messageSource interface {
	FetchMessage(ctx context.Context) (kafkago.Message, error)
	CommitMessages(ctx context.Context, msgs ...kafkago.Message) error
}

// 退避参数。写榜的瞬时失败(RankRedis 抖动 / 宕机)**不跳过**:跳过等于把这条快照丢到该玩家下一局才自愈,
// 而 Redis 宕机期间每一条都会被跳过 —— 卡住等 Redis 恢复才是投影消费者的正确行为(RankConsumerStalled 告警兜着)。
// 只有能确定"再试也一样"的永久错误才跳过并提交(见 handleMessage)。
const (
	writeRetryInitialBackoff = 200 * time.Millisecond
	writeRetryMaxBackoff     = 30 * time.Second
	fetchBackoff             = time.Second
)

// ErrInvalidConsumerConfig 表示消费者参数不完整(调用方缺陷,不重试)。
var ErrInvalidConsumerConfig = errors.New("rank: invalid rating snapshot consumer config")

// EnsureSnapshotTopic 校验 / 创建评分快照 topic。**RetentionMs 传 -1**:保留期只由生产方 match 声明(§2.4),
// rank 每次启动都改写会让保留期随两边的重启顺序来回变。topic 不存在时按契约分区数建出(保留期先用 broker 默认),
// match 起来后按自己的配置写正。分区数与 broker 不一致直接报错(禁止原地扩分区,见 kafkautil.EnsureTopics)。
func EnsureSnapshotTopic(cfg ConsumerConfig) error {
	if err := cfg.validate(); err != nil {
		return err
	}
	return kafkautil.EnsureTopics(cfg.Brokers, []kafkautil.TopicSpec{{
		Name:        cfg.Topic,
		Partitions:  cfg.Partitions,
		RetentionMs: -1,
	}})
}

func (cfg ConsumerConfig) validate() error {
	if len(cfg.Brokers) == 0 || cfg.Topic == "" || cfg.GroupID == "" || cfg.Partitions <= 0 {
		return fmt.Errorf("%w: brokers=%d topic=%q group=%q partitions=%d",
			ErrInvalidConsumerConfig, len(cfg.Brokers), cfg.Topic, cfg.GroupID, cfg.Partitions)
	}
	return nil
}

// StartSnapshotConsumer 校验 topic 契约后起消费 goroutine,立即返回;ctx 结束即退出。
// 返回的 done 在 goroutine 退出(reader.Close 完成、已离开消费组;CommitInterval=0,提交早已逐条同步做完)后关闭:停机时 main 有上限地等它,
// 不然进程先退出、broker 收不到离组,其余副本要等会话超时(kafka-go 默认 30s)才接管这些分区。
// 返回 error(Kafka 暂不可达、分区契约不符)时调用方负责后台重试(rank.go),读路径照常服务。
func StartSnapshotConsumer(ctx context.Context, cfg ConsumerConfig, writer SnapshotWriter) (done <-chan struct{}, err error) {
	if writer == nil {
		return nil, fmt.Errorf("%w: nil snapshot writer", ErrInvalidConsumerConfig)
	}
	if err := EnsureSnapshotTopic(cfg); err != nil {
		return nil, fmt.Errorf("ensure rating snapshot topic %s: %w", cfg.Topic, err)
	}
	// 声明 topic 的 admin 调用不认 ctx,可能卡十几秒;这期间开始停机就不再建 reader —— NewReader 会立刻发 JoinGroup
	// 触发全组再均衡,而 main 可能已经等过消费者、马上退出,其余副本要等会话超时才恢复。
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	reader := kafkago.NewReader(kafkago.ReaderConfig{
		Brokers: cfg.Brokers,
		GroupID: cfg.GroupID,
		Topic:   cfg.Topic,
		// 新消费组从最早开始:rank 晚于 match 上线、或按 §3.5 删组重建榜时,保留期内的快照都要读到。
		StartOffset: kafkago.FirstOffset,
		MinBytes:    1,
		MaxBytes:    1 << 20,
		MaxWait:     500 * time.Millisecond,
		// CommitInterval=0:每条处理完同步提交;崩溃最多重放一条,写榜的版本判定让重放落成 stale。
		CommitInterval: 0,
	})
	logx.Infof("[rank] Rating snapshot consumer started topic=%s group=%s brokers=%v", cfg.Topic, cfg.GroupID, cfg.Brokers)
	finished := make(chan struct{})
	safego.Go("rank.kafka.rating_snapshot", func() {
		defer close(finished)
		defer func() {
			if err := reader.Close(); err != nil {
				logx.Errorf("[rank] Failed to close rating snapshot reader: %v", err)
			}
		}()
		runConsumer(ctx, reader, writer, time.Now, sleepCtx)
	})
	return finished, nil
}

// runConsumer 拉取 → 处理到终态 → 提交,直到 ctx 结束。处理不到终态(ctx 在重试中结束)就不提交,下次从这条重来。
func runConsumer(ctx context.Context, src messageSource, writer SnapshotWriter, now func() time.Time, sleep func(context.Context, time.Duration) bool) {
	// 消费者起来就先写一次"最后消费时刻":否则新进程在第一条消息之前一直报 0,安静期里的一次滚动发布就会让 RankConsumerStalled 误报。
	metrics.MarkConsumerStarted(now())
	for {
		msg, err := src.FetchMessage(ctx)
		if err != nil {
			if ctx.Err() != nil || errors.Is(err, context.Canceled) {
				logx.Info("[rank] Rating snapshot consumer stopped")
				return
			}
			logx.Errorf("[rank] Failed to fetch rating snapshot; retrying after %s: %v", fetchBackoff, err)
			if !sleep(ctx, fetchBackoff) {
				return
			}
			continue
		}
		if !handleMessage(ctx, msg, writer, now, sleep) {
			return
		}
		if err := src.CommitMessages(ctx, msg); err != nil && ctx.Err() == nil {
			// 提交失败只记日志:下一条提交会覆盖;崩溃时重放到上次成功提交处,写榜的版本判定兜底。
			logx.Errorf("[rank] Failed to commit rating snapshot offset partition=%d offset=%d: %v", msg.Partition, msg.Offset, err)
		}
	}
}

// handleMessage 把一条消息处理到终态,返回 true 表示可以提交。返回 false 只有一种情况:ctx 在重试过程中结束(停机)。
//
// 终态分类(与 rank_write_total{outcome} 对应):
//   - 写入 / stale / gen_reset:成功;
//   - 反序列化失败、快照字段非法(producer 缺陷)、分数越界、该玩家的版本值被手写坏:**永久错误**,再试也一样,
//     记数、打 ERROR、跳过并提交(不让一条坏消息卡住整个分区);
//   - 其它写榜错误(RankRedis 不可达、整键类型错等):**瞬时 / 需人修**,指数退避重试直到成功或停机,不跳过。
func handleMessage(ctx context.Context, msg kafkago.Message, writer SnapshotWriter, now func() time.Time, sleep func(context.Context, time.Duration) bool) bool {
	event := &kafkapb.PlayerRatingSnapshotEvent{}
	if err := proto.Unmarshal(msg.Value, event); err != nil {
		// 与快照字段非法同记 invalid:"上游契约被破坏、永久跳过"的消息出现一条就由告警 RankWriteSkipped 报出
		// (跳过也推进 rank_last_consume_unixtime,只靠 RankConsumerStalled 会漏掉整片坏消息)。
		metrics.ObserveWrite(metrics.WriteInvalid)
		metrics.ObserveConsumeError()
		logx.Errorf("[rank] Failed to deserialize rating snapshot; skipping partition=%d offset=%d key=%s: %v",
			msg.Partition, msg.Offset, string(msg.Key), err)
		metrics.MarkConsumed(now(), 0)
		return true
	}
	if key := string(msg.Key); key != "" && key != strconv.FormatUint(event.GetPlayerId(), 10) {
		// key 契约是 player_id(同一玩家有序);不一致只记日志,以 payload 为准。
		logx.Errorf("[rank] Rating snapshot key=%s does not match player_id=%d partition=%d offset=%d",
			key, event.GetPlayerId(), msg.Partition, msg.Offset)
	}
	snap := data.Snapshot{
		PlayerID:    event.GetPlayerId(),
		RatingCenti: event.GetRatingCenti(),
		Games:       event.GetGames(),
		UpdatedAtMs: event.GetUpdatedAtMs(),
		RatingGen:   event.GetRatingGen(),
	}

	backoff := writeRetryInitialBackoff
	for attempt := 1; ; attempt++ {
		outcome, err := writer.Write(ctx, snap)
		if err == nil {
			metrics.ObserveWrite(outcomeLabel(outcome))
			if outcome == data.WriteGenReset {
				// match 侧评分键丢过(被淘汰 / 清空)后重建了代际:新代际覆盖旧条目(§3.2),此处留痕供排查淘汰策略(§3.5)。
				logx.Errorf("[rank] Rating generation reset for player=%d gen=%d games=%d battle=%d: match rating key was lost and rebuilt",
					snap.PlayerID, snap.RatingGen, snap.Games, event.GetBattleId())
			}
			metrics.MarkConsumed(now(), event.GetFinishedAtMs())
			return true
		}
		if label, permanent := permanentWriteError(err); permanent {
			metrics.ObserveWrite(label)
			metrics.ObserveConsumeError()
			logx.Errorf("[rank] Rating snapshot rejected permanently (%s); skipping partition=%d offset=%d player=%d battle=%d: %v",
				label, msg.Partition, msg.Offset, snap.PlayerID, event.GetBattleId(), err)
			metrics.MarkConsumed(now(), 0)
			return true
		}
		if ctx.Err() != nil {
			// 停机打断了写榜:不计失败、不提交,下次启动从这条重来。
			return false
		}
		metrics.ObserveWrite(metrics.WriteError)
		metrics.ObserveConsumeError()
		logx.Errorf("[rank] Failed to write rating snapshot (attempt %d); retrying after %s partition=%d offset=%d player=%d: %v",
			attempt, backoff, msg.Partition, msg.Offset, snap.PlayerID, err)
		if !sleep(ctx, backoff) {
			return false
		}
		backoff *= 2
		if backoff > writeRetryMaxBackoff {
			backoff = writeRetryMaxBackoff
		}
	}
}

// permanentWriteError 判定写榜错误是否"再试也一样",是的话返回对应的 rank_write_total outcome。
//   - ErrInvalidSnapshot:快照字段违反 match 的对外承诺(player_id / games / rating_gen 为 0);
//   - ErrOutOfRange:分数或时间超出编码范围(理论上不可能,出现即缺陷);
//   - ErrMalformedVersion:版本表里这一个玩家的值被手写坏了,只坏一个成员,要人修但不该卡住其他玩家。
//
// 三者各有自己的 outcome(不与瞬时的 error 混记),告警 RankWriteSkipped 出现一次就报。
func permanentWriteError(err error) (string, bool) {
	switch {
	case errors.Is(err, data.ErrInvalidSnapshot):
		return metrics.WriteInvalid, true
	case errors.Is(err, data.ErrOutOfRange):
		return metrics.WriteOutOfRange, true
	case errors.Is(err, data.ErrMalformedVersion):
		return metrics.WriteMalformedVersion, true
	}
	return "", false
}

// outcomeLabel 把写榜结果映射成 rank_write_total 的 outcome。
func outcomeLabel(o data.WriteOutcome) string {
	switch o {
	case data.WriteWritten:
		return metrics.WriteWritten
	case data.WriteGenReset:
		return metrics.WriteGenReset
	default:
		return metrics.WriteStale
	}
}

// sleepCtx 可中断的 sleep;ctx 结束返回 false。
func sleepCtx(ctx context.Context, d time.Duration) bool {
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return false
	case <-timer.C:
		return true
	}
}
