package taskmanager

import (
	"context"
	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
)

type TaskExecutor struct {
	pool    *ants.Pool
	taskMgr *TaskManager
	redis   redis.Cmdable
}

func NewTaskExecutor(workerCount int, taskMgr *TaskManager, redis redis.Cmdable) (*TaskExecutor, error) {
	pool, err := ants.NewPool(workerCount)
	if err != nil {
		return nil, err
	}
	return &TaskExecutor{
		pool:    pool,
		taskMgr: taskMgr,
		redis:   redis,
	}, nil
}

func (te *TaskExecutor) SubmitTask(ctx context.Context, taskKey string) error {
	return te.pool.Submit(func() {
		te.taskMgr.ProcessBatch(ctx, taskKey, te.redis)
	})
}
