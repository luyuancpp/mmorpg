package utils

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/internal/logic/pkg/queue"
	"github.com/redis/go-redis/v9"
	"hash/fnv"
	"strconv"

	"github.com/golang/protobuf/proto"
	"github.com/zeromicro/go-zero/core/logx"
)

func BuildRedisKey(message proto.Message, playerIdStr string) string {
	return string(proto.MessageReflect(message).Descriptor().FullName()) + ":" + playerIdStr
}

func LoadFromDB(message proto.Message, playerId uint64) (*queue.MsgChannel, error) {
	playerIdStr := strconv.FormatUint(playerId, 10)

	hash := fnv.New64a()
	_, err := hash.Write([]byte(playerIdStr))
	if err != nil {
		logx.Error(err)
		return nil, err
	}
	hashKey := hash.Sum64()

	ch := &queue.MsgChannel{
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

	channels := make([]*queue.MsgChannel, 0, len(messages))
	keys := make([]string, 0, len(messages))

	for _, msg := range messages {
		key := BuildRedisKey(msg, playerIdStr)
		keys = append(keys, key)

		cmd := redisClient.Get(ctx, key)
		val, err := cmd.Bytes()
		if err == nil && len(val) > 0 {
			continue // already in Redis
		}

		ch := &queue.MsgChannel{
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
