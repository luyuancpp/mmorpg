package dataloader

import (
	"context"
	"errors"
	"fmt"
	"login/internal/config"
	"login/internal/kafka"
	login_proto "proto/db"
	"strconv"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// dbReadTask tracks a sub-message that must be fetched from the DB.
type dbReadTask struct {
	taskID  string
	message proto.Message // updated in-place when the DB result arrives
	subKey  string        // Redis cache key for this individual sub-message
}

// parentEntry tracks a parent message and the sub-messages that compose it.
type parentEntry struct {
	cacheKey    string
	template    proto.Message   // original parent message (used for cloning during assembly)
	subMessages []proto.Message // all sub-messages — some from cache, others filled by DB
	hasMultiSub bool            // true when re-assembly into the parent is needed
}

// LoadPlayerDataSync loads player data from Redis cache or MySQL (via Kafka + DB service).
// This is a synchronous, blocking call — it returns when all data is loaded (or an error occurs).
//
// Steps:
//  1. Check parent-level Redis cache — skip entirely if cached.
//  2. Check individual sub-message Redis caches.
//  3. Send Kafka DB-read requests for sub-messages not in cache.
//  4. Wait for DB results via Redis BLPop.
//  5. Cache results and re-assemble parent messages.
func LoadPlayerDataSync(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	messages []proto.Message,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)

	var tasks []*dbReadTask
	var parents []*parentEntry
	taskByID := make(map[string]*dbReadTask)

	// ── Steps 1–2: Check caches, collect what needs DB loading ─────────────
	for _, msg := range messages {
		parentKey := buildParentKey(msg, playerId)

		// Step 1: parent-level cache (fast path)
		if found, _ := LoadProtoFromRedis(ctx, redisClient, parentKey, proto.Clone(msg)); found {
			logx.Debugf("LoadSync: parent cache hit key=%s", parentKey)
			continue
		}

		// Step 2: check each sub-message
		subMsgs := collectSubMessages(msg)
		pe := &parentEntry{
			cacheKey:    parentKey,
			template:    msg,
			hasMultiSub: len(subMsgs) > 1,
		}

		for _, sub := range subMsgs {
			subKey := fmt.Sprintf("%s:%s", sub.ProtoReflect().Descriptor().FullName(), playerIdStr)

			// Try sub-message cache
			if val, _ := redisClient.Get(ctx, subKey).Bytes(); val != nil {
				loaded := proto.Clone(sub)
				if proto.Unmarshal(val, loaded) == nil {
					pe.subMessages = append(pe.subMessages, loaded)
					logx.Debugf("LoadSync: sub cache hit key=%s", subKey)
					continue
				}
			}

			// Cache miss — will be loaded from DB
			taskID := uuid.NewString()
			t := &dbReadTask{taskID: taskID, message: sub, subKey: subKey}
			tasks = append(tasks, t)
			taskByID[taskID] = t
			pe.subMessages = append(pe.subMessages, sub) // same pointer; updated in step 4
		}

		parents = append(parents, pe)
	}

	if len(tasks) == 0 {
		logx.Debugf("LoadSync: all cached for playerId=%d", playerId)
		return nil
	}

	// ── Step 3: Send DB read requests via Kafka ────────────────────────────
	if err := sendDBReadRequests(ctx, kafkaProducer, tasks, playerId, playerIdStr); err != nil {
		return err
	}

	// ── Step 4: Wait for all DB results via Redis BLPop ────────────────────
	if err := waitForDBResults(ctx, redisClient, tasks, taskByID); err != nil {
		return err
	}

	// ── Step 5: Re-assemble and cache parent messages ──────────────────────
	return assembleAndCacheParents(ctx, redisClient, parents)
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

// waitForDBResults polls Redis BLPop until all task results arrive.
// Each task's message is updated in-place with the DB data.
func waitForDBResults(
	ctx context.Context,
	redisClient redis.Cmdable,
	tasks []*dbReadTask,
	taskByID map[string]*dbReadTask,
) error {
	resultKeys := make([]string, len(tasks))
	for i, t := range tasks {
		resultKeys[i] = "task:result:" + t.taskID
	}

	// Always clean up result keys when done (prevent leaks)
	defer func() {
		cleanCtx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()
		if len(resultKeys) > 0 {
			redisClient.Del(cleanCtx, resultKeys...)
		}
	}()

	completed := 0
	for completed < len(tasks) {
		result, err := redisClient.BLPop(ctx, 1*time.Second, resultKeys...).Result()
		if err != nil {
			if errors.Is(err, redis.Nil) {
				continue // BLPop timeout, retry
			}
			return fmt.Errorf("BLPop failed (%d/%d completed): %w", completed, len(tasks), err)
		}
		if len(result) != 2 {
			continue
		}

		var taskID string
		if _, scanErr := fmt.Sscanf(result[0], "task:result:%s", &taskID); scanErr != nil {
			logx.Errorf("LoadSync: cannot parse result key %q: %v", result[0], scanErr)
			continue
		}
		t, ok := taskByID[taskID]
		if !ok {
			continue
		}

		// Parse and validate result
		var taskResult login_proto.TaskResult
		if err := proto.Unmarshal([]byte(result[1]), &taskResult); err != nil {
			return fmt.Errorf("unmarshal task result (taskID=%s): %w", taskID, err)
		}
		if !taskResult.Success {
			return fmt.Errorf("DB read failed (taskID=%s): %s", taskID, taskResult.Error)
		}

		// Fill the sub-message in-place with DB data
		if err := proto.Unmarshal(taskResult.Data, t.message); err != nil {
			return fmt.Errorf("unmarshal DB data (taskID=%s, type=%s): %w",
				taskID, t.message.ProtoReflect().Descriptor().FullName(), err)
		}

		// Cache loaded sub-message
		if cacheErr := saveToRedis(ctx, redisClient, t.subKey, t.message,
			config.AppConfig.Timeouts.RoleCacheExpire); cacheErr != nil {
			logx.Errorf("LoadSync: cache sub failed key=%s: %v", t.subKey, cacheErr)
		}

		completed++
		logx.Debugf("LoadSync: task done %d/%d taskID=%s", completed, len(tasks), taskID)
	}

	return nil
}

// assembleAndCacheParents re-assembles parent messages from loaded sub-messages and caches them.
func assembleAndCacheParents(ctx context.Context, redisClient redis.Cmdable, parents []*parentEntry) error {
	for _, p := range parents {
		if !p.hasMultiSub {
			continue
		}
		assembled, err := assembleParent(p.template, p.subMessages)
		if err != nil {
			return fmt.Errorf("assemble parent %s: %w", p.cacheKey, err)
		}
		if cacheErr := saveToRedis(ctx, redisClient, p.cacheKey, assembled,
			config.AppConfig.Timeouts.RoleCacheExpire); cacheErr != nil {
			logx.Errorf("LoadSync: cache parent failed key=%s: %v", p.cacheKey, cacheErr)
		}
	}
	return nil
}

// assembleParent sets loaded sub-messages into a cloned parent proto by matching field types.
func assembleParent(template proto.Message, subMsgs []proto.Message) (proto.Message, error) {
	parent := proto.Clone(template)
	pr := parent.ProtoReflect()
	fields := pr.Descriptor().Fields()

	for _, sub := range subMsgs {
		typeName := string(sub.ProtoReflect().Descriptor().Name())
		matched := false
		for i := 0; i < fields.Len(); i++ {
			f := fields.Get(i)
			if f.Kind() == protoreflect.MessageKind && string(f.Message().Name()) == typeName {
				pr.Set(f, protoreflect.ValueOf(sub.ProtoReflect()))
				matched = true
				break
			}
		}
		if !matched {
			return nil, fmt.Errorf("parent %s has no field for sub-type %s",
				template.ProtoReflect().Descriptor().FullName(), typeName)
		}
	}
	return parent, nil
}

// saveToRedis marshals a proto message and saves to Redis with TTL.
func saveToRedis(ctx context.Context, rc redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal for cache key=%s: %w", key, err)
	}
	return rc.Set(ctx, key, data, ttl).Err()
}
