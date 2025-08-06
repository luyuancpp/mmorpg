package taskmanager

import (
	"context"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"login/internal/config"
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

func WaitForTaskResult(ctx context.Context, redisClient redis.Cmdable, key string, timeout time.Duration) ([]byte, error) {
	// BLPOP 返回 [key, value]，如果超时或 key 不存在则返回 redis.Nil
	result, err := redisClient.BLPop(ctx, timeout, key).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return nil, fmt.Errorf("timeout waiting for task: %s", key)
		}
		return nil, err
	}

	if len(result) != 2 {
		return nil, fmt.Errorf("unexpected BLPOP result: %v", result)
	}

	return []byte(result[1]), nil
}

func (tm *TaskManager) ProcessBatch(ctx context.Context, taskKey string, redisClient redis.Cmdable) {
	logx.Infof("Start processing task batch for key: %s", taskKey)

	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("No task batch found for key: %s", taskKey)
		return
	}

	for _, task := range batch.Tasks {
		if task.Status != "pending" {
			logx.Infof("Skipping task %s: status is %s", task.TaskID, task.Status)
			continue
		}

		logx.Infof("Waiting for task result: %s", task.TaskID)
		resBytes, err := WaitForTaskResult(ctx, redisClient, task.TaskID, time.Duration(config.AppConfig.Timeouts.TaskWaitTimeoutSec)*time.Second)
		if err != nil {
			task.Status = "failed"
			task.Error = err
			logx.Errorf("Task %s failed while waiting: %v", task.TaskID, err)
			continue
		}

		var result taskpb.TaskResult
		if err := proto.Unmarshal(resBytes, &result); err != nil {
			task.Status = "failed"
			task.Error = err
			logx.Errorf("Failed to unmarshal TaskResult for task %s: %v", task.TaskID, err)
			continue
		}

		if !result.Success {
			err := fmt.Errorf("task %s failed: %s", task.TaskID, result.Error)
			task.Status = "failed"
			task.Error = err
			logx.Errorf("Task %s failed with error: %s", task.TaskID, result.Error)
			continue
		}

		if err := proto.Unmarshal(result.Data, task.Message); err != nil {
			task.Status = "failed"
			task.Error = err
			logx.Errorf("Failed to unmarshal Message for task %s: %v", task.TaskID, err)
			continue
		}

		if err := SaveProtoToRedis(ctx, redisClient, task.RedisKey, task.Message, 5*time.Minute); err != nil {
			task.Status = "failed"
			task.Error = err
			logx.Errorf("Failed to save message to Redis for task %s: %v", task.TaskID, err)
			continue
		}

		task.Status = "done"
		logx.Infof("Task %s processed successfully and saved to Redis", task.TaskID)
	}

	logx.Infof("Finished processing task batch for key: %s", taskKey)
}
