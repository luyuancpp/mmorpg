package taskmanager

import (
	"context"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"login/pb/taskpb"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch
}

func NewTaskManager() *TaskManager {
	return &TaskManager{
		batches: make(map[string]*TaskBatch),
	}
}

func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask) {
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.batches[taskKey] = &TaskBatch{Tasks: tasks}
}

func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, exists := tm.batches[taskKey]
	return batch, exists
}

func SaveProtoToRedis(ctx context.Context, redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto to Redis error: %v", err)
		return err
	}
	return redis.Set(ctx, key, data, ttl).Err()
}

func WaitForTaskResult(ctx context.Context, redisClient redis.Cmdable, key string, maxTries int) ([]byte, error) {
	for try := 0; try < maxTries; try++ {
		resBytes, err := redisClient.Get(ctx, key).Bytes()
		if errors.Is(err, redis.Nil) {
			time.Sleep(time.Duration(try+1) * time.Millisecond)
			continue
		}
		if err != nil {
			return nil, err
		}
		return resBytes, nil
	}
	return nil, fmt.Errorf("timeout waiting for task: %s", key)
}

func (tm *TaskManager) ProcessBatch(ctx context.Context, taskKey string, redisClient redis.Cmdable) {
	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("No task batch for key: %s", taskKey)
		return
	}

	for _, task := range batch.Tasks {
		if task.Status != "pending" {
			continue
		}

		resBytes, err := WaitForTaskResult(ctx, redisClient, task.TaskID, 1000)
		if err != nil {
			task.Status = "failed"
			task.Error = err
			continue
		}

		var result taskpb.TaskResult
		if err := proto.Unmarshal(resBytes, &result); err != nil {
			task.Status = "failed"
			task.Error = err
			continue
		}

		if !result.Success {
			task.Status = "failed"
			task.Error = fmt.Errorf("task %s failed: %s", task.TaskID, result.Error)
			continue
		}

		if err := proto.Unmarshal(result.Data, task.Message); err != nil {
			task.Status = "failed"
			task.Error = err
			continue
		}

		if err := SaveProtoToRedis(ctx, redisClient, task.RedisKey, task.Message, 5*time.Minute); err != nil {
			task.Status = "failed"
			task.Error = err
			continue
		}

		task.Status = "done"
	}
}
