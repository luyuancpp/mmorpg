package utils

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/internal/logic/pkg/queue"
	"errors"
	"github.com/redis/go-redis/v9"
	"hash/fnv"
	"strconv"
	"time"

	"github.com/golang/protobuf/proto"
	"github.com/zeromicro/go-zero/core/logx"
)

func BuildRedisKey(message proto.Message, playerIdStr string) string {
	return string(proto.MessageReflect(message).Descriptor().FullName()) + ":" + playerIdStr
}

func LoadFromDB(message proto.Message, playerId uint64) (*queue.MessageTask, error) {
	playerIdStr := strconv.FormatUint(playerId, 10)

	hash := fnv.New64a()
	_, err := hash.Write([]byte(playerIdStr))
	if err != nil {
		logx.Error(err)
		return nil, err
	}
	hashKey := hash.Sum64()

	ch := &queue.MessageTask{
		Key:       hashKey,
		Body:      message,
		Chan:      make(chan bool),
		WhereCase: "where player_id='" + playerIdStr + "'",
	}

	db.DB.MsgQueue.Put(*ch)
	return ch, nil
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
		logx.Error("Redis get error:", err)
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
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	hash := fnv.New64a()
	_, _ = hash.Write([]byte(playerIdStr))
	hashKey := hash.Sum64()

	channels := make([]*queue.MessageTask, 0, len(messages))
	keys := make([]string, 0, len(messages))

	for _, msg := range messages {
		key := BuildRedisKey(msg, playerIdStr)
		keys = append(keys, key)

		cmd := redisClient.Get(ctx, key)
		val, err := cmd.Bytes()
		if err == nil && len(val) > 0 {
			continue // already in Redis
		}

		ch := &queue.MessageTask{
			Key:       hashKey,
			Body:      msg,
			Chan:      make(chan bool),
			WhereCase: "where player_id='" + playerIdStr + "'",
		}
		db.DB.MsgQueue.Put(*ch)
		channels = append(channels, ch)
	}

	// 等待所有 ch
	for _, ch := range channels {
		<-ch.Chan
	}

	// 写入 Redis
	for i, msg := range messages {
		if i < len(keys) {
			err := SaveToRedis(ctx, redisClient, msg, keys[i])
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
	return false, err
	if err := proto.Unmarshal(val, msg); err != nil {
		return false, err
	}
	return true, nil
}

func SaveProtoToRedis(ctx context.Context, redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto to Redis error: %v", err)
		return err
	}
	return redis.Set(ctx, key, data, ttl).Err()
}

func LoadAggregateData(
	ctx context.Context,
	redisClient redis.Cmdable,
	playerId uint64,
	result proto.Message,
	build func(uint64) []proto.Message,
	assign func([]proto.Message, proto.Message) error,
	keyBuilder func(uint64) string,
	ttl time.Duration,
) error {
	key := keyBuilder(playerId)

	// 1. 尝试从 Redis 加载
	found, err := LoadProtoFromRedis(ctx, redisClient, key, result)
	if err != nil {
		logx.Errorf("Redis get failed: %v", err)
		return err
	}
	if found {
		return nil
	}

	// 2. Redis 未命中，从 DB 加载子结构
	playerIdStr := strconv.FormatUint(playerId, 10)
	hash := fnv.New64a()
	_, _ = hash.Write([]byte(playerIdStr))
	hashKey := hash.Sum64()

	// 构建子结构
	subMsgs := build(playerId)
	var chs []*queue.MessageTask

	for _, msg := range subMsgs {
		ch := &queue.MessageTask{
			Key:       hashKey,
			Body:      msg,
			Chan:      make(chan bool, 1),
			WhereCase: "where player_id='" + playerIdStr + "'",
			Operation: queue.OpRead,
		}
		db.DB.MsgQueue.Put(*ch)
		chs = append(chs, ch)
	}

	// 等待所有异步加载完成
	for _, ch := range chs {
		<-ch.Chan
	}

	// 聚合数据
	err = assign(subMsgs, result)
	if err != nil {
		logx.Errorf("assign aggregate data failed: %v", err)
		return err
	}

	// 存回 Redis
	err = SaveProtoToRedis(ctx, redisClient, key, result, ttl)
	if err != nil {
		logx.Errorf("Save to Redis failed: %v", err)
		return err
	}

	return nil
}
