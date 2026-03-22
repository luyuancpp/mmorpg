package taskmanager

import (
	"context"
	"hash/fnv"

	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// TaskExecutor manages multiple TaskManagers, each backed by a worker pool.
type TaskExecutor struct {
	pools        []*ants.Pool
	taskManagers []*TaskManager
	redis        redis.Cmdable
	workerCount  int
	cancel       context.CancelFunc
}

// NewTaskExecutor creates a task executor.
func NewTaskExecutor(workerCount int, redis redis.Cmdable) (*TaskExecutor, error) {
	if workerCount <= 0 {
		workerCount = 16
	}

	customCtx, cancel := context.WithCancel(context.Background())

	pools := make([]*ants.Pool, 0, workerCount)
	taskManagers := make([]*TaskManager, 0, workerCount)

	for i := 0; i < workerCount; i++ {
		pool, err := ants.NewPool(1)
		if err != nil {
			for _, p := range pools {
				p.Release()
			}
			cancel()
			return nil, err
		}
		pools = append(pools, pool)
		taskManagers = append(taskManagers, NewTaskManager(customCtx))
	}

	return &TaskExecutor{
		pools:        pools,
		taskManagers: taskManagers,
		redis:        redis,
		workerCount:  workerCount,
		cancel:       cancel,
	}, nil
}

// SubmitTask submits a task to the corresponding worker's TaskManager.
func (te *TaskExecutor) SubmitTask(taskKey string) error {
	idx := te.hashKey(taskKey)
	return te.pools[idx].Submit(func() {
		logx.Infof("Worker %d processing task: %s", idx, taskKey)
		te.taskManagers[idx].ProcessBatch(taskKey, te.redis)
		logx.Infof("Worker %d finished task: %s", idx, taskKey)
	})
}

// GetTaskManagerByKey returns the TaskManager for a given taskKey.
func (te *TaskExecutor) GetTaskManagerByKey(taskKey string) *TaskManager {
	idx := te.hashKey(taskKey)
	return te.taskManagers[idx]
}

// Release releases all worker pools.
func (te *TaskExecutor) Release() {
	for _, p := range te.pools {
		p.Release()
	}
	te.cancel()
}

// hashKey maps a key to a worker index via FNV hash.
func (te *TaskExecutor) hashKey(key string) int {
	h := fnv.New32a()
	_, err := h.Write([]byte(key))
	if err != nil {
		logx.Errorf("hashKey error: %v", err)
		return 0
	}
	return int(h.Sum32() % uint32(te.workerCount))
}
