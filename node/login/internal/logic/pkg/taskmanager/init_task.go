package taskmanager

import (
	"context"
	"login/internal/logic/pkg/cache"
	"login/internal/logic/pkg/task"
	"login/pb/taskpb"
	"strconv"

	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// InitAndAddMessageTasks 初始化并添加消息任务到对应key的TaskManager
// 注意：现在需要传入TaskExecutor而不是直接传入TaskManager
func InitAndAddMessageTasks(
	ctx context.Context,
	executor *TaskExecutor, // 改为传入TaskExecutor
	taskKey string,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	var tasks []*MessageTask

	// 构造 Redis key 和映射
	keys := make([]string, 0, len(messages))
	msgMap := make(map[string]proto.Message, len(messages))

	for _, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr)
		keys = append(keys, key)
		msgMap[key] = msg
	}

	// 使用 MGET 批量获取 Redis 数据
	values, err := redisClient.MGet(ctx, keys...).Result()
	if err != nil {
		return err
	}

	// 遍历 Redis 返回值，处理 miss 的 key（nil）
	for i, val := range values {
		if val != nil {
			continue // 已命中缓存，跳过
		}

		key := keys[i]
		msg := msgMap[key]

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
			Status:   TaskStatusPending,
		})
	}

	if len(tasks) > 0 {
		// 通过TaskExecutor获取该taskKey对应的TaskManager
		manager := executor.GetTaskManagerByKey(taskKey)
		manager.AddBatch(taskKey, tasks)
	}

	return nil
}
