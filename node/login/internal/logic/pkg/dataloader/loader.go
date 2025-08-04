package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
	"login/internal/logic/pkg/task"
	"login/pb/taskpb"
	"strconv"
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
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)

	var (
		taskIDs         []string
		messagesToFetch []proto.Message
		uncachedKeys    []string
	)

	for _, msg := range messages {
		key := BuildRedisKey(msg, playerIdStr)

		_, err := redisClient.Get(ctx, key).Bytes()
		if err == nil {
			continue
		}
		if !errors.Is(err, redis.Nil) {
			logx.Errorf("Redis GET error for key %s: %v", key, err)
			return err
		}

		taskID := uuid.NewString()
		data, err := proto.Marshal(msg)
		if err != nil {
			logx.Errorf("Proto marshal failed: %v", err)
			return err
		}

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
			logx.Errorf("Marshal task payload failed: %v", err)
			return err
		}

		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			logx.Errorf("Enqueue task failed: %v", err)
			return err
		}

		taskIDs = append(taskIDs, taskID)
		messagesToFetch = append(messagesToFetch, msg)
		uncachedKeys = append(uncachedKeys, key)
	}

	for i, tid := range taskIDs {
		resBytes, err := WaitForTaskResult(ctx, redisClient, tid, 100)
		if err != nil {
			return err
		}

		var result taskpb.TaskResult
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

	for i, msg := range messagesToFetch {
		err := SaveProtoToRedis(ctx, redisClient, uncachedKeys[i], msg, 5*time.Minute)
		if err != nil {
			logx.Errorf("SaveToRedis failed for key %s: %v", uncachedKeys[i], err)
			return err
		}
	}

	return nil
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

	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		logx.Errorf("Redis get failed: %v", err)
		return err
	}
	if found {
		return nil
	}

	playerIdStr := strconv.FormatUint(playerId, 10)
	subMsgs := build(playerId)
	taskIDs := make([]string, 0, len(subMsgs))
	msgTypes := make([]string, 0, len(subMsgs))

	for _, msg := range subMsgs {
		taskID := uuid.NewString()

		data, err := proto.Marshal(msg)
		if err != nil {
			logx.Errorf("proto marshal failed: %v", err)
			return err
		}

		msgType := string(msg.ProtoReflect().Descriptor().FullName())
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

	var subResults []proto.Message
	for i, tid := range taskIDs {
		resBytes, err := WaitForTaskResult(ctx, redisClient, tid, 1000)
		if err != nil {
			return err
		}

		var resultMsg taskpb.TaskResult
		if err := proto.Unmarshal(resBytes, &resultMsg); err != nil {
			return fmt.Errorf("unmarshal TaskResult failed: %v", err)
		}

		if !resultMsg.Success {
			return fmt.Errorf("task %s failed: %s", tid, resultMsg.Error)
		}

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

	if err := assign(subResults, result); err != nil {
		return fmt.Errorf("assign aggregated result failed: %v", err)
	}

	if err := SaveProtoToRedis(ctx, redisClient, key, result, ttl); err != nil {
		logx.Errorf("SaveProtoToRedis failed: %v", err)
		return err
	}

	return nil
}
