package kafka

import (
	"context"
	db_config "db/internal/config"
	"db/internal/logic/pkg/proto_sql"
	"encoding/binary"
	"errors"
	"fmt"
	db_proto "proto/db"
	"runtime/debug"
	"strconv"
	"sync"
	"time"

	"github.com/luyuancpp/proto2mysql"

	"db/internal/locker"

	"shared/kafkautil"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
)

const (
	expandStatusExpireDuration = 30 * time.Minute
	taskResultExpireDuration   = 5 * time.Minute
	// taskResultNotifyChannel is the Redis Pub/Sub channel that downstream
	// services (e.g. login) subscribe to for event-driven task-result delivery.
	// Payload: the task_id string. Subscriber LPOPs the per-task list to fetch
	// the actual TaskResult bytes.
	taskResultNotifyChannel = "task:result:notify"
)

// buildCacheKey builds the same Redis key as login's cache.BuildRedisKey:
// "{MsgType}:{Key}" — e.g. "db.PlayerData:12345"
func buildCacheKey(task *db_proto.DBTask) string {
	return fmt.Sprintf("%s:%d", task.MsgType, task.Key)
}

func cacheTTL() time.Duration {
	sec := db_config.AppConfig.ServerConfig.RedisClient.DefaultTTLSeconds
	if sec <= 0 {
		return 24 * time.Hour
	}
	return time.Duration(sec) * time.Second
}

type KeyOrderedKafkaConsumer struct {
	consumer             sarama.ConsumerGroup
	redisClient          redis.Cmdable
	topic                string
	groupID              string
	partitionCount       int32
	isOfflineExpand      bool
	workers              map[int32]*worker
	wg                   *sync.WaitGroup
	ctx                  context.Context
	cancel               context.CancelFunc
	locker               *locker.RedisLocker
	retryQueueKey        string
	retryConsumeInterval time.Duration
	retryMaxTimes        int
}

type worker struct {
	partition     int32
	taskCh        chan *workerTask
	ctx           context.Context
	redisClient   redis.Cmdable
	locker        *locker.RedisLocker
	topic         string
	retryQueueKey string
	wg            *sync.WaitGroup
}

// workerTask wraps either a Kafka message or a pre-parsed retry task.
//
// seq is a per-(key, msgType) monotonic version used by the applied-seq guard
// to drop stale retries. For Kafka-origin tasks, seq = kafkaMsg.Offset + 1
// (Kafka offsets are monotonic per partition, and per-key routing puts the
// same logical key in the same partition). For retry-origin tasks, seq is
// carried in the retry queue payload prefix so retries inherit their
// original Kafka ordering. seq == 0 means "no version info" → the guard is
// a no-op (legacy/back-compat).
type workerTask struct {
	kafkaMsg *sarama.ConsumerMessage
	session  sarama.ConsumerGroupSession
	dbTask   *db_proto.DBTask // non-nil for retry tasks
	seq      uint64
}

