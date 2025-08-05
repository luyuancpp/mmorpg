package taskmanager

import (
	"context"
	"errors"
	"login/internal/logic/pkg/cache"
	"login/internal/logic/pkg/task"
	"login/pb/taskpb"
	"strconv"

	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// 这是任务初始化函数
func InitAndAddMessageTasks(
	ctx context.Context,
	manager *TaskManager,
	taskKey string,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	var tasks []*MessageTask

	for _, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr) // 你自己的 Redis key 生成函数

		_, err := redisClient.Get(ctx, key).Bytes()
		if err == nil {
			continue
		}
		if !errors.Is(err, redis.Nil) {
			return err
		}

		data, err := proto.Marshal(msg)
		if err != nil {
			return err
		}

		taskID := uuid.NewString()
		msgType := string(msg.ProtoReflect().Descriptor().FullName())

		taskPayload := &taskpb.DBTask{
			Key:       playerId,
			WhereCase: "where player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   msgType,
			Body:      data,
			TaskId:    taskID,
		}

		payloadBytes, err := proto.Marshal(taskPayload)
		if err != nil {
			return err
		}

		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			return err
		}

		tasks = append(tasks, &MessageTask{
			TaskID:   taskID,
			Message:  msg,
			RedisKey: key,
			PlayerID: playerId,
			Status:   "pending",
		})
	}

	if len(tasks) > 0 {
		manager.AddBatch(taskKey, tasks)
	}

	return nil
}
