package task

import (
	"context"
	"fmt"

	"github.com/hibiken/asynq"
)

const shardCount = 32

type DBTask struct {
	Key       uint64
	WhereCase string
	Body      []byte // Protobuf 序列化后的数据
}

func GetQueueName(playerID uint64) string {
	shard := playerID % shardCount
	return fmt.Sprintf("player_queue_%d", shard)
}

func EnqueueTaskWithID(ctx context.Context, client *asynq.Client, playerID uint64, taskID string, payload []byte) (string, error) {
	queueName := GetQueueName(playerID)

	task := asynq.NewTask("player_task", payload)

	// 加入带唯一 ID 的任务
	_, err := client.Enqueue(task,
		asynq.Queue(queueName),
		asynq.TaskID(taskID), // 设置唯一 Task ID
	)

	if err != nil {
		return "", err
	}
	return taskID, nil
}
