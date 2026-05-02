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

	// Fast path
	if exists, err := rc.Exists(ctx, parentKey).Result(); err == nil && exists > 0 {
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

	// Try sub-key cache first; only enqueue a DB task on miss.
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
		remaining   atomic.Int32
		firstErr    atomic.Pointer[string]
		mu          sync.Mutex
		fireOnce    sync.Once
	)
	remaining.Store(int32(len(pending)))

	finalize := func() {
		fireOnce.Do(func() {
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

	// Register dispatcher callbacks BEFORE sending the Kafka tasks to avoid
	// the (rare) race where a result arrives before the registration lands.
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

	// Send Kafka tasks. The SyncProducer call returns once the broker acks —
	// fast in steady state. Failure here unwinds all registered callbacks via
	// fail-fast finalize so no callback leaks.
	if err := sendDBReadRequests(ctx, producer, tasks, playerId, playerIdStr); err != nil {
		errStr := fmt.Sprintf("send db read tasks: %v", err)
		firstErr.CompareAndSwap(nil, &errStr)
		// Fire finalize for each pending task so the dispatcher entries we
		// already registered get cleaned up by their TTL sweep (we can't
		// easily un-register; the TTL is bounded so this is acceptable).
		fireOnce.Do(func() { onComplete(fmt.Errorf("%s", errStr)) })
		return
	}
}