// dbOpHandler is the handler signature for DB operations.
type dbOpHandler func(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string

var dbOpHandlers = map[string]dbOpHandler{
	"read":  handleDBReadOp,
	"write": handleDBWriteOp,
}

func handleDBReadOp(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string {
	if err := proto_sql.DB.SqlModel.FindOneByWhereClause(msg, task.WhereCase); err != nil && !errors.Is(err, proto2mysql.ErrNoRowsFound) {
		return fmt.Sprintf("db read failed: %v", err)
	}

	resultData, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Sprintf("marshal read result failed: %v", err)
	}

	// Write-back: update Redis cache so login can hit cache next time
	cacheKey := buildCacheKey(task)
	if err := redisClient.Set(ctx, cacheKey, resultData, cacheTTL()).Err(); err != nil {
		logx.Errorf("cache write-back failed on read: key=%s, taskID=%s, err=%v", cacheKey, task.TaskId, err)
	}

	// Write result to Redis (read ops only)
	if task.TaskId != "" {
		result := &db_proto.TaskResult{
			Success: true,
			Data:    resultData,
			Error:   "",
		}
		resBytes, err := proto.Marshal(result)
		if err != nil {
			return fmt.Sprintf("marshal result failed: %v", err)
		}
		resultKey := fmt.Sprintf("task:result:%s", task.TaskId)
		if err := redisClient.LPush(ctx, resultKey, resBytes).Err(); err != nil {
			return fmt.Sprintf("save read result failed: %v", err)
		}
		if err := redisClient.Expire(ctx, resultKey, taskResultExpireDuration).Err(); err != nil {
			return fmt.Sprintf("set expire for result key failed: %v", err)
		}
		// Notify event-driven subscribers (e.g. login dispatcher) so they don't
		// have to BLPOP-wait. Existing LPush + BLPOP consumers still work; this
		// is purely additive. Failure to publish only degrades latency for
		// notification-based consumers (their stale-cleanup will catch it).
		if err := redisClient.Publish(ctx, taskResultNotifyChannel, task.TaskId).Err(); err != nil {
			logx.Errorf("publish task result notify failed: taskID=%s, err=%v", task.TaskId, err)
		}
	}

	return ""
}

// appliedSeqKey is the Redis key holding the highest seq successfully
// applied for a given (topic, player key, msg type). Used by the per-key
// monotonic guard to reject stale retries.
func appliedSeqKey(topic string, key uint64, msgType string) string {
	return fmt.Sprintf("consumer:applied:%s:%d:%s", topic, key, msgType)
}

// shouldApplyBySeq returns true if the task's seq is strictly newer than
// the seq we have already applied for the same (key, msgType). On Redis
// errors it FAILS OPEN (returns true) so a flickering Redis cannot block
// data-bearing writes; the worst case is one extra apply, which is
// idempotent because writes are last-write-wins.
//
// seq == 0 disables the guard for that task (legacy/back-compat).
// Reads ("read" op) are never gated — they must always observe latest state.
func shouldApplyBySeq(ctx context.Context, rc redis.Cmdable, topic string, task *db_proto.DBTask, seq uint64) bool {
	if seq == 0 || task.Op != "write" {
		return true
	}
	cur, err := rc.Get(ctx, appliedSeqKey(topic, task.Key, task.MsgType)).Uint64()
	if err != nil {
		// redis.Nil → no prior value, apply. Other errors → fail-open.
		return true
	}
	return seq > cur
}

// markAppliedSeq records the seq as the latest successfully applied for the
// (key, msgType). Called only after a successful write completes. Within a
// single worker (which owns one Kafka partition, which owns one logical key)
// processing is serial, so a plain SET cannot race with itself for the same
// key.
func markAppliedSeq(ctx context.Context, rc redis.Cmdable, topic string, task *db_proto.DBTask, seq uint64) {
	if seq == 0 || task.Op != "write" {
		return
	}
	if err := rc.Set(ctx, appliedSeqKey(topic, task.Key, task.MsgType), seq, cacheTTL()).Err(); err != nil {
		logx.Errorf("markAppliedSeq failed: key=%d msgType=%s seq=%d err=%v",
			task.Key, task.MsgType, seq, err)
	}
}

// retryPayloadMagic identifies the new retry-queue payload format that
// carries an explicit seq prefix. The byte 0x01 is unambiguous because raw
// proto3-marshaled DBTask bytes always start with a field tag (≥ 0x08).
const retryPayloadMagic byte = 0x01

// wrapRetryPayload encodes a (seq, dbTask bytes) tuple as the on-wire retry
// payload. Format: [magic 1B][seq 8B big-endian][dbTask bytes].
func wrapRetryPayload(seq uint64, taskBytes []byte) []byte {
	buf := make([]byte, 1+8+len(taskBytes))
	buf[0] = retryPayloadMagic
	binary.BigEndian.PutUint64(buf[1:9], seq)
	copy(buf[9:], taskBytes)
	return buf
}

// unwrapRetryPayload reads the (seq, dbTask bytes) tuple. Legacy payloads
// without the magic header return seq=0 and the original bytes, so existing
// queued retries from prior versions still drain correctly (just without
// the new ordering guard).
func unwrapRetryPayload(payload []byte) (uint64, []byte) {
	if len(payload) >= 9 && payload[0] == retryPayloadMagic {
		return binary.BigEndian.Uint64(payload[1:9]), payload[9:]
	}
	return 0, payload
}

func handleDBWriteOp(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string {
	if err := proto_sql.DB.SqlModel.Save(msg); err != nil {
		return fmt.Sprintf("db write failed: %v", err)
	}

	// Write-back: update Redis cache after MySQL write (serial per key via Kafka partition)
	cacheKey := buildCacheKey(task)
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("cache write-back marshal failed: key=%s, taskID=%s, err=%v", cacheKey, task.TaskId, err)
		return ""
	}
	if err := redisClient.Set(ctx, cacheKey, data, cacheTTL()).Err(); err != nil {
		logx.Errorf("cache write-back failed on write: key=%s, taskID=%s, err=%v", cacheKey, task.TaskId, err)
	}

	return ""
}

func NewKeyOrderedKafkaConsumer(
	cfg db_config.Config,
	redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Consumer.Return.Errors = true
	config.Consumer.Offsets.Initial = sarama.OffsetOldest

	consumerGroup, err := sarama.NewConsumerGroup(
		cfg.ServerConfig.Kafka.Brokers,
		cfg.ServerConfig.Kafka.GroupID,
		config,
	)
	if err != nil {
		return nil, fmt.Errorf("create consumer group failed: groupID=%s, err=%w", cfg.ServerConfig.Kafka.GroupID, err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	wg := &sync.WaitGroup{}

	retryQueueKey := fmt.Sprintf("kafka:retry:queue:%s", cfg.ServerConfig.Kafka.Topic)
	lockerIns := locker.NewRedisLocker(redisClient)

	workers := make(map[int32]*worker)
	for i := int32(0); i < cfg.ServerConfig.Kafka.PartitionCnt; i++ {
		workers[i] = &worker{
			partition:     i,
			taskCh:        make(chan *workerTask, 1000),
			ctx:           ctx,
			redisClient:   redisClient,
			locker:        lockerIns,
			topic:         cfg.ServerConfig.Kafka.Topic,
			retryQueueKey: retryQueueKey,
			wg:            wg,
		}
	}

	return &KeyOrderedKafkaConsumer{
		consumer:             consumerGroup,
		redisClient:          redisClient,
		topic:                cfg.ServerConfig.Kafka.Topic,
		groupID:              cfg.ServerConfig.Kafka.GroupID,
		partitionCount:       cfg.ServerConfig.Kafka.PartitionCnt,
		isOfflineExpand:      cfg.ServerConfig.Kafka.IsOfflineExpand,
		workers:              workers,
		wg:                   wg,
		ctx:                  ctx,
		cancel:               cancel,
		locker:               lockerIns,
		retryQueueKey:        retryQueueKey,
		retryConsumeInterval: 1 * time.Second,
		retryMaxTimes:        3,
	}, nil
}

func (c *KeyOrderedKafkaConsumer) Start() error {
	for _, w := range c.workers {
		c.wg.Add(1)
		go w.start(c.isOfflineExpand)
	}

	c.StartRetryConsumer()

	go func() {
		for {
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, &consumerGroupHandler{consumer: c}); err != nil {
				logx.Errorf("consumer group consume failed: groupID=%s, topic=%s, err=%v", c.groupID, c.topic, err)
				time.Sleep(1 * time.Second)
			}
			if c.ctx.Err() != nil {
				logx.Infof("consumer group stopped: groupID=%s, topic=%s", c.groupID, c.topic)
				return
			}
		}
	}()

	logx.Infof("consumer started successfully: groupID=%s, topic=%s, partitionCount=%d, isOfflineExpand=%v",
		c.groupID, c.topic, c.partitionCount, c.isOfflineExpand)
	return nil
}

func (c *KeyOrderedKafkaConsumer) StartRetryConsumer() {
	go func() {
		ticker := time.NewTicker(c.retryConsumeInterval)
		defer ticker.Stop()

		for {
			select {
			case <-c.ctx.Done():
				logx.Infof("retry consumer stopped: topic=%s, retryQueueKey=%s", c.topic, c.retryQueueKey)
				return
			case <-ticker.C:
				c.consumeRetryQueue()
			}
		}
	}()
	logx.Infof("retry consumer started: topic=%s, interval=%v, maxRetryTimes=%d",
		c.topic, c.retryConsumeInterval, c.retryMaxTimes)
}

func (c *KeyOrderedKafkaConsumer) consumeRetryQueue() {
	msgBytes, err := c.redisClient.RPop(c.ctx, c.retryQueueKey).Bytes()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return
		}
		logx.Errorf("pop retry queue failed: queueKey=%s, err=%v", c.retryQueueKey, err)
		return
	}

	seq, taskBytes := unwrapRetryPayload(msgBytes)
	var task db_proto.DBTask
	if err := proto.Unmarshal(taskBytes, &task); err != nil {
		logx.Errorf("unmarshal retry task failed: err=%v, taskBytes=%v", err, taskBytes)
		return
	}

	if task.RetryCount >= int32(c.retryMaxTimes) {
		deadQueueKey := fmt.Sprintf("kafka:dead:queue:%s", c.topic)
		if err := c.redisClient.LPush(c.ctx, deadQueueKey, msgBytes).Err(); err != nil {
			logx.Errorf("failed to push to dead queue %s: taskID=%s, err=%v", deadQueueKey, task.TaskId, err)
		}
		logx.Errorf("retry task exceed max times: taskID=%s, retryCount=%d, move to dead queue: %s",
			task.TaskId, task.RetryCount, deadQueueKey)
		return
	}

	task.RetryCount++

	// Route retry task to the correct worker based on key to preserve per-key serialization
	partition := int32(task.Key % uint64(c.partitionCount))
	w, ok := c.workers[partition]
	if !ok {
		logx.Errorf("no worker for partition %d during retry: taskID=%s", partition, task.TaskId)
		return
	}

	retryTask := &workerTask{dbTask: &task, seq: seq}
	select {
	case w.taskCh <- retryTask:
		logx.Debugf("retry task routed to worker: taskID=%s, partition=%d, retryCount=%d", task.TaskId, partition, task.RetryCount)
	case <-c.ctx.Done():
		return
	}
}

