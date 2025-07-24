package utils

import (
	"context"
	"db/internal/logic/task"
	"db/pb/game"
	"db/pb/taskpb"
	"errors"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"strconv"
	"time"
)

type DBTaskPayload struct {
	Key       uint64 `json:"key"`
	WhereCase string `json:"where_case"`
	Op        string `json:"op"`       // "read" / "write"
	MsgType   string `json:"msg_type"` // 消息类型名，用于反序列化
	Body      []byte `json:"-"`        // 序列化后放 task.Payload() 的 raw data
}

func BuildRedisKey(message proto.Message, playerIdStr string) string {
	return string(proto.MessageReflect(message).Descriptor().FullName()) + ":" + playerIdStr
}

func SaveToRedis(ctx context.Context, redisClient redis.Cmdable, message proto.Message, key string) error {
	data, err := proto.Marshal(message)
	if err != nil {
		logx.Error("Marshal proto to redis error:", err)
		return err
	}
	return redisClient.Set(ctx, key, data, 0).Err()
}

func LoadFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, message proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if err != nil {
		if err == redis.Nil {
			return false, nil
		}
		logx.Error("RedisClient get error:", err)
		return false, err
	}
	if err := proto.Unmarshal(val, message); err != nil {
		logx.Error("Unmarshal error:", err)
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
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	taskIDs := make([]string, 0, len(messages))
	redisKeys := make([]string, 0, len(messages))
	messagesToFetch := make([]proto.Message, 0, len(messages))

	for _, msg := range messages {
		key := BuildRedisKey(msg, playerIdStr)
		redisKeys = append(redisKeys, key)

		// 尝试从 Redis 读取
		val, err := redisClient.Get(ctx, key).Bytes()
		if err == nil && len(val) > 0 {
			continue // 已缓存，跳过
		}
		if err != nil && err != redis.Nil {
			logx.Errorf("Redis get failed: %v", err)
			return err
		}

		// ✅ 生成 UUID 作为 taskID
		taskID := uuid.NewString()

		// 序列化消息体
		data, err := proto.Marshal(msg)
		if err != nil {
			logx.Errorf("proto marshal failed: %v", err)
			return err
		}

		msgType := string(proto.MessageReflect(msg).Descriptor().FullName())

		taskPayload := &taskpb.DBTask{
			Key:       playerId,
			WhereCase: "where player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   msgType,
			Body:      data,
			TaskId:    taskID, // 👈 把 UUID 放进结构体中
		}

		payloadBytes, err := proto.Marshal(taskPayload)
		if err != nil {
			logx.Errorf("marshal task payload failed: %v", err)
			return err
		}

		// 入队
		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			logx.Errorf("enqueue task failed: %v", err)
			return err
		}

		taskIDs = append(taskIDs, taskID)
		messagesToFetch = append(messagesToFetch, msg)
	}

	// 等待返回值
	for i, tid := range taskIDs {
		var resBytes []byte
		var err error
		for try := 0; try < 100; try++ {
			resBytes, err = redisClient.Get(ctx, tid).Bytes()
			if err == redis.Nil {
				time.Sleep(100 * time.Millisecond)
				continue
			} else if err != nil {
				return err
			}
			break
		}
		if resBytes == nil {
			return fmt.Errorf("timeout waiting for task: %s", tid)
		}

		// 反序列化覆盖原始 proto.Message
		if err := proto.Unmarshal(resBytes, messagesToFetch[i]); err != nil {
			logx.Errorf("unmarshal returned proto failed: %v", err)
			return err
		}
	}

	// 缓存回 Redis
	for i, msg := range messagesToFetch {
		if i < len(redisKeys) {
			err := SaveToRedis(ctx, redisClient, msg, redisKeys[i])
			if err != nil {
				logx.Errorf("SaveToRedis failed: %v", err)
				return err
			}
		}
	}

	return nil
}

func LoadProtoFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if errors.Is(err, redis.Nil) {
		return false, nil // 未命中
	}
	if err := proto.Unmarshal(val, msg); err != nil {
		return false, err
	}
	return true, nil
}

func SaveProtoToRedis(ctx context.Context, redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto to RedisClient error: %v", err)
		return err
	}
	return redis.Set(ctx, key, data, ttl).Err()
}

func LoadAggregateData(
	ctx context.Context,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	result proto.Message,
	build func(uint64) []proto.Message,
	assign func([]proto.Message, proto.Message) error,
	keyBuilder func(uint64) string,
	ttl time.Duration,
) error {
	key := keyBuilder(playerId)

	// 1. RedisClient 命中直接返回
	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		logx.Errorf("RedisClient get failed: %v", err)
		return err
	}
	if found {
		return nil
	}

	// 2. 构造子消息并发异步任务
	playerIdStr := strconv.FormatUint(playerId, 10)
	subMsgs := build(playerId)
	taskIDs := make([]string, 0, len(subMsgs))

	for _, msg := range subMsgs {
		// ✅ 生成 UUID 作为 taskID
		taskID := uuid.NewString()

		// 序列化消息体
		data, err := proto.Marshal(msg)
		if err != nil {
			logx.Errorf("proto marshal failed: %v", err)
			return err
		}

		msgType := string(proto.MessageReflect(msg).Descriptor().FullName())

		taskPayload := &taskpb.DBTask{
			Key:       playerId,
			WhereCase: "where player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   msgType,
			Body:      data,
			TaskId:    taskID, // 👈 把 UUID 放进结构体中
		}

		payloadBytes, err := proto.Marshal(taskPayload)
		if err != nil {
			logx.Errorf("marshal task payload failed: %v", err)
			return err
		}

		// 入队
		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			logx.Errorf("enqueue task failed: %v", err)
			return err
		}

		taskIDs = append(taskIDs, taskID)
	}

	// 3. 等待所有异步任务完成（轮询或订阅 RedisClient）
	var subResults []proto.Message
	for _, tid := range taskIDs {
		var resBytes []byte
		for try := 0; try < 100; try++ { // 最多等 10 秒
			resBytes, err = redisClient.Get(ctx, tid).Bytes()
			if err == redis.Nil {
				time.Sleep(100 * time.Millisecond)
				continue
			} else if err != nil {
				return err
			}
			break
		}
		if resBytes == nil {
			return fmt.Errorf("timeout waiting for task: %s", tid)
		}

		// 反序列化回对应的 proto.Message
		// ⚠️ 你可以根据 type 做 switch-case 区分
		msg := &game.PlayerDatabase{} // 举例
		if err := proto.Unmarshal(resBytes, msg); err != nil {
			return err
		}
		subResults = append(subResults, msg)
	}

	// 4. 聚合
	if err := assign(subResults, result); err != nil {
		return err
	}

	// 5. 写入 RedisClient 缓存
	if err := SaveProtoToRedis(ctx, redisClient, key, result, ttl); err != nil {
		return err
	}

	return nil
}
