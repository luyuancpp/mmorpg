package dataloader

import (
	"context"
	"fmt"
	"login/internal/dispatcher"
	"login/internal/kafka"
	login_proto_database "proto/common/database"
	db_proto "proto/db"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// EnsurePlayerAllDataInRedisAsync guarantees that the
// `PlayerAllData:{playerId}` Redis parent key exists and invokes `onComplete`
// exactly once when the work is finished (or has failed).
//
// Crucially, this function NEVER blocks the caller waiting for DB results.
// Sub-table fetches are dispatched via the TaskResultDispatcher (Redis Pub/Sub
// driven). When the last sub-result arrives, this function assembles the
// parent blob, SETs it in Redis, and fires onComplete.
//
// Fast path: the parent key is already cached → onComplete(nil) is invoked
// inline.
//
// Slow path: Kafka task send happens in this goroutine (SyncProducer is fast
// once the connection is warm). All result waits are event-driven — no
// goroutine sits on BLPOP.
//
// `perTaskTTL` bounds how long an individual task registration may sit
// unfulfilled before the dispatcher fires its callback with a timeout error.
// Pass 0 to use the dispatcher default.
func EnsurePlayerAllDataInRedisAsync(
	ctx context.Context,
	rc redis.Cmdable,
	disp *dispatcher.TaskResultDispatcher,
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	perTaskTTL time.Duration,
	onComplete func(error),
) {
	parent := &login_proto_database.PlayerAllData{
		PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
		PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
	}
	parentKey := buildParentKey(parent, playerId)

	// Stage 1: parent-key EXISTS check.
	cacheCheckStart := time.Now()
	exists, existsErr := rc.Exists(ctx, parentKey).Result()
	hitLabel := "miss"
	if existsErr == nil && exists > 0 {
		hitLabel = "hit"
	}
	cacheCheckSeconds.ObserveFloat(time.Since(cacheCheckStart).Seconds(), hitLabel)

	// Fast path
	if existsErr == nil && exists > 0 {
		onComplete(nil)
		return
	}

	playerIdStr := strconv.FormatUint(playerId, 10)

	type pendingRead struct {
		taskID string
		sub    proto.Message
	}
	var pending []pendingRead
	var tasks []*dbReadTask
	subs := collectSubMessages(parent)

	// Stage 2: sub-table cache GET loop.
	subCacheStart := time.Now()
	for _, sub := range subs {
		subKey := fmt.Sprintf("%s:%s", sub.ProtoReflect().Descriptor().FullName(), playerIdStr)
		if val, err := rc.Get(ctx, subKey).Bytes(); err == nil && val != nil {
			if proto.Unmarshal(val, sub) == nil {
				continue
			}
			logx.Errorf("EnsurePlayerAllDataInRedisAsync: sub cache parse failed key=%s, falling back to DB", subKey)
		}
		taskID := uuid.NewString()
		tasks = append(tasks, &dbReadTask{taskID: taskID, message: sub, subKey: subKey})
		pending = append(pending, pendingRead{taskID: taskID, sub: sub})
	}
	// Label by "needs db fetch count" so a 0-fetch path (sub-cache full
	// hit) is visually separate from a 1-2-fetch path (typical) and a
	// many-fetch path (cold cache after restart).
	subCacheCheckSeconds.ObserveFloat(
		time.Since(subCacheStart).Seconds(),
		strconv.Itoa(len(tasks)),
	)

	if len(tasks) == 0 {
		if err := saveToRedis(ctx, rc, parentKey, parent, playerAllDataParentTTL); err != nil {
			onComplete(fmt.Errorf("set parent key %s: %w", parentKey, err))
			return
		}
		onComplete(nil)
		return
	}

	// Fan-in coordinator: each registered callback decrements the counter.
	// When the counter hits zero we assemble + SET + fire onComplete. A
	// once+sync.Once wrapper guarantees onComplete fires exactly once even if
	// multiple callbacks happen to error.
	var (
		remaining atomic.Int32
		firstErr  atomic.Pointer[string]
		mu        sync.Mutex
		fireOnce  sync.Once
	)
	remaining.Store(int32(len(pending)))

	// callbackWaitStart captures t0 for stage 5 (Kafka-sent → finalize).
	// It is set inside the closure-returning helper so all timers use the
	// same clock origin; finalize then observes against this when it fires.
	var callbackWaitStart time.Time

	finalize := func() {
		fireOnce.Do(func() {
			// Stage 5 observation: wait from "kafka sent" to "finalize fires".
			// callbackWaitStart is zero only if we never got to the Kafka
			// send (registered+returned-early path); guard against that to
			// avoid logging bogus deltas.
			if !callbackWaitStart.IsZero() {
				if errPtr := firstErr.Load(); errPtr != nil {
					callbackWaitSeconds.ObserveFloat(time.Since(callbackWaitStart).Seconds(), resultFailed)
					callbackResultTotal.Inc(resultFailed)
				} else {
					callbackWaitSeconds.ObserveFloat(time.Since(callbackWaitStart).Seconds(), resultSuccess)
					callbackResultTotal.Inc(resultSuccess)
				}
			}

			if errPtr := firstErr.Load(); errPtr != nil {
				onComplete(fmt.Errorf("%s", *errPtr))
				return
			}
			if err := saveToRedis(ctx, rc, parentKey, parent, playerAllDataParentTTL); err != nil {
				onComplete(fmt.Errorf("set parent key %s: %w", parentKey, err))
				return
			}
			logx.Infof("EnsurePlayerAllDataInRedisAsync: parent written for playerId=%d (%d sub reads)", playerId, len(tasks))
			onComplete(nil)
		})
	}

	// Stage 3: dispatcher Register loop.
	// Register dispatcher callbacks BEFORE sending the Kafka tasks to avoid
	// the (rare) race where a result arrives before the registration lands.
	registerStart := time.Now()
	for _, p := range pending {
		p := p
		disp.Register(p.taskID, perTaskTTL, func(res *db_proto.TaskResult, err error) {
			defer func() {
				if remaining.Add(-1) == 0 {
					finalize()
				}
			}()

			if err != nil {
				errStr := fmt.Sprintf("task %s wait: %v", p.taskID, err)
				firstErr.CompareAndSwap(nil, &errStr)
				return
			}
			if !res.Success {
				errStr := fmt.Sprintf("db read failed for task %s: %s", p.taskID, res.Error)
				firstErr.CompareAndSwap(nil, &errStr)
				return
			}
			// Empty Data == row not found; sub stays at default (player_id only).
			if len(res.Data) > 0 {
				mu.Lock()
				if uerr := proto.Unmarshal(res.Data, p.sub); uerr != nil {
					mu.Unlock()
					errStr := fmt.Sprintf("unmarshal sub %s: %v", p.taskID, uerr)
					firstErr.CompareAndSwap(nil, &errStr)
					return
				}
				mu.Unlock()
			}
		})
	}
	dispatcherRegisterSeconds.ObserveFloat(time.Since(registerStart).Seconds())

	// Stage 4: Kafka SyncProducer.SendMessages call.
	kafkaSendStart := time.Now()
	if err := sendDBReadRequests(ctx, producer, tasks, playerId, playerIdStr); err != nil {
		kafkaSendSeconds.ObserveFloat(time.Since(kafkaSendStart).Seconds(), resultFailed)
		errStr := fmt.Sprintf("send db read tasks: %v", err)
		firstErr.CompareAndSwap(nil, &errStr)
		// Fire finalize for each pending task so the dispatcher entries we
		// already registered get cleaned up by their TTL sweep (we can't
		// easily un-register; the TTL is bounded so this is acceptable).
		fireOnce.Do(func() { onComplete(fmt.Errorf("%s", errStr)) })
		return
	}
	kafkaSendSeconds.ObserveFloat(time.Since(kafkaSendStart).Seconds(), resultSuccess)

	// Mark t0 for stage 5; the first callback that fires (and eventually
	// finalize) will observe against this.
	callbackWaitStart = time.Now()
}
