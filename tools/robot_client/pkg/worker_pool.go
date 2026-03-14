package pkg

import (
	"context"
	"hash/fnv"

	"github.com/panjf2000/ants/v2"
)

// TaskExecutor 管理多个TaskManager，每个对应一个worker池
type TaskExecutor struct {
	pools       []*ants.Pool
	workerCount int
	cancel      context.CancelFunc
}

// NewTaskExecutor 创建任务执行器
func NewTaskExecutor(workerCount int) (*TaskExecutor, error) {
	if workerCount <= 0 {
		workerCount = 16
	}

	pools := make([]*ants.Pool, 0, workerCount)

	for i := 0; i < workerCount; i++ {
		pool, err := ants.NewPool(1)
		if err != nil {
			for _, p := range pools {
				p.Release()
			}
			return nil, err
		}
		pools = append(pools, pool)
	}

	return &TaskExecutor{
		pools:       pools,
		workerCount: workerCount,
	}, nil
}

// SubmitTask 提交任务到对应worker的TaskManager
func (te *TaskExecutor) SubmitTask(taskKey string, task func()) error {
	idx := te.hashKey(taskKey)
	return te.pools[idx].Submit(task)
}

// Release 释放资源
func (te *TaskExecutor) Release() {
	for _, p := range te.pools {
		p.Release()
	}
	te.cancel()
}

// hashKey 计算key的哈希值，映射到worker索引
func (te *TaskExecutor) hashKey(key string) int {
	h := fnv.New32a()
	_, err := h.Write([]byte(key))
	if err != nil {
		return 0
	}
	return int(h.Sum32() % uint32(te.workerCount))
}
