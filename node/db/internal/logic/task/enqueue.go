package task

import (
	"context"
	"db/internal/config"
	"fmt"
	"github.com/hibiken/asynq"
)

func GetQueueName(playerId uint64) string {
	return fmt.Sprintf("player:%d", playerId%config.AppConfig.ServerConfig.QueueShardCount)
}

func EnqueueTaskWithID(ctx context.Context, client *asynq.Client, playerID uint64, taskID string, payload []byte) (string, error) {
	queueName := GetQueueName(playerID)
	task := asynq.NewTask("player_task", payload)
	_, err := client.Enqueue(task,
		asynq.Queue(queueName),
		asynq.TaskID(taskID),
	)
	if err != nil {
		return "", err
	}
	return taskID, nil
}
