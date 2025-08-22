package taskmanager

import (
	"hash/fnv"

	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// TaskExecutor 管理多个TaskManager，每个对应一个worker池
type TaskExecutor struct {
	pools        []*ants.Pool
	taskManagers []*TaskManager
	redis        redis.Cmdable
	workerCount  int
}

// NewTaskExecutor 创建任务执行器
func NewTaskExecutor(workerCount int, redis redis.Cmdable) (*TaskExecutor, error) {
	if workerCount <= 0 {
		workerCount = 16
	}

	pools := make([]*ants.Pool, 0, workerCount)
	taskManagers := make([]*TaskManager, 0, workerCount)

	for i := 0; i < workerCount; i++ {
		pool, err := ants.NewPool(1)
		if err != nil {
			for _, p := range pools {
				p.Release()
			}
			return nil, err
		}
		pools = append(pools, pool)
		taskManagers = append(taskManagers, NewTaskManager())
	}

	return &TaskExecutor{
		pools:        pools,
		taskManagers: taskManagers,
		redis:        redis,
		workerCount:  workerCount,
	}, nil
}

// SubmitTask 提交任务到对应worker的TaskManager
func (te *TaskExecutor) SubmitTask(taskKey string) error {
	idx := te.hashKey(taskKey)
	return te.pools[idx].Submit(func() {
		logx.Infof("Worker %d processing task: %s", idx, taskKey)
		te.taskManagers[idx].ProcessBatch(taskKey, te.redis)
		logx.Infof("Worker %d finished task: %s", idx, taskKey)
	})
}

// GetTaskManagerByKey 根据taskKey获取对应的TaskManager
func (te *TaskExecutor) GetTaskManagerByKey(taskKey string) *TaskManager {
	idx := te.hashKey(taskKey)
	return te.taskManagers[idx]
}

// Release 释放资源
func (te *TaskExecutor) Release() {
	for _, p := range te.pools {
		p.Release()
	}
}

// hashKey 计算key的哈希值，映射到worker索引
func (te *TaskExecutor) hashKey(key string) int {
	h := fnv.New32a()
	h.Write([]byte(key))
	return int(h.Sum32() % uint32(te.workerCount))
}
