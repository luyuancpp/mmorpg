package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
	"login/internal/logic/pkg/task"
	"login/pb/taskpb"
	"strconv"
	"time"
)

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

func BatchLoadAndCache(
	ctx context.Context,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)

	var (
		taskIDs         []string
		messagesToFetch []proto.Message
		uncachedKeys    []string
	)

	for _, msg := range messages {
		key := BuildRedisKey(msg, playerIdStr)

		// Redis 命中检查
		_, err := redisClient.Get(ctx, key).Bytes()
		if err == nil {
			continue
		}
		if !errors.Is(err, redis.Nil) {
			logx.Errorf("Redis GET error for key %s: %v", key, err)
			return err
		}

		// 未命中缓存 → 构建任务
		taskID := uuid.NewString()
		data, err := proto.Marshal(msg)
		if err != nil {
			logx.Errorf("Proto marshal failed: %v", err)
			return err
		}

		msgType := string(proto.MessageReflect(msg).Descriptor().FullName())
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
			logx.Errorf("Marshal task payload failed: %v", err)
			return err
		}

		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			logx.Errorf("Enqueue task failed: %v", err)
			return err
		}

		// 记录未命中
		taskIDs = append(taskIDs, taskID)
		messagesToFetch = append(messagesToFetch, msg)
		uncachedKeys = append(uncachedKeys, key)
	}

	// 等待任务返回结果
	for i, tid := range taskIDs {
		var (
			resBytes []byte
			err      error
		)

		for try := 0; try < 100; try++ {
			resBytes, err = redisClient.Get(ctx, tid).Bytes()
			if errors.Is(err, redis.Nil) {
				time.Sleep(time.Duration(try+1) * time.Millisecond) // 线性退避
				continue
			} else if err != nil {
				logx.Errorf("Redis get task result %s failed: %v", tid, err)
				return err
			}
			break
		}

		if err != nil {
			return fmt.Errorf("timeout waiting for task: %s", tid)
		}

		var result taskpb.TaskResult // 你需要定义这个结构
		if err := proto.Unmarshal(resBytes, &result); err != nil {
			logx.Errorf("Unmarshal task result failed: %v", err)
			return err
		}
		if !result.Success {
			return fmt.Errorf("task %s failed: %s", tid, result.Error)
		}

		err = proto.Unmarshal(result.Data, messagesToFetch[i])
		if err != nil {
			logx.Errorf("Unmarshal message failed: %v", err)
			return err
		}
	}

	// 缓存未命中成功加载的数据
	for i, msg := range messagesToFetch {
		err := SaveToRedis(ctx, redisClient, msg, uncachedKeys[i])
		if err != nil {
			logx.Errorf("SaveToRedis failed for key %s: %v", uncachedKeys[i], err)
			return err
		}
	}

	return nil
}

func LoadProtoFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if errors.Is(err, redis.Nil) {
		// Key 不存在，缓存未命中
		return false, nil
	}
	if err != nil {
		// Redis 其他错误（比如 context deadline）
		return false, err
	}
	if err := proto.Unmarshal(val, msg); err != nil {
		// Redis 拿到的是垃圾值或结构错误，反序列化失败
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

	// Step 1: 先尝试读取缓存
	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		logx.Errorf("RedisClient get failed: %v", err)
		return err
	}
	if found {
		return nil
	}

	playerIdStr := strconv.FormatUint(playerId, 10)
	subMsgs := build(playerId)
	taskIDs := make([]string, 0, len(subMsgs))
	msgTypes := make([]string, 0, len(subMsgs)) // 记录每个类型

	for _, msg := range subMsgs {
		taskID := uuid.NewString()

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
			TaskId:    taskID,
		}

		payload, err := proto.Marshal(taskPayload)
		if err != nil {
			logx.Errorf("marshal DBTask failed: %v", err)
			return err
		}

		_, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payload)
		if err != nil {
			logx.Errorf("enqueue task failed: %v", err)
			return err
		}

		taskIDs = append(taskIDs, taskID)
		msgTypes = append(msgTypes, msgType)
	}

	// Step 3: 等待所有任务完成
	var subResults []proto.Message
	for i, tid := range taskIDs {
		var resBytes []byte
		var err error
		for try := 0; try < 100; try++ {
			resBytes, err = redisClient.Get(ctx, tid).Bytes()
			if errors.Is(err, redis.Nil) {
				time.Sleep(time.Duration(try+1) * time.Millisecond)
				continue
			} else if err != nil {
				logx.Errorf("Redis get task result failed: %v", err)
				return err
			}
			break
		}

		if resBytes == nil || len(resBytes) == 0 {
			return fmt.Errorf("timeout or empty result for task: %s", tid)
		}

		var resultMsg taskpb.TaskResult
		if err := proto.Unmarshal(resBytes, &resultMsg); err != nil {
			return fmt.Errorf("unmarshal TaskResult failed: %v", err)
		}

		if !resultMsg.Success {
			return fmt.Errorf("task %s failed: %s", tid, resultMsg.Error)
		}

		// 动态反序列化 resultMsg.Data 到指定类型
		mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(msgTypes[i]))
		if err != nil {
			return fmt.Errorf("unknown message type: %s", msgTypes[i])
		}
		msg := dynamicpb.NewMessage(mt.Descriptor())

		if err := proto.Unmarshal(resultMsg.Data, msg); err != nil {
			return fmt.Errorf("unmarshal sub message failed: %v", err)
		}

		subResults = append(subResults, msg)
	}

	// Step 4: 聚合合并结果
	if err := assign(subResults, result); err != nil {
		return fmt.Errorf("assign aggregated result failed: %v", err)
	}

	// Step 5: 写入缓存
	if err := SaveProtoToRedis(ctx, redisClient, key, result, ttl); err != nil {
		logx.Errorf("SaveProtoToRedis failed: %v", err)
		return err
	}

	return nil
}
