package dataloader

import (
	"context"
	"errors"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
)

// 定义数据加载完成后的回调函数类型
// 参数说明：
// - taskKey：任务批次标识
// - success：是否加载成功
// - err：错误信息（若有）
type LoadCallback func(taskKey string, success bool, err error)

// LoadProtoFromRedis 从Redis加载单个PB数据
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

// BatchLoadAndCache 批量加载并缓存普通任务（支持回调）
// 新增callback参数：所有任务完成后触发
func BatchLoadAndCache(
	ctx context.Context,
	redisClient redis.Cmdable,
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	messages []proto.Message,
	executor *taskmanager.TaskExecutor,
	callback LoadCallback, // 新增回调参数
) error {
	taskKey := taskmanager.GenerateBatchTaskKey(messages, playerId)

	// 调用InitAndAddMessageTasks时传入回调
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

	return executor.SubmitTask(taskKey) // 非阻塞提交
}

// LoadAggregateData 加载聚合数据（支持回调）
// 新增callback参数：聚合完成后触发
func LoadAggregateData(
	ctx context.Context,
	redisClient redis.Cmdable,
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	result proto.Message,
	build func(uint64) []proto.Message,
	keyBuilder func(uint64) string,
	executor *taskmanager.TaskExecutor,
	callback LoadCallback, // 新增回调参数
) error {
	key := keyBuilder(playerId)

	// 先查缓存，命中则直接返回（此时无需触发回调，因数据已就绪）
	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		return err
	}
	if found {
		// 若缓存命中，直接触发回调（视为加载成功）
		if callback != nil {
			callback(key, true, nil)
		}
		return nil
	}

	// 缓存未命中，构建子任务
	subMsgs := build(playerId)
	taskKey := taskmanager.GenerateTaskKey(result, playerId)

	// 创建通用聚合器（注意：NewGenericAggregator返回(error, nil)，需处理错误）
	aggregator, err := taskmanager.NewGenericAggregator(result, key)
	if err != nil {
		return err
	}

	// 调用InitAndAddMessageTasks时传入回调
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

	return executor.SubmitTask(taskKey) // 非阻塞提交
}
