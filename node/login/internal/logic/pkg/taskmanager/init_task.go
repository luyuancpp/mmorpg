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

// 新增：定义聚合任务选项
type InitTaskOptions struct {
	Aggregator Aggregator // 聚合器（为nil时表示普通任务）
}

// InitAndAddMessageTasks 初始化并添加消息任务（支持普通任务和聚合任务）
// 当options.Aggregator不为nil时，创建聚合任务（不缓存子PB）
func InitAndAddMessageTasks(
	ctx context.Context,
	executor *TaskExecutor,
	taskKey string,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
	options InitTaskOptions, // 新增选项参数，用于传递聚合器
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
			// SkipSubCache 不需要在这里设置，由AddBatch/AddAggregateBatch自动处理
		})
	}

	if len(tasks) > 0 {
		// 通过TaskExecutor获取该taskKey对应的TaskManager
		manager := executor.GetTaskManagerByKey(taskKey)

		// 根据是否有聚合器，选择添加普通批次或聚合批次
		if options.Aggregator != nil {
			// 聚合任务：不缓存子PB，最终缓存聚合后的父PB
			manager.AddAggregateBatch(taskKey, tasks, options.Aggregator)
		} else {
			// 普通任务：缓存每个子PB
			manager.AddBatch(taskKey, tasks)
		}
	}

	return nil
}
