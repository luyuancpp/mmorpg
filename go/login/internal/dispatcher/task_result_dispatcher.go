// Package dispatcher hosts process-wide event-driven dispatchers. The
// TaskResultDispatcher converts DB-task result delivery from a polling/BLPOP
// model into a callback registry driven by a single Redis Pub/Sub subscriber
// goroutine. Hot paths (gRPC handlers, preload pool tasks) register a callback
// for a taskID and return immediately — no goroutine is left waiting on Redis.
package dispatcher

import (
	"context"
	"fmt"
	"sync"
	"time"

	db_proto "proto/db"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// TaskResultNotifyChannel must match the channel published by the DB consumer.
const TaskResultNotifyChannel = "task:result:notify"

// ResultCallback receives the decoded TaskResult, or an error if the result
// could not be fetched (timeout, decode failure, etc.).
type ResultCallback func(res *db_proto.TaskResult, err error)

type pendingEntry struct {
	cb        ResultCallback
	expiresAt time.Time
}

// TaskResultDispatcher is a singleton-style component owned by the service
// context. Lifecycle: NewTaskResultDispatcher → Start → (Register many times)
// → Stop on shutdown.
type TaskResultDispatcher struct {
	rc         redis.UniversalClient
	defaultTTL time.Duration

	mu      sync.Mutex
	pending map[string]pendingEntry

	stopCh chan struct{}
	doneCh chan struct{}
}

// NewTaskResultDispatcher constructs the dispatcher. defaultTTL bounds how long
// a registration may sit unfulfilled before its callback is fired with a
// timeout error — protects against DB drops / pub/sub loss / etc.
func NewTaskResultDispatcher(rc redis.UniversalClient, defaultTTL time.Duration) *TaskResultDispatcher {
	if defaultTTL <= 0 {
		defaultTTL = 30 * time.Second
	}
	return &TaskResultDispatcher{
		rc:         rc,
		defaultTTL: defaultTTL,
		pending:    make(map[string]pendingEntry),
		stopCh:     make(chan struct{}),
		doneCh:     make(chan struct{}),
	}
}

// Start launches the subscriber goroutine and the periodic GC sweep.
func (d *TaskResultDispatcher) Start() {
	go d.run()
}

// Stop terminates background goroutines and fires every pending callback with
// a shutdown error so EnterGame chains can unwind cleanly.
func (d *TaskResultDispatcher) Stop() {
	close(d.stopCh)
	<-d.doneCh
	d.mu.Lock()
	for _, e := range d.pending {
		go e.cb(nil, fmt.Errorf("dispatcher stopped"))
	}
	d.pending = nil
	d.mu.Unlock()
}

// Register associates `cb` with `taskID`. The callback is invoked exactly once
// when a notification arrives, when the registration TTL elapses, or on Stop.
// `ttl<=0` uses the dispatcher default.
func (d *TaskResultDispatcher) Register(taskID string, ttl time.Duration, cb ResultCallback) {
	if ttl <= 0 {
		ttl = d.defaultTTL
	}
	d.mu.Lock()
	if d.pending == nil {
		d.mu.Unlock()
		go cb(nil, fmt.Errorf("dispatcher stopped"))
		return
	}
	d.pending[taskID] = pendingEntry{cb: cb, expiresAt: time.Now().Add(ttl)}
	d.mu.Unlock()
}

// take atomically removes and returns the callback for taskID (or nil if none).
func (d *TaskResultDispatcher) take(taskID string) ResultCallback {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.pending == nil {
		return nil
	}
	e, ok := d.pending[taskID]
	if !ok {
		return nil
	}
	delete(d.pending, taskID)
	return e.cb
}

// run subscribes to the notify channel and dispatches incoming taskIDs. A
// secondary ticker sweeps expired registrations.
func (d *TaskResultDispatcher) run() {
	defer close(d.doneCh)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go func() {
		<-d.stopCh
		cancel()
	}()

	gc := time.NewTicker(d.defaultTTL / 2)
	defer gc.Stop()

	for {
		// Restart the subscription on any error (Redis disconnect, etc.) with
		// a small backoff to avoid hot-loops.
		if err := d.subscribeLoop(ctx, gc); err != nil {
			if ctx.Err() != nil {
				return
			}
			logx.Errorf("TaskResultDispatcher subscribeLoop error: %v", err)
			select {
			case <-time.After(time.Second):
			case <-ctx.Done():
				return
			}
		} else {
			return
		}
	}
}

func (d *TaskResultDispatcher) subscribeLoop(ctx context.Context, gc *time.Ticker) error {
	psub := d.rc.Subscribe(ctx, TaskResultNotifyChannel)
	defer psub.Close()

	// Confirm subscription is live before processing.
	if _, err := psub.Receive(ctx); err != nil {
		return fmt.Errorf("subscribe receive: %w", err)
	}

	ch := psub.Channel()
	for {
		select {
		case <-ctx.Done():
			return nil
		case <-gc.C:
			d.sweepExpired()
		case msg, ok := <-ch:
			if !ok {
				return fmt.Errorf("pubsub channel closed")
			}
			d.dispatch(ctx, msg.Payload)
		}
	}
}

// dispatch handles one notification for taskID.
func (d *TaskResultDispatcher) dispatch(ctx context.Context, taskID string) {
	cb := d.take(taskID)
	if cb == nil {
		// Either belongs to a different process (pub/sub broadcasts to all
		// subscribers) or already expired — both safe to ignore.
		return
	}

	resultKey := fmt.Sprintf("task:result:%s", taskID)
	bytesVal, err := d.rc.LPop(ctx, resultKey).Bytes()
	if err != nil {
		go cb(nil, fmt.Errorf("lpop %s: %w", resultKey, err))
		return
	}
	res := &db_proto.TaskResult{}
	if err := proto.Unmarshal(bytesVal, res); err != nil {
		go cb(nil, fmt.Errorf("unmarshal task result %s: %w", taskID, err))
		return
	}
	// Fire callback in its own goroutine so a slow callback can't stall the
	// subscriber loop.
	go cb(res, nil)
}

// sweepExpired fires timeout callbacks for entries whose TTL has elapsed. This
// catches lost notifications (DB crash between LPush and Publish, network
// blip, subscriber miss during reconnect, etc.).
func (d *TaskResultDispatcher) sweepExpired() {
	now := time.Now()
	var expired []ResultCallback

	d.mu.Lock()
	for id, e := range d.pending {
		if now.After(e.expiresAt) {
			expired = append(expired, e.cb)
			delete(d.pending, id)
		}
	}
	d.mu.Unlock()

	for _, cb := range expired {
		go cb(nil, fmt.Errorf("task result wait timed out"))
	}
}