func (w *worker) start(isOfflineExpand bool) {
	defer func() {
		w.wg.Done()
		logx.Infof("worker stopped: partition=%d, topic=%s", w.partition, w.topic)
	}()

	logx.Infof("worker started: partition=%d, topic=%s, isOfflineExpand=%v", w.partition, w.topic, isOfflineExpand)

	for {
		select {
		case <-w.ctx.Done():
			logx.Infof("worker received stop signal: partition=%d", w.partition)
			return
		case task, ok := <-w.taskCh:
			if !ok {
				logx.Infof("worker task channel closed: partition=%d", w.partition)
				return
			}

			// Drain all immediately available tasks from the channel
			batch := []*workerTask{task}
		drainLoop:
			for {
				select {
				case t, ok := <-w.taskCh:
					if !ok {
						break drainLoop
					}
					batch = append(batch, t)
				default:
					break drainLoop
				}
			}

			w.processTaskBatch(batch, isOfflineExpand)
		}
	}
}

// writeCoalesceKey identifies a write that can be coalesced: same player + same table.
type writeCoalesceKey struct {
	key     uint64
	msgType string
}

// processTaskBatch coalesces consecutive writes for the same (key, msg_type),
// keeping only the latest body of each "write segment". A read task acts as
// a barrier: writes that precede a read for the same (key, msg_type) are NOT
// coalesced into writes that follow it, because the read must observe the
// post-write state in MySQL (read-after-write consistency within the same
// per-key Kafka partition).
//
// All Kafka messages (including superseded ones) are MarkMessage'd for offset
// tracking.
func (w *worker) processTaskBatch(batch []*workerTask, isOfflineExpand bool) {
	if len(batch) == 1 {
		w.handleTask(batch[0], isOfflineExpand)
		return
	}

	type parsedTask struct {
		wt     *workerTask
		dbTask *db_proto.DBTask
	}
	parsed := make([]parsedTask, 0, len(batch))

	// Pass 1 (forward): unmarshal kafka payloads once per task.
	for _, wt := range batch {
		var task *db_proto.DBTask
		if wt.dbTask != nil {
			task = wt.dbTask
		} else if wt.kafkaMsg != nil {
			var t db_proto.DBTask
			if err := proto.Unmarshal(wt.kafkaMsg.Value, &t); err != nil {
				logx.Errorf("coalesce: unmarshal failed, partition=%d, offset=%d, err=%v",
					w.partition, wt.kafkaMsg.Offset, err)
				if wt.session != nil {
					wt.session.MarkMessage(wt.kafkaMsg, "")
				}
				continue
			}
			task = &t
		}
		parsed = append(parsed, parsedTask{wt: wt, dbTask: task})
	}

	// Pass 2 (reverse): mark writes that are SUPERSEDED by a later write in
	// the SAME segment (no intervening read for the same key+msgType).
	//
	// Walking backwards: when we see a write for (ck), it becomes the "live
	// last" — earlier writes for ck are superseded UNLESS a read for ck
	// appears between them, which clears the "live last" tracker.
	superseded := make([]bool, len(parsed))
	liveLastWrite := make(map[writeCoalesceKey]struct{})
	for i := len(parsed) - 1; i >= 0; i-- {
		pt := parsed[i]
		if pt.dbTask == nil {
			continue
		}
		ck := writeCoalesceKey{key: pt.dbTask.Key, msgType: pt.dbTask.MsgType}
		switch pt.dbTask.Op {
		case "write":
			if _, exists := liveLastWrite[ck]; exists {
				superseded[i] = true
			} else {
				liveLastWrite[ck] = struct{}{}
			}
		case "read":
			// Read acts as a barrier: any earlier write for ck is in a
			// different segment and must execute so the read sees post-write
			// state.
			delete(liveLastWrite, ck)
		}
	}

	// Pass 3 (forward): execute or skip.
	skipped := 0
	for i, pt := range parsed {
		if superseded[i] {
			skipped++
			logx.Debugf("coalesce: skipping superseded write: partition=%d, key=%d, msgType=%s, taskID=%s",
				w.partition, pt.dbTask.Key, pt.dbTask.MsgType, pt.dbTask.TaskId)
			if pt.wt.session != nil && pt.wt.kafkaMsg != nil {
				pt.wt.session.MarkMessage(pt.wt.kafkaMsg, "")
			}
			continue
		}
		w.handleTask(pt.wt, isOfflineExpand)
	}

	if skipped > 0 {
		logx.Infof("coalesce: partition=%d, batch=%d, skipped=%d writes", w.partition, len(batch), skipped)
	}
}

