package kafka

import (
	"context"
	"testing"
	"time"

	"login/internal/logic/pkg/consistent"
	db_proto "proto/db"

	"github.com/IBM/sarama"
	"github.com/IBM/sarama/mocks"
)

// newLockTestProducer 直接拼一个不连 broker 的生产者,只换掉底层 SyncProducer。
func newLockTestProducer(t *testing.T, sp sarama.SyncProducer, partitions int32) *KeyOrderedKafkaProducer {
	t.Helper()
	ring := consistent.NewConsistent(20)
	for i := int32(0); i < partitions; i++ {
		ring.AddPartition(i)
	}
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	p := &KeyOrderedKafkaProducer{
		producer:              sp,
		topic:                 "db_task_lock_test",
		partitionCnt:          int(partitions),
		ctx:                   ctx,
		cancel:                cancel,
		consistent:            ring,
		unavailablePartitions: make(map[int32]time.Time),
		retryInterval:         time.Minute,
	}
	p.payloadPool.New = func() interface{} { return make([]byte, 0, 1024) }
	return p
}

// 回归:Kafka 选主期间 SendMessage 返回 ErrLeaderNotAvailable,失败分支在持有 p.mu 时把分区标为不可用。
// 旧代码在那里再加一次 p.mu(sync.Mutex 不可重入),发送 goroutine 卡死,之后本进程所有发送都排在锁后面。
func TestSendTaskLeaderNotAvailableDoesNotDeadlock(t *testing.T) {
	if !isPartitionUnavailableErr(sarama.ErrLeaderNotAvailable) {
		t.Fatal("precondition: ErrLeaderNotAvailable must be classified as partition-unavailable, otherwise this test proves nothing")
	}

	sp := mocks.NewSyncProducer(t, nil)
	sp.ExpectSendMessageAndFail(sarama.ErrLeaderNotAvailable)
	sp.ExpectSendMessageAndSucceed()
	t.Cleanup(func() { _ = sp.Close() })

	p := newLockTestProducer(t, sp, 2)

	type result struct{ first, second error }
	done := make(chan result, 1)
	go func() {
		var r result
		r.first = p.SendTask(context.Background(), &db_proto.DBTask{TaskId: "lock-1", Key: 42, Op: "read"}, "42")
		r.second = p.SendTask(context.Background(), &db_proto.DBTask{TaskId: "lock-2", Key: 42, Op: "read"}, "42")
		done <- r
	}()

	select {
	case r := <-done:
		if r.first == nil {
			t.Fatal("first send must surface ErrLeaderNotAvailable")
		}
		if r.second != nil {
			t.Fatalf("second send must not be blocked or poisoned by the first failure: %v", r.second)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("SendTask deadlocked on p.mu after a partition-unavailable error")
	}

	countDone := make(chan int, 1)
	go func() { countDone <- p.getUnavailablePartitionCount() }()
	select {
	case got := <-countDone:
		if got != 1 {
			t.Fatalf("unavailable partitions = %d, want 1", got)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("p.mu still held after SendTask returned")
	}
}
