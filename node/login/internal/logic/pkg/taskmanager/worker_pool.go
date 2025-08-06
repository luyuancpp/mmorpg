package taskmanager

import (
	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
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

func (te *TaskExecutor) SubmitTask(taskKey string) error {
	return te.pool.Submit(func() {
		logx.Infof("Processing task: %s", taskKey)
		te.taskMgr.ProcessBatch(taskKey, te.redis)
		logx.Infof("Finished processing task: %s", taskKey)
	})
}
