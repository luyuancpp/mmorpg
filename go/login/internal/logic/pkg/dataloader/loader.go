package dataloader

import (
	"context"
	"errors"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
)

type LoadCallback func(taskKey string, success bool, err error)

// 原有方法保持不变：底层InitAndAddMessageTasks已支持批量投递，上层无需修改
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
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	messages []proto.Message,
	executor *taskmanager.TaskExecutor,
	callback LoadCallback,
) error {
	taskKey := taskmanager.GenerateBatchTaskKey(messages, playerId)

	// 直接调用：底层会自动判断批量/单条投递
	err := taskmanager.InitAndAddMessageTasks(
		ctx,
		executor,
		taskKey,
		redisClient,
		producer,
		playerId,
		messages,
		taskmanager.InitTaskOptions{
			Callback: func(tk string, allSuccess bool, err error) {
				if callback != nil {
					callback(tk, allSuccess, err)
				}
			},
		},
	)
	if err != nil {
		return err
	}

	return executor.SubmitTask(taskKey)
}

func LoadAggregateData(
	ctx context.Context,
	redisClient redis.Cmdable,
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	result proto.Message,
	build func(uint64) []proto.Message,
	keyBuilder func(uint64) string,
	executor *taskmanager.TaskExecutor,
	callback LoadCallback,
) error {
	key := keyBuilder(playerId)

	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		return err
	}
	if found {
		if callback != nil {
			callback(key, true, nil)
		}
		return nil
	}

	subMsgs := build(playerId)
	taskKey := taskmanager.GenerateTaskKey(result, playerId)

	aggregator, err := taskmanager.NewGenericAggregator(result, key)
	if err != nil {
		return err
	}

	// 直接调用：底层自动处理批量投递
	err = taskmanager.InitAndAddMessageTasks(
		ctx,
		executor,
		taskKey,
		redisClient,
		producer,
		playerId,
		subMsgs,
		taskmanager.InitTaskOptions{
			Aggregator: aggregator,
			Callback: func(tk string, allSuccess bool, err error) {
				if callback != nil {
					callback(tk, allSuccess, err)
				}
			},
		},
	)
	if err != nil {
		return err
	}

	return executor.SubmitTask(taskKey)
}