func (w *worker) handleTask(task *workerTask, isOfflineExpand bool) {
	// Per-message panic recovery: a single bad message won't kill the partition worker
	defer func() {
		if r := recover(); r != nil {
			logx.Errorf("worker panic recovered: partition=%d, panic=%v, stack=%s",
				w.partition, r, string(debug.Stack()))
		}
	}()

	// Resolve dbTask up front (handles both Kafka-origin and retry-origin
	// tasks). We need dbTask available for both the seq guard AND the retry
	// queue save path on failure — historically Kafka-origin failures were
	// silently dropped here (data loss).
	var dbTask *db_proto.DBTask
	if task.dbTask != nil {
		dbTask = task.dbTask
	} else if task.kafkaMsg != nil {
		var t db_proto.DBTask
		if err := proto.Unmarshal(task.kafkaMsg.Value, &t); err != nil {
			logx.Errorf("worker unmarshal kafka task failed: partition=%d, offset=%d, err=%v",
				w.partition, task.kafkaMsg.Offset, err)
			if task.session != nil {
				task.session.MarkMessage(task.kafkaMsg, "")
			}
			return
		}
		dbTask = &t
	} else {
		return
	}

	// Per-key monotonic seq guard: drop stale retries that would otherwise
	// overwrite newer state already persisted by a more-recent task. Done
	// BEFORE dispatch so we save the wasted MySQL/Redis round-trip.
	if !shouldApplyBySeq(w.ctx, w.redisClient, w.topic, dbTask, task.seq) {
		logx.Infof("seq guard: dropping stale write: partition=%d key=%d msgType=%s seq=%d taskID=%s",
			w.partition, dbTask.Key, dbTask.MsgType, task.seq, dbTask.TaskId)
		if task.session != nil && task.kafkaMsg != nil {
			task.session.MarkMessage(task.kafkaMsg, "")
		}
		return
	}

	startTime := time.Now()
	err := processDBTask(w.ctx, w, dbTask, w.partition, task.seq, isOfflineExpand)

	if err != nil {
		logx.Errorf("worker process task failed: partition=%d, cost=%v, err=%v",
			w.partition, time.Since(startTime), err)
		// Re-queue ALL failed tasks (kafka-origin and retry-origin alike) for
		// another attempt. Carry the original seq so the per-key guard still
		// applies to the retry. Without this, kafka-origin failures used to
		// be silently dropped while their offset was committed → data loss.
		if saveErr := saveToRetryQueue(w.ctx, w.redisClient, w.retryQueueKey, dbTask, task.seq); saveErr != nil {
			logx.Errorf("failed to re-queue retry task: taskID=%s, err=%v", dbTask.TaskId, saveErr)
		}
	} else {
		logx.Debugf("worker process task success: partition=%d, cost=%v",
			w.partition, time.Since(startTime))
		// Update applied seq ONLY after a successful write so that a failed
		// write does not falsely block its own retry.
		markAppliedSeq(w.ctx, w.redisClient, w.topic, dbTask, task.seq)
	}

	// Mark Kafka offset AFTER processing completes (not at dispatch time)
	if task.session != nil && task.kafkaMsg != nil {
		task.session.MarkMessage(task.kafkaMsg, "")
	}
}

