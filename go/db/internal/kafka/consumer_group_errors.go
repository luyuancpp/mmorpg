package kafka

// sarama 消费组错误通道的观测(R7 复核)。
//
// NewKeyOrderedKafkaConsumer 设了 Consumer.Return.Errors=true,sarama v1.43.1 因此不再把这些错误交给它的 Logger,只往
// consumerGroup.errors(容量 ChannelBufferSize=256)里非阻塞投递,满了就丢(consumer_group.go handleError)。以前全仓
// 没有生产代码读这个通道,心跳失败、位点提交失败、claim 返回的错误都不可见:本成员被 coordinator 逐出、而本进程还要
// 过约 Net.ReadTimeout 到 (Metadata.Retry.Max+1)×Net.ReadTimeout 才察觉的双属主窗口(retry_routing.go 文件头)无从定位。
//
// 现在 Start 起一个协程读这个通道:按来源计 db_consumer_group_errors_total{source},每个来源每 storeLogEvery(10s)
// 打一行 ERROR,带上期间压掉的条数。通道在 consumer.Close(Stop 第③步)里关闭,协程随之退出;不进 c.wg —— 它只观测,
// Stop 不必等它。替身(fakeConsumerGroup)返回 nil 通道时不起协程(range nil 通道会永远挂住)。
//
// 来源的判定(consumerGroupErrorSource):
//   - 不是 *sarama.ConsumerError → heartbeat:sarama 只对不属于某个分区的组级错误(心跳、找 coordinator、Cleanup)
//     传空 topic,我们的 Cleanup 恒返回 nil,所以实际上是心跳 / coordinator;
//   - *sarama.ConsumerError 且链上有 claimHandlerError → claim:ConsumeClaim 返回的错误(它在返回前包一层);
//   - 其余 *sarama.ConsumerError → partition:sarama 自己的分区级错误,位点提交与拉取在通道上无法区分。

import (
	"errors"
	"time"

	"db/internal/metrics"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
)

// claimHandlerError 标记「这是本进程 ConsumeClaim 返回的错误」,只为错误通道上的来源判定;Error / Unwrap 原样透传,
// errors.Is / As 照常穿透到原错误。
type claimHandlerError struct {
	err error
}

func (e *claimHandlerError) Error() string { return e.err.Error() }

func (e *claimHandlerError) Unwrap() error { return e.err }

// consumerGroupErrorLogLimiter 给错误通道的日志限速(按来源;与 storeLogLimiter 分开计)。
var consumerGroupErrorLogLimiter = &logLimiter{last: map[string]time.Time{}, held: map[string]int{}}

// consumerGroupErrorSource 判定一条错误的来源,取值见 metrics.ConsumerGroupError*。
func consumerGroupErrorSource(err error) string {
	var consumerErr *sarama.ConsumerError
	if !errors.As(err, &consumerErr) {
		return metrics.ConsumerGroupErrorHeartbeat
	}
	var handlerErr *claimHandlerError
	if errors.As(err, &handlerErr) {
		return metrics.ConsumerGroupErrorClaim
	}
	return metrics.ConsumerGroupErrorPartition
}

// startConsumerGroupErrorWatcher 在 errs 非 nil 时起观测协程并返回 true;nil 通道返回 false。
func (c *KeyOrderedKafkaConsumer) startConsumerGroupErrorWatcher(errs <-chan error) bool {
	if errs == nil {
		return false
	}
	go c.watchConsumerGroupErrors(errs)
	return true
}

// watchConsumerGroupErrors 读到通道关闭为止:每条计数,按来源限速打 ERROR。
func (c *KeyOrderedKafkaConsumer) watchConsumerGroupErrors(errs <-chan error) {
	for err := range errs {
		if err == nil {
			continue
		}
		source := consumerGroupErrorSource(err)
		metrics.CountConsumerGroupError(source)
		ok, suppressed := consumerGroupErrorLogLimiter.allow(source, c.nowTime())
		if !ok {
			continue
		}
		logx.Errorf("kafka consumer group error: source=%s groupID=%s topic=%s suppressed_since_last=%d err=%v",
			source, c.groupID, c.topic, suppressed, err)
	}
}
