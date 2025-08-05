package dataloader

import (
	"context"
	"errors"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/taskmanager"
)

func LoadProtoFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if errors.Is(err, redis.Nil) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	if err := proto.Unmarshal(val, msg); err != nil {
		return false, err
	}
	return true, nil
}

func BatchLoadAndCache(
	ctx context.Context,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
	taskMgr *taskmanager.TaskManager,
	executor *taskmanager.TaskExecutor,
) error {
	taskKey := taskmanager.GenerateBatchTaskKey(messages, playerId)

	err := taskmanager.InitAndAddMessageTasks(ctx, taskMgr, taskKey, redisClient, asyncClient, playerId, messages)

	if err != nil {
		return err
	}

	return executor.SubmitTask(ctx, taskKey) // 非阻塞提交
}

func LoadAggregateData(
	ctx context.Context,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	result proto.Message,
	build func(uint64) []proto.Message,
	keyBuilder func(uint64) string,
	taskMgr *taskmanager.TaskManager,
	executor *taskmanager.TaskExecutor,
) error {
	key := keyBuilder(playerId)

	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		return err
	}
	if found {
		return nil
	}

	subMsgs := build(playerId)

	taskKey := taskmanager.GenerateTaskKey(result, playerId)

	err = taskmanager.InitAndAddMessageTasks(ctx, taskMgr, taskKey, redisClient, asyncClient, playerId, subMsgs)
	if err != nil {
		return err
	}

	return executor.SubmitTask(ctx, taskKey) // 非阻塞提交
}
