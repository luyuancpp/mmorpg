package kafka

// R7 复核 [1] 的单测:sarama 消费组错误通道的来源判定、计数与 nil 通道。

import (
	"errors"
	"fmt"
	"testing"
	"time"

	"db/internal/metrics"

	"github.com/IBM/sarama"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func consumerGroupErrorCount(t *testing.T, source string) float64 {
	t.Helper()
	return metricCount(t, "db_consumer_group_errors_total", map[string]string{"source": source})
}

// sarama 投进错误通道的三种形状:组级错误(心跳)是裸错误;分区级错误(位点提交 / 拉取)是 *ConsumerError;
// ConsumeClaim 返回的错误被 sarama 包成 *ConsumerError,链上有我们包的 claimHandlerError。
func TestConsumerGroupErrorSource(t *testing.T) {
	heartbeat := fmt.Errorf("read tcp: %w", fakeTimeoutNetErr{})
	commit := &sarama.ConsumerError{Topic: "t", Partition: 3, Err: sarama.ErrUnknownMemberId}
	claimErr := &sarama.ConsumerError{Topic: "t", Partition: 3,
		Err: &claimHandlerError{err: fmt.Errorf("poison DLQ write failed: %w", errEnqueueNotDurable)}}

	assert.Equal(t, metrics.ConsumerGroupErrorHeartbeat, consumerGroupErrorSource(heartbeat))
	assert.Equal(t, metrics.ConsumerGroupErrorHeartbeat, consumerGroupErrorSource(sarama.ErrUnknownMemberId))
	assert.Equal(t, metrics.ConsumerGroupErrorPartition, consumerGroupErrorSource(commit))
	assert.Equal(t, metrics.ConsumerGroupErrorClaim, consumerGroupErrorSource(claimErr))
	assert.True(t, errors.Is(claimErr, errEnqueueNotDurable), "the wrapper must not hide the original error")
}

// 观测协程读到通道关闭为止,逐条计数(限速只管日志);nil 通道不起协程(range nil 通道会永远挂住)。
func TestConsumerGroupErrorWatcher_CountsUntilClosed(t *testing.T) {
	c := &KeyOrderedKafkaConsumer{groupID: "g", topic: "t", now: func() time.Time { return time.Unix(1_800_000_000, 0) }}
	assert.False(t, c.startConsumerGroupErrorWatcher(nil), "a nil error channel (test doubles) starts nothing")

	before := map[string]float64{}
	for _, source := range []string{metrics.ConsumerGroupErrorHeartbeat, metrics.ConsumerGroupErrorPartition, metrics.ConsumerGroupErrorClaim} {
		before[source] = consumerGroupErrorCount(t, source)
	}
	errs := make(chan error, 8)
	errs <- errors.New("heartbeat: i/o timeout")
	errs <- errors.New("heartbeat: i/o timeout")
	errs <- &sarama.ConsumerError{Topic: "t", Partition: 1, Err: sarama.ErrIllegalGeneration}
	errs <- &sarama.ConsumerError{Topic: "t", Partition: 1, Err: &claimHandlerError{err: errors.New("claim failed")}}
	errs <- nil
	close(errs)

	done := make(chan struct{})
	go func() {
		c.watchConsumerGroupErrors(errs)
		close(done)
	}()
	waitDone(t, done, "the watcher returns once the channel is closed")

	assert.Equal(t, before[metrics.ConsumerGroupErrorHeartbeat]+2, consumerGroupErrorCount(t, metrics.ConsumerGroupErrorHeartbeat),
		"rate limiting only applies to the log line, every error is counted")
	assert.Equal(t, before[metrics.ConsumerGroupErrorPartition]+1, consumerGroupErrorCount(t, metrics.ConsumerGroupErrorPartition))
	assert.Equal(t, before[metrics.ConsumerGroupErrorClaim]+1, consumerGroupErrorCount(t, metrics.ConsumerGroupErrorClaim))
}

// ConsumeClaim 返回的错误包一层 claimHandlerError(来源判定用),errors.Is 照常穿透。
func TestConsumeClaim_ReturnedErrorIsTaggedAsClaim(t *testing.T) {
	f := newShutdownFixture(t)
	held := &holdingEnqueue{}
	t.Cleanup(held.close)
	f.c.durable = held
	f.claim.messages <- &sarama.ConsumerMessage{Topic: f.w.topic, Partition: 0, Offset: 0, Value: []byte{0xff, 0xff, 0xff}}
	done := f.startClaim()

	var reqs []*heldEnqueue
	require.Eventually(t, func() bool { reqs = held.requests(); return len(reqs) == 1 }, 5*time.Second, 5*time.Millisecond,
		"the poison message is handed to the enqueue layer")
	reqs[0].release(fmt.Errorf("%w: injected", errEnqueueNotDurable))

	err := waitClaimResult(t, done)
	require.Error(t, err)
	assert.ErrorIs(t, err, errEnqueueNotDurable)
	var tagged *claimHandlerError
	assert.True(t, errors.As(err, &tagged))
	assert.Equal(t, metrics.ConsumerGroupErrorClaim,
		consumerGroupErrorSource(&sarama.ConsumerError{Topic: f.w.topic, Partition: 0, Err: err}))
}
