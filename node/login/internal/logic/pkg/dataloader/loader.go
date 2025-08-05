package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/taskmanager"
	"time"
)

func BuildRedisKey(message proto.Message, playerIdStr string) string {
	return string(message.ProtoReflect().Descriptor().FullName()) + ":" + playerIdStr
}

func SaveProtoToRedis(ctx context.Context, redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto to Redis error: %v", err)
		return err
	}
	return redis.Set(ctx, key, data, ttl).Err()
}

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
