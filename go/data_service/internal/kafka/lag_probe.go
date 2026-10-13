package kafka

import (
	"context"
	"time"

	"data_service/internal/metrics"

	"shared/kafkautil"
	"shared/safego"

	"github.com/zeromicro/go-zero/core/logx"
)

// lagProbePoint 是 lag 探针 goroutine 与每一轮的 safego 点位名(常量,不拼任何运行期值)。
const lagProbePoint = "data_service.kafka.lag_probe"

// lagTarget 是一条消费者在 broker 上要探的对象:哪个组、哪个 topic、几个分区。
type lagTarget struct {
	consumer, group, topic string
	partitions             int32
}

// runLagProbe 每 interval 探一次各消费组的 lag,直到 ctx 结束。只读 broker 元数据,不碰消费与落库。
//
// 为什么不用 kafka-go 的 Reader.Stats().Lag、也不按消息的 HighWaterMark 算:组模式下前者只反映最后读过的分区;
// 后者是各实例各自看到的局部值,rebalance 后旧主人的数留成陈值。要的是 broker 上「这个组在这个 topic 上还差多少」,
// 与哪个实例拿着哪个分区无关 —— 每个实例报同一个数,告警取 max。
//
// 探不到只记 lag_probe_total{result="error"} 并在下一轮重建连接,gauge 保留上一次的值(由 LagProbeBlind 告警兜)。
// 每一轮用 safego.Run 包住,一轮 panic 不杀循环。第一轮在 interval 之后。
func runLagProbe(ctx context.Context, brokers []string, interval time.Duration, targets []lagTarget) {
	var prober *kafkautil.GroupLagProber
	defer func() {
		if prober != nil {
			if err := prober.Close(); err != nil {
				logx.Errorf("[kafka] lag probe close: %v", err)
			}
		}
	}()

	// 每条消费者上一轮看到的「没提交过的分区数」:只在它变化时打 Info,低流量环境里永远收不到消息的分区
	// 不至于每 30s 刷一行。
	lastUncommitted := make(map[string]int, len(targets))
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
		safego.Run(lagProbePoint, func() {
			prober = probeLagOnce(prober, brokers, targets, lastUncommitted)
		})
	}
}

// probeLagOnce 探一轮,返回下一轮要用的连接:任何一个目标探失败都关掉连接并返回 nil,下一轮重建
// (sarama 的连接坏了没有可靠的就地自愈判据,重建最简单)。
func probeLagOnce(prober *kafkautil.GroupLagProber, brokers []string, targets []lagTarget, lastUncommitted map[string]int) *kafkautil.GroupLagProber {
	if prober == nil {
		p, err := kafkautil.NewGroupLagProber(brokers)
		if err != nil {
			for _, t := range targets {
				metrics.ObserveKafkaConsumerLagProbe(t.consumer, metrics.KafkaResultError)
			}
			logx.Errorf("[kafka] lag probe cannot connect to brokers %v; lag / retention-gap gauges keep their last value until it recovers: %v", brokers, err)
			return nil
		}
		prober = p
	}

	healthy := true
	for _, t := range targets {
		lag, err := prober.Probe(t.group, t.topic, t.partitions)
		if err != nil {
			healthy = false
			metrics.ObserveKafkaConsumerLagProbe(t.consumer, metrics.KafkaResultError)
			logx.Errorf("[%s] lag probe failed group=%s topic=%s: %v", t.consumer, t.group, t.topic, err)
			continue
		}
		metrics.SetKafkaConsumerLag(t.consumer, lag.Lag, lag.RetentionGap)
		metrics.ObserveKafkaConsumerLagProbe(t.consumer, metrics.KafkaResultOK)
		if lag.RetentionGap > 0 {
			// 审计数据已经丢了:保留期(retention.ms 或每分区 retention.bytes)删掉了这个组还没消费到的消息。
			// 精确条数以 [kafka-go] 的 out-of-range 跳转日志为准(消费者跳过并提交之后本值会自行归零)。
			logx.Errorf("[%s] AUDIT DATA LOST: %d message(s) of topic %s were deleted by retention before consumer group %s consumed them (lag=%d); see docs/ops/audit-topic-generation-bump-runbook.md",
				t.consumer, lag.RetentionGap, t.topic, t.group, lag.Lag)
		}
		if prev, seen := lastUncommitted[t.consumer]; lag.Uncommitted > 0 && (!seen || prev != lag.Uncommitted) {
			logx.Infof("[%s] lag probe: %d of %d partition(s) of topic %s have no committed offset for group %s yet (new group / new topic generation); counted from the log start",
				t.consumer, lag.Uncommitted, t.partitions, t.topic, t.group)
		}
		lastUncommitted[t.consumer] = lag.Uncommitted
	}
	if !healthy {
		if err := prober.Close(); err != nil {
			logx.Errorf("[kafka] lag probe close after failure: %v", err)
		}
		return nil
	}
	return prober
}
