// Package kafka 是 data_service 的两条落库消费者:
//
//   - transaction_log_topic → transaction_log 表(攒批 INSERT IGNORE,tx_id 主键天然幂等);
//   - player_snapshot_topic → player_snapshot 表(按分区并发、可配批量(默认逐条),按 snapshot_guid 去重,source=1;
//     见 snapshot_worker.go)。
//
// 生产者是 C++ scene(全局单 topic、裸 proto、key = player_id)。形态照
// go/match/internal/kafka/result_consumer.go:kafka-go reader、FirstOffset、同步提交。
//
// 与 match 的"软数据、重试用尽就跳过"**相反**:这两张表是审计数据,DB 写不进去时
// 消费者有界重试后**停下且不提交 offset**(宁可积压不许丢),并把健康标志翻成不健康。
// 只有"消息本身坏了"(解不出 proto / 主键为 0 / 超过 MEDIUMBLOB 上限)才跳过并提交:
// 那种消息留在 topic 里只会每次重启都撞一遍。
//
// 本包不 import internal/logic / svc(避免成环),落库能力以小接口注入,单测用 fake。
package kafka

import (
	"context"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"data_service/internal/metrics"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
)

// MessageReader 是 *kafkago.Reader 的最小面,便于单测注入 fake。
type MessageReader interface {
	FetchMessage(ctx context.Context) (kafkago.Message, error)
	CommitMessages(ctx context.Context, msgs ...kafkago.Message) error
	Close() error
}

// Health 是一条消费者的健康标志。Healthy()=false 有两种含义,靠 Stopped() 区分:
// 还没启动 / 已正常退出,或者因 DB 故障停下(StoppedOnError)。
type Health struct {
	name    string
	running atomic.Bool
	failed  atomic.Bool

	mu      sync.Mutex
	lastErr error
}

func newHealth(name string) *Health {
	return &Health{name: name}
}

// Name 是消费者名("transaction_log" | "player_snapshot"),同时是指标 label。
func (h *Health) Name() string { return h.name }

// Healthy 表示消费循环仍在跑且没有因故障停下。
func (h *Health) Healthy() bool { return h.running.Load() && !h.failed.Load() }

// StoppedOnError 表示消费循环因 DB 故障主动停下(offset 未提交,重启即从断点补消费)。
func (h *Health) StoppedOnError() bool { return h.failed.Load() }

// LastError 是最近一次让消费者停下的错误;nil 表示从未故障。
func (h *Health) LastError() error {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.lastErr
}

// markRunning 进入消费循环。**同时清掉上一轮的故障标记**:supervisor 重建 reader 并
// 恢复消费之后,进程确实又健康了。不清的话 Healthy() 会因为一次早已恢复的 DB 抖动
// 在整个进程生命周期里恒为 false,把"现在真的停了"这个信号淹掉。
func (h *Health) markRunning() {
	h.mu.Lock()
	h.lastErr = nil
	h.mu.Unlock()
	h.failed.Store(false)
	h.running.Store(true)
	metrics.SetKafkaConsumerUp(h.name, true)
}

func (h *Health) markStopped() {
	h.running.Store(false)
	metrics.SetKafkaConsumerUp(h.name, false)
}

