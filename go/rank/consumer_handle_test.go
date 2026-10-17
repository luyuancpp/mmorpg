package main

import (
	"testing"
	"time"

	"rank/internal/lifecycle"
)

// 收尾串行最坏 = 注销 + 排空(两者用满时等消费者的截止点已过,wait 不再阻塞)→ 框架收尾。
// 截止点必须给框架收尾留出时间,否则会被 24s 硬截止强退,丢掉 proc.Shutdown 与最后一段日志。
func TestConsumerStopBudgetLeavesRoomForFrameworkFinish(t *testing.T) {
	if consumerStopTimeout <= 0 || consumerStopBudget <= 0 {
		t.Fatalf("consumerStopTimeout=%v consumerStopBudget=%v 必须为正", consumerStopTimeout, consumerStopBudget)
	}
	if consumerStopBudget+lifecycle.FinishTimeout >= lifecycle.HardTimeout {
		t.Fatalf("consumerStopBudget(%v) + FinishTimeout(%v) 必须 < HardTimeout(%v)",
			consumerStopBudget, lifecycle.FinishTimeout, lifecycle.HardTimeout)
	}
}

// waitReturns 在 bound 内等 h.wait(limit) 返回;bound 只是防挂死的上限,远大于被测路径应有的耗时。
func waitReturns(t *testing.T, h *consumerHandle, limit time.Duration) {
	t.Helper()
	returned := make(chan struct{})
	go func() {
		h.wait(limit)
		close(returned)
	}()
	select {
	case <-returned:
	case <-time.After(5 * time.Second):
		t.Fatalf("wait(%v) 没有返回", limit)
	}
}

func TestConsumerHandleWaitReturnsWhenConsumerNeverStarted(t *testing.T) {
	waitReturns(t, &consumerHandle{}, time.Hour)
}

func TestConsumerHandleWaitReturnsOnceConsumerLeft(t *testing.T) {
	h := &consumerHandle{}
	done := make(chan struct{})
	h.set(done)
	close(done)
	waitReturns(t, h, time.Hour)
}

func TestConsumerHandleWaitDoesNotBlockWhenBudgetIsExhausted(t *testing.T) {
	h := &consumerHandle{}
	h.set(make(chan struct{})) // 消费者卡住、永不退出
	waitReturns(t, h, 0)
	waitReturns(t, h, -time.Second)
}