type consumerGroupHandler struct {
	consumer *KeyOrderedKafkaConsumer
}

func (h *consumerGroupHandler) Setup(session sarama.ConsumerGroupSession) error {
	claims := session.Claims()
	if partitions, ok := claims[h.consumer.topic]; ok {
		partitionIDs := make([]int32, 0, len(partitions))
		for p := range partitions {
			partitionIDs = append(partitionIDs, int32(p))
		}
		logx.Infof("consumer group assigned partitions: groupID=%s, topic=%s, partitions=%v",
			h.consumer.groupID, h.consumer.topic, partitionIDs)
	} else {
		logx.Errorf("no partitions assigned: groupID=%s, topic=%s", h.consumer.groupID, h.consumer.topic)
	}
	return nil
}

func (h *consumerGroupHandler) Cleanup(session sarama.ConsumerGroupSession) error {
	claims := session.Claims()
	if partitions, ok := claims[h.consumer.topic]; ok {
		partitionIDs := make([]int32, 0, len(partitions))
		for p := range partitions {
			partitionIDs = append(partitionIDs, int32(p))
		}
		logx.Infof("consumer group releasing partitions: groupID=%s, topic=%s, partitions=%v",
			h.consumer.groupID, h.consumer.topic, partitionIDs)
	}
	return nil
}