func (h *Health) markFailed(err error) {
	h.mu.Lock()
	h.lastErr = err
	h.mu.Unlock()
	h.failed.Store(true)
	h.markStopped()
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

// highestPerPartition 把一批消息压成"每个分区最高 offset 的那一条"。
// kafka-go 的 CommitMessages 按每条消息的 offset+1 提交,只交最高的那条即可覆盖整批,
// 少交 N-1 次 offset 也少一次误提交低位的机会。
func highestPerPartition(msgs []kafkago.Message) []kafkago.Message {
	if len(msgs) == 0 {
		return nil
	}
	best := make(map[int]kafkago.Message, 4)
	for _, m := range msgs {
		if cur, ok := best[m.Partition]; !ok || m.Offset > cur.Offset {
			best[m.Partition] = m
		}
	}
	out := make([]kafkago.Message, 0, len(best))
	for _, m := range best {
		out = append(out, m)
	}
	return out
}

// observeFlushAttempt 记一次 DB 尝试的耗时与结局(两条消费者共用)。ctx 已结束导致的失败不记:
// 关停 / 兄弟分区停机不是库慢,记进去会让 flush_seconds{result="error"} 在每次发布时冒一下。
func observeFlushAttempt(ctx context.Context, consumer string, start time.Time, err error) {
	if err != nil && ctx.Err() != nil {
		return
	}
	result := metrics.KafkaResultOK
	if err != nil {
		result = metrics.KafkaResultError
	}
	metrics.ObserveKafkaConsumerFlushAttempt(consumer, result, time.Since(start))
}

// isReaderOffsetTraceLine 判断 kafka-go 普通 Logger(不是 ErrorLogger)上的一行是不是分区 reader (重新)初始化时的位点记录。
// 按 format 子串放行两行(kafka-go v0.4.47 reader.go,已按 tag 源码核对,两行都只经 Logger):
// (*reader).run 每次初始化前的 "initializing kafka reader for partition %d of %s starting at offset %d"(传入的起读位点)
// 与 (*reader).initialize 里的 "the kafka reader for partition %d of %s is seeking to offset %d"(实际起读位点)。
// Logger 上其余输出一律不放行(全集未核实,白名单与全集无关)。其中 fetch 超时的 "no messages received ..." 一行
// 也只经 Logger:空闲分区上它会反复出现(频率未核实),这是用白名单而不是整个接出来的原因。
func isReaderOffsetTraceLine(format string) bool {
	return strings.Contains(format, "starting at offset") || strings.Contains(format, "seeking to offset")
}

// newReader 建 kafka-go reader。NewReader 不拨号,连不上只会让 FetchMessage 报错并按
// 退避重试,所以这里不是启动失败点。
func newReader(brokers []string, topic, group string) *kafkago.Reader {
	return kafkago.NewReader(kafkago.ReaderConfig{
		Brokers: brokers,
		GroupID: group,
		Topic:   topic,
		// 新消费组从最早开始:data_service 晚于 scene 启动时,先发的流水/快照不能丢。
		StartOffset: kafkago.FirstOffset,
		MinBytes:    1,
		MaxBytes:    16 << 20, // 快照单条几百 KB,给足
		MaxWait:     500 * time.Millisecond,
		// CommitInterval=0:同步提交,由消费循环自己决定何时提交(插库成功之后)。
		CommitInterval: 0,
		// 保留期删掉未消费的消息之后 kafka-go 会自动跳到日志起点。它留下什么记录分两种情形
		// (v0.4.47 reader.go,已按 tag 源码核对;组 / 非组模式的分区 reader 同走 (*reader).run):
		//   - 运行中被保留期追上(fetch 返回 OffsetOutOfRange 且 offset < first):经 ErrorLogger 打
		//     `... skipping from offset X to Y (N messages)`,是该情形的精确记录;
		//   - 分区 reader 每次(重新)initialize(进程重启 / STOPPED 冷却后重建 / rebalance / 断线重连)时,已提交位点早于
		//     日志起点会在 initialize 里**静默**改到起点,ErrorLogger 不出声。这条路径靠下面 Logger 放行的两行:
		//     `starting at offset A`(传入位点;负数 = 这个组没提交过,不算丢)与 `seeking to offset B`(实际起读位点),
		//     A >= 0 且 B > A 时 B − A 就是被跳过的条数。DB 故障 → 停机 → 冷却 → 重建走的正是这条。
		// 两个都不配,这一跳完全静默,而 lag 探针的 retention_gap 会在消费者越过缺口、提交之后自行归零
		// (探针的 `AUDIT DATA LOST` 行只是采样到的下界)。只加日志,不改消费行为。
		ErrorLogger: kafkago.LoggerFunc(func(format string, args ...interface{}) {
			logx.Errorf("[kafka-go] topic=%s group=%s: "+format, append([]interface{}{topic, group}, args...)...)
		}),
		// 只放行上面说的两行位点记录,其余丢弃(见 isReaderOffsetTraceLine)。量:每个分区每次(重新)初始化两行;
		// broker 不可达期间每次初始化重试多一行 `starting at offset`,与 ErrorLogger 上已有的 `error initializing` 行同频。
		Logger: kafkago.LoggerFunc(func(format string, args ...interface{}) {
			if isReaderOffsetTraceLine(format) {
				logx.Infof("[kafka-go] topic=%s group=%s: "+format, append([]interface{}{topic, group}, args...)...)
			}
		}),
	})
}
