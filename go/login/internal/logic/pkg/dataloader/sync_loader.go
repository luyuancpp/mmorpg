package dataloader

import (
	"context"
	"fmt"
	"login/internal/kafka"
	login_proto "proto/db"
	"strconv"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// dbReadTask tracks a sub-message that must be fetched from the DB.
type dbReadTask struct {
	taskID  string
	message proto.Message // updated in-place when the DB result arrives
	subKey  string        // Redis cache key for this individual sub-message
}

// TriggerPlayerDataPreload fires Kafka DB-read requests for any data not already cached in Redis,
// but does NOT wait for the results (fire-and-forget).
//
// Use this during login to warm the Redis cache without blocking the EnterGame RPC.
// The scene node will load the data asynchronously via its own HandlePlayerAsyncLoaded path
// regardless — this is just a hint to pre-populate Redis before the scene asks for it.
//
// Steps:
//  1. Check parent-level Redis cache — skip entirely if cached.
//  2. Check individual sub-message Redis caches.
//  3. Send Kafka DB-read requests for sub-messages not in cache (fire-and-forget).
func TriggerPlayerDataPreload(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)

	var tasks []*dbReadTask

	for _, msg := range messages {
		parentKey := buildParentKey(msg, playerId)

		if found, _ := LoadProtoFromRedis(ctx, redisClient, parentKey, proto.Clone(msg)); found {
			logx.Debugf("TriggerPreload: parent cache hit key=%s", parentKey)
			continue
		}

		subMsgs := collectSubMessages(msg)
		for _, sub := range subMsgs {
			subKey := fmt.Sprintf("%s:%s", sub.ProtoReflect().Descriptor().FullName(), playerIdStr)

			if val, _ := redisClient.Get(ctx, subKey).Bytes(); val != nil {
				loaded := proto.Clone(sub)
				if proto.Unmarshal(val, loaded) == nil {
					logx.Debugf("TriggerPreload: sub cache hit key=%s", subKey)
					continue
				}
			}

			taskID := uuid.NewString()
			tasks = append(tasks, &dbReadTask{taskID: taskID, message: sub, subKey: subKey})
		}
	}

	if len(tasks) == 0 {
		logx.Debugf("TriggerPreload: all cached for playerId=%d", playerId)
		return nil
	}

	logx.Infof("TriggerPreload: firing %d DB read tasks (no wait) for playerId=%d", len(tasks), playerId)
	return sendDBReadRequests(ctx, kafkaProducer, tasks, playerId, playerIdStr)
}

// sendDBReadRequests sends DB read tasks to Kafka (single or batch).
func sendDBReadRequests(
	ctx context.Context,
	producer *kafka.KeyOrderedKafkaProducer,
	tasks []*dbReadTask,
	playerId uint64,
	playerIdStr string,
) error {
	dbTasks := make([]*login_proto.DBTask, len(tasks))
	for i, t := range tasks {
		body, err := proto.Marshal(t.message)
		if err != nil {
			return fmt.Errorf("marshal DB task body (type=%s): %w",
				t.message.ProtoReflect().Descriptor().FullName(), err)
		}
		dbTasks[i] = &login_proto.DBTask{
			Key:       playerId,
			WhereCase: "player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   string(t.message.ProtoReflect().Descriptor().FullName()),
			Body:      body,
			TaskId:    t.taskID,
		}
	}

	logx.Infof("LoadSync: sending %d DB read tasks for playerId=%d", len(dbTasks), playerId)
	if len(dbTasks) == 1 {
		return producer.SendTask(ctx, dbTasks[0], playerIdStr)
	}
	return producer.SendTasks(ctx, dbTasks, playerIdStr)
}

// saveToRedis marshals a proto message and saves to Redis with TTL.
func saveToRedis(ctx context.Context, rc redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal for cache key=%s: %w", key, err)
	}
	return rc.Set(ctx, key, data, ttl).Err()
}

// playerAllDataParentTTL bounds how long the assembled parent-key blob may
// stay in Redis if Scene never takes ownership (e.g. login -> immediate
// disconnect before EnterScene). Once Scene executes SavePlayerToRedis, it
// overwrites this entry persistently via its own Lua script.
const playerAllDataParentTTL = 24 * time.Hour

