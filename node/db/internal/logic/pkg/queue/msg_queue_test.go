package queue_test

import (
	"db/internal/logic/pkg/queue"
	"log"
	"sync"
	"testing"
	"time"
)

// dummyMsg 实现 proto.Message 空接口，用于测试
type dummyMsg struct{}

func (d *dummyMsg) Reset()         {}
func (d *dummyMsg) String() string { return "dummy" }
func (d *dummyMsg) ProtoMessage()  {}

func TestMsgQueue_AllFeatures(t *testing.T) {
	policy := queue.AutoExpandPolicy{
		Threshold: 0.7,
		Step:      2.0,
		MaxSize:   20,
		Enabled:   true,
	}

	initialWorkers := 1
	initialBuffer := uint64(5)

	mq := queue.NewMsgQueue(initialWorkers, initialBuffer, policy)

	var wg sync.WaitGroup

	// 启动初始worker
	for i := 0; i < initialWorkers; i++ {
		go queue.StartWorker(i, mq.QueueList[i].Data)
	}

	// 发送消息，触发自动扩容
	for i := 0; i < 15; i++ {
		wg.Add(1)
		ch := make(chan bool)
		err := mq.Put(queue.MessageTask{
			Key:       uint64(i),
			Body:      &dummyMsg{},
			Operation: queue.OpWrite,
			Chan:      ch,
		})
		if err != nil {
			t.Fatalf("Put failed: %v", err)
		}
		go func(c chan bool) {
			defer wg.Done()
			<-c
			log.Printf("task done")
		}(ch)
	}

	// 扩展worker数
	err := mq.ExpandWorker(2, func(index int, ch <-chan queue.MessageTask) {
		queue.StartWorker(index, ch)
	})
	if err != nil {
		t.Fatalf("ExpandWorker failed: %v", err)
	}

	// 给新worker发点消息验证
	for i := 15; i < 20; i++ {
		wg.Add(1)
		ch := make(chan bool)
		err := mq.Put(queue.MessageTask{
			Key:       uint64(i),
			Body:      &dummyMsg{},
			Operation: queue.OpRead,
			Chan:      ch,
		})
		if err != nil {
			t.Fatalf("Put failed: %v", err)
		}
		go func(c chan bool) {
			defer wg.Done()
			<-c
			log.Printf("task done")
		}(ch)
	}

	wg.Wait()
	mq.Close()
}

func startTestWorker(t *testing.T, mq *queue.MsgQueue, index int) {
	go queue.StartWorker(index, mq.QueueList[index].Data)
	time.Sleep(10 * time.Millisecond) // 给协程启动时间
}

// --- 测试入口 ---
func TestMsgQueue_All(t *testing.T) {
	policy := queue.AutoExpandPolicy{
		Threshold: 0.6,
		Step:      2.0,
		MaxSize:   10,
		Enabled:   true,
	}
	routineNum := 1
	channelBuffer := uint64(5)

	mq := queue.NewMsgQueue(routineNum, channelBuffer, policy)

	// 启动初始worker
	startTestWorker(t, mq, 0)

	t.Run("basic Put/Pop", func(t *testing.T) {
		ch := make(chan bool)
		task := queue.MessageTask{
			Key:       1,
			Body:      &dummyMsg{},
			Operation: queue.OpWrite,
			Chan:      ch,
		}
		err := mq.Put(task)
		if err != nil {
			t.Fatal("Put failed:", err)
		}

		select {
		case <-ch:
		case <-time.After(time.Second):
			t.Fatal("Task not processed in time")
		}
	})

	t.Run("auto expand buffer", func(t *testing.T) {
		// 发送多条消息，触发buffer自动扩容（capacity 5 -> 10）
		wg := sync.WaitGroup{}
		for i := 0; i < 8; i++ {
			wg.Add(1)
			ch := make(chan bool)
			task := queue.MessageTask{
				Key:       uint64(i),
				Body:      &dummyMsg{},
				Operation: queue.OpWrite,
				Chan:      ch,
			}
			err := mq.Put(task)
			if err != nil {
				t.Fatal("Put failed:", err)
			}
			go func(c chan bool) {
				defer wg.Done()
				<-c
			}(ch)
		}
		wg.Wait()

		// 断言容量扩容了
		mq.Mu.RLock()
		capacity := cap(mq.QueueList[0].Data)
		mq.Mu.RUnlock()
		if capacity <= int(channelBuffer) {
			t.Fatalf("expected capacity > %d, got %d", channelBuffer, capacity)
		}
	})

	t.Run("TryPop test", func(t *testing.T) {
		// 先保证队列为空，TryPop 应该立刻返回 false
		msg, ok := mq.TryPop(0)
		if ok {
			t.Fatal("TryPop expected false but got true, msg:", msg)
		}
	})

	t.Run("ExpandWorker test", func(t *testing.T) {
		initialWorkers := mq.RoutineNum
		err := mq.ExpandWorker(2, func(idx int, ch <-chan queue.MessageTask) {
			queue.StartWorker(idx, ch)
		})
		if err != nil {
			t.Fatal("ExpandWorker failed:", err)
		}

		if mq.RoutineNum != initialWorkers+2 {
			t.Fatalf("expected %d workers, got %d", initialWorkers+2, mq.RoutineNum)
		}

		// 向新worker投递任务验证
		wg := sync.WaitGroup{}
		for i := initialWorkers; i < initialWorkers+2; i++ {
			wg.Add(1)
			ch := make(chan bool)
			task := queue.MessageTask{
				Key:       uint64(i + 1000), // 确保映射到新增队列
				Body:      &dummyMsg{},
				Operation: queue.OpRead,
				Chan:      ch,
			}
			err := mq.Put(task)
			if err != nil {
				t.Fatal("Put failed:", err)
			}
			go func(c chan bool) {
				defer wg.Done()
				<-c
			}(ch)
		}
		wg.Wait()
	})

	t.Run("Close test", func(t *testing.T) {
		mq.Close()
		err := mq.Put(queue.MessageTask{
			Key:       123,
			Body:      &dummyMsg{},
			Operation: queue.OpRead,
			Chan:      make(chan bool),
		})
		if err == nil {
			t.Fatal("expected Put to fail after Close")
		}

		// 关闭后 TryPop 应该返回 false
		_, ok := mq.TryPop(0)
		if ok {
			t.Fatal("TryPop after Close expected false")
		}
	})
}
