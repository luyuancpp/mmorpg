package task

import (
	"context"
	"fmt"
	"github.com/hibiken/asynq"
	"github.com/zeromicro/go-zero/core/logx"
	"login/internal/config"
)

func GetQueueName(playerId uint64) string {
	return fmt.Sprintf("shard:%d", playerId%config.AppConfig.Node.QueueShardCount)
}

func EnqueueTaskWithID(ctx context.Context, client *asynq.Client, playerID uint64, taskID string, payload []byte) (string, error) {
	if taskID == "" {
		return "", fmt.Errorf("taskID cannot be empty")
	}
	if len(payload) == 0 {
		return "", fmt.Errorf("payload cannot be empty")
	}
	if config.AppConfig.Node.QueueShardCount <= 0 {
		return "", fmt.Errorf("invalid QueueShardCount: %d", config.AppConfig.Node.QueueShardCount)
	}

	queueName := fmt.Sprintf("shard:%d", playerID%config.AppConfig.Node.QueueShardCount)
	task := asynq.NewTask("shard_task", payload)

	_, err := client.Enqueue(task,
		asynq.Queue(queueName),
		asynq.TaskID(taskID),
	)
	if err != nil {
		logx.Errorf("enqueue task failed: %v", err)
		return "", err
	}
	return taskID, nil
}