func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	logx.Infof("ConsumeClaim invoked: partition=%d, initialOffset=%d",
		claim.Partition(), claim.InitialOffset())

	for msg := range claim.Messages() {
		logx.Debugf("fetched message: topic=%s, partition=%d, offset=%d, key=%s",
			msg.Topic, msg.Partition, msg.Offset, string(msg.Key))

		partition := msg.Partition
		w, ok := h.consumer.workers[partition]
		if !ok {
			logx.Errorf("no worker found for partition: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			session.MarkMessage(msg, "")
			continue
		}

		// seq = offset + 1 so that seq=0 sentinel ("no version info") is
		// reserved and never collides with a real Kafka offset.
		task := &workerTask{
			kafkaMsg: msg,
			session:  session,
			seq:      uint64(msg.Offset) + 1,
		}

		// Block until worker accepts the task or context is canceled.
		// Never drop messages — the worker will mark the offset after processing.
		select {
		case w.taskCh <- task:
			logx.Debugf("message dispatched to worker: partition=%d, offset=%d", partition, msg.Offset)
		case <-w.ctx.Done():
			logx.Infof("worker context canceled, stop dispatching: topic=%s, partition=%d",
				h.consumer.topic, partition)
			return nil
		}
	}
	return nil
}

func processDBTask(ctx context.Context, w *worker, task *db_proto.DBTask, partition int32, seq uint64, isOfflineExpand bool) error {
	key := strconv.FormatUint(task.Key, 10)
	logx.Debugf("received db task: taskID=%s, key=%s, partition=%d, seq=%d, isOfflineExpand=%v",
		task.TaskId, key, partition, seq, isOfflineExpand)

	if isOfflineExpand {
		logx.Debugf("offline expand mode: skip lock/status check, taskID=%s", task.TaskId)
		return processTaskWithoutLock(ctx, w.redisClient, task)
	}

	expandStatus, err := kafkautil.GetExpandStatus(ctx, w.redisClient, w.topic)
	if err != nil {
		logx.Errorf("get expand status failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		return tryLockAndProcess(ctx, w, key, task, seq)
	}

	currentTime := time.Now().UnixMilli()
	if expandStatus.Status == kafkautil.ExpandStatusExpanding &&
		(expandStatus.UpdateTime == 0 || currentTime-expandStatus.UpdateTime > expandStatusExpireDuration.Milliseconds()) {
		logx.Errorf("expand status expired: topic=%s, key=%s, lastUpdate=%d",
			w.topic, key, expandStatus.UpdateTime)
		expandStatus.Status = kafkautil.ExpandStatusNormal
		_ = kafkautil.SetExpandStatus(ctx, w.redisClient, w.topic, kafkautil.ExpandStatusNormal, expandStatus.PartitionCount)
	}

	if expandStatus.Status == kafkautil.ExpandStatusExpanding {
		logx.Debugf("expanding mode: try lock for task: taskID=%s, key=%s", task.TaskId, key)
		return tryLockAndProcess(ctx, w, key, task, seq)
	}

	return processTaskWithoutLock(ctx, w.redisClient, task)
}

func tryLockAndProcess(ctx context.Context, worker *worker, key string, task *db_proto.DBTask, seq uint64) error {
	lockKey := fmt.Sprintf("kafka:consumer:lock:%s", key)
	lockTTL := 5 * time.Second

	tryLock, err := worker.locker.TryLock(ctx, lockKey, lockTTL)
	if err != nil {
		return fmt.Errorf("try lock failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
	}
	if !tryLock.IsLocked() {
		if err := saveToRetryQueue(ctx, worker.redisClient, worker.retryQueueKey, task, seq); err != nil {
			return fmt.Errorf("save to retry queue failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
		}
		logx.Debugf("lock occupied: task saved to retry queue: taskID=%s, key=%s, seq=%d", task.TaskId, key, seq)
		return nil
	}

	defer func() {
		if _, err := tryLock.Release(ctx); err != nil {
			logx.Errorf("release lock failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		}
	}()

	return processTaskWithoutLock(ctx, worker.redisClient, task)
}

func processTaskWithoutLock(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask) error {
	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		return fmt.Errorf("find message type failed: type=%s, taskID=%s, err=%w", task.MsgType, task.TaskId, err)
	}

	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		return fmt.Errorf("unmarshal task body failed: taskID=%s, err=%w", task.TaskId, err)
	}

	handler, ok := dbOpHandlers[task.Op]
	var resultErr string
	if !ok {
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	} else {
		resultErr = handler(ctx, redisClient, task, msg)
	}

	logx.Infof("task processed: taskID=%s, op=%s, success=%v, err=%s",
		task.TaskId, task.Op, resultErr == "", resultErr)

	if resultErr != "" {
		return fmt.Errorf("%s", resultErr)
	}
	return nil
}

func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask, seq uint64) error {
	taskBytes, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task for retry failed: taskID=%s, err=%w", task.TaskId, err)
	}
	return redisClient.LPush(ctx, retryQueueKey, wrapRetryPayload(seq, taskBytes)).Err()
}

func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()
	c.wg.Wait()
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("close consumer group failed: groupID=%s, err=%v", c.groupID, err)
	} else {
		logx.Infof("consumer group closed: groupID=%s, topic=%s", c.groupID, c.topic)
	}
}
