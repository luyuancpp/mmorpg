package task

import (
	"context"
	"fmt"
	"github.com/hibiken/asynq"
	"login/internal/config"
)

func GetQueueName(playerId uint64) string {
	return fmt.Sprintf("shard:%d", playerId%config.AppConfig.Node.QueueShardCount)
}

func EnqueueTaskWithID(ctx context.Context, client *asynq.Client, playerID uint64, taskID string, payload []byte) (string, error) {
	queueName := GetQueueName(playerID)
	task := asynq.NewTask("shard_task", payload)
	_, err := client.Enqueue(task,
		asynq.Queue(queueName),
		asynq.TaskID(taskID),
	)
	if err != nil {
		return "", err
	}
	return taskID, nil
}
