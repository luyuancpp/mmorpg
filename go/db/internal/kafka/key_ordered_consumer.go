package kafka

import (
	"context"
	db_config "db/internal/config"
	"db/internal/logic/pkg/proto_sql"
	db_proto "db/proto/service/go/grpc/db"
	"errors"
	"fmt"
	"github.com/luyuancpp/proto2mysql-go"
	"runtime/debug"
	"strconv"
	"sync"
	"time"

	"db/internal/locker"
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
	// 新增：定义扩容状态常量（之前缺失）
)

// ExpandStatus 扩容状态结构体（之前缺失）
type ExpandStatus struct {
	Status         string
	PartitionCount int32
	UpdateTime     int64
}

type KafkaConsumerConfig struct {
	BootstrapServers string `yaml:"bootstrapServers"`
	GroupID          string `yaml:"groupID"`
	Topic            string `yaml:"topic"`
	PartitionCount   int32  `yaml:"partitionCount"`
	IsOfflineExpand  bool   `yaml:"isOfflineExpand"`
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
	msgCh         chan *sarama.ConsumerMessage
	ctx           context.Context
	redisClient   redis.Cmdable
	locker        *locker.RedisLocker
	topic         string
	retryQueueKey string
	wg            *sync.WaitGroup
}

func NewKeyOrderedKafkaConsumer(
	cfg db_config.Config,
	redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0 // 与 v1.43.1 兼容的 Kafka 版本
	config.Consumer.Return.Errors = true
	config.Consumer.Offsets.Initial = sarama.OffsetOldest

	consumerGroup, err := sarama.NewConsumerGroup(
		[]string{cfg.ServerConfig.Kafka.Brokers},
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
			msgCh:         make(chan *sarama.ConsumerMessage, 1000),
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
		go w.start(processDBTaskMessage, c.isOfflineExpand)
	}

	c.StartRetryConsumer()

	go func() {
		for {
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, &consumerGroupHandler{consumer: c}); err != nil {
				logx.Errorf("consumer group consume failed: groupID=%s, topic=%s, err=%v", c.groupID, c.topic, err)
				time.Sleep(1 * time.Second)
			}
			if c.ctx.Err() != nil {
				logx.Info("consumer group stopped: groupID=%s, topic=%s", c.groupID, c.topic)
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
				logx.Info("retry consumer stopped: topic=%s, retryQueueKey=%s", c.topic, c.retryQueueKey)
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

	var task db_proto.DBTask
	if err := proto.Unmarshal(msgBytes, &task); err != nil {
		logx.Errorf("unmarshal retry task failed: err=%v, taskBytes=%v", err, msgBytes)
		return
	}

	if task.RetryCount >= int32(c.retryMaxTimes) {
		deadQueueKey := fmt.Sprintf("kafka:dead:queue:%s", c.topic)
		_ = c.redisClient.LPush(c.ctx, deadQueueKey, msgBytes)
		logx.Errorf("retry task exceed max times: taskID=%s, retryCount=%d, move to dead queue: %s",
			task.TaskId, task.RetryCount, deadQueueKey)
		return
	}

	task.RetryCount++
	if err := processTaskWithoutLock(c.ctx, c.redisClient, &task); err != nil {
		logx.Errorf("retry process task failed: taskID=%s, retryCount=%d, err=%v", task.TaskId, task.RetryCount, err)
		retryTaskBytes, _ := proto.Marshal(&task)
		_ = c.redisClient.LPush(c.ctx, c.retryQueueKey, retryTaskBytes)
		return
	}

	logx.Infof("retry process task success: taskID=%s, retryCount=%d", task.TaskId, task.RetryCount)
}

func (w *worker) start(
	processFunc func(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage, isOfflineExpand bool) error,
	isOfflineExpand bool,
) {
	defer func() {
		close(w.msgCh)
		w.wg.Done()
		logx.Infof("worker stopped: partition=%d, topic=%s", w.partition, w.topic)
	}()

	defer func() {
		if r := recover(); r != nil {
			logx.Errorf("worker panic recovered: partition=%d, panic=%v, stack=%s",
				w.partition, r, string(debug.Stack()))
		}
	}()

	logx.Infof("worker started: partition=%d, topic=%s, isOfflineExpand=%v", w.partition, w.topic, isOfflineExpand)

	for {
		select {
		case <-w.ctx.Done():
			logx.Infof("worker received stop signal: partition=%d", w.partition)
			return
		case msg, ok := <-w.msgCh:
			if !ok {
				logx.Infof("worker msg channel closed: partition=%d", w.partition)
				return
			}
			startTime := time.Now()
			if err := processFunc(w.ctx, w, msg, isOfflineExpand); err != nil {
				logx.Errorf("worker process msg failed: partition=%d, offset=%d, cost=%v, err=%v",
					w.partition, msg.Offset, time.Since(startTime), err)
			} else {
				logx.Debugf("worker process msg success: partition=%d, offset=%d, cost=%v",
					w.partition, msg.Offset, time.Since(startTime))
			}
		}
	}
}

type consumerGroupHandler struct {
	consumer *KeyOrderedKafkaConsumer
}

// Setup 实现 sarama.ConsumerGroupHandler 接口
// 在 v1.43.1 中通过 Claims() 获取分配的分区
func (h *consumerGroupHandler) Setup(session sarama.ConsumerGroupSession) error {
	// Claims() 返回 map[topic]map[int32]sarama.OffsetMetadata
	claims := session.Claims()
	if partitions, ok := claims[h.consumer.topic]; ok {
		// 提取分区 ID 列表
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

// Cleanup 实现 sarama.ConsumerGroupHandler 接口
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

// ConsumeClaim 实现 sarama.ConsumerGroupHandler 接口
func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	logx.Infof("ConsumeClaim invoked: partition=%d, initialOffset=%d",
		claim.Partition(), claim.InitialOffset())

	// 从 claim 通道读取消息
	for msg := range claim.Messages() {
		logx.Debugf("fetched message: topic=%s, partition=%d, offset=%d, key=%s",
			msg.Topic, msg.Partition, msg.Offset, string(msg.Key))

		partition := msg.Partition
		worker, ok := h.consumer.workers[partition]
		if !ok {
			logx.Errorf("no worker found for partition: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			session.MarkMessage(msg, "")
			continue
		}

		// 发送消息到 worker 通道
		select {
		case worker.msgCh <- msg:
			session.MarkMessage(msg, "")
			logx.Debugf("message dispatched to worker: partition=%d, offset=%d", partition, msg.Offset)
		case <-worker.ctx.Done():
			logx.Info("worker context canceled, stop dispatching: topic=%s, partition=%d",
				h.consumer.topic, partition)
			return nil
		case <-time.After(100 * time.Millisecond):
			logx.Errorf("worker msg channel full, retrying: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			time.Sleep(50 * time.Millisecond)

			select {
			case worker.msgCh <- msg:
				session.MarkMessage(msg, "")
				logx.Debugf("message dispatched after retry: partition=%d, offset=%d", partition, msg.Offset)
			default:
				logx.Errorf("worker channel still full, drop message: partition=%d, offset=%d", partition, msg.Offset)
				session.MarkMessage(msg, "")
			}
		}
	}
	return nil
}

func processDBTaskMessage(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage, isOfflineExpand bool) error {
	var task db_proto.DBTask
	if err := proto.Unmarshal(msg.Value, &task); err != nil {
		return fmt.Errorf("unmarshal task failed: offset=%d, err=%w", msg.Offset, err)
	}
	key := strconv.FormatUint(task.Key, 10)
	logx.Debugf("received db task: taskID=%s, key=%s, partition=%d, isOfflineExpand=%v",
		task.TaskId, key, msg.Partition, isOfflineExpand)

	if isOfflineExpand {
		logx.Debugf("offline expand mode: skip lock/status check, taskID=%s", task.TaskId)
		return processTaskWithoutLock(ctx, worker.redisClient, &task)
	}

	expandStatus, err := GetExpandStatus(ctx, worker.redisClient, worker.topic)
	if err != nil {
		logx.Errorf("get expand status failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	currentTime := time.Now().UnixMilli()
	if expandStatus.Status == ExpandStatusExpanding &&
		(expandStatus.UpdateTime == 0 || currentTime-expandStatus.UpdateTime > expandStatusExpireDuration.Milliseconds()) {
		logx.Errorf("expand status expired: topic=%s, key=%s, lastUpdate=%d",
			worker.topic, key, expandStatus.UpdateTime)
		expandStatus.Status = ExpandStatusNormal
		_ = SetExpandStatus(ctx, worker.redisClient, worker.topic, ExpandStatusNormal, expandStatus.PartitionCount)
	}

	if expandStatus.Status == ExpandStatusExpanding {
		logx.Debugf("expanding mode: try lock for task: taskID=%s, key=%s", task.TaskId, key)
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	return processTaskWithoutLock(ctx, worker.redisClient, &task)
}

func tryLockAndProcess(ctx context.Context, worker *worker, key string, task *db_proto.DBTask, offset int64) error {
	lockKey := fmt.Sprintf("kafka:consumer:lock:%s", key)
	lockTTL := 5 * time.Second

	tryLock, err := worker.locker.TryLock(ctx, lockKey, lockTTL)
	if err != nil {
		return fmt.Errorf("try lock failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
	}
	if !tryLock.IsLocked() {
		if err := saveToRetryQueue(ctx, worker.redisClient, worker.retryQueueKey, task); err != nil {
			return fmt.Errorf("save to retry queue failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
		}
		logx.Debugf("lock occupied: task saved to retry queue: taskID=%s, key=%s", task.TaskId, key)
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

	var resultErr string
	var resultData []byte
	switch task.Op {
	case "read":
		if err := proto_sql.DB.SqlModel.FindOneByWhereClause(msg, task.WhereCase); err != nil && !errors.Is(err, proto2mysql.ErrNoRowsFound) {
			resultErr = fmt.Sprintf("db read failed: %v", err)
		} else {
			if resultData, err = proto.Marshal(msg); err != nil {
				resultErr = fmt.Sprintf("marshal read result failed: %v", err)
			}
		}
	case "write":
		if err := proto_sql.DB.SqlModel.Save(msg); err != nil {
			resultErr = fmt.Sprintf("db write failed: %v", err)
		}
	default:
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	}

	// 在 processTaskWithoutLock 函数中，修改 LPush 的 Key
	if task.Op == "read" && task.TaskId != "" {
		result := &db_proto.TaskResult{
			Success: resultErr == "",
			Data:    resultData,
			Error:   resultErr,
		}
		resBytes, err := proto.Marshal(result)
		if err != nil {
			return fmt.Errorf("marshal result failed: taskID=%s, err=%w", task.TaskId, err)
		}
		// 关键修改：使用 "task:{taskID}" 作为Key，与 taskmanager 保持一致
		resultKey := fmt.Sprintf("%s", task.TaskId)
		if err := redisClient.LPush(ctx, resultKey, resBytes).Err(); err != nil {
			return fmt.Errorf("save read result failed: taskID=%s, err=%w", task.TaskId, err)
		}
	}

	logx.Infof("task processed: taskID=%s, op=%s, success=%v, err=%s",
		task.TaskId, task.Op, resultErr == "", resultErr)
	return nil
}

func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask) error {
	taskBytes, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task for retry failed: taskID=%s, err=%w", task.TaskId, err)
	}
	return redisClient.LPush(ctx, retryQueueKey, taskBytes).Err()
}

func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()
	c.wg.Wait()
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("close consumer group failed: groupID=%s, err=%v", c.groupID, err)
	} else {
		logx.Info("consumer group closed: groupID=%s, topic=%s", c.groupID, c.topic)
	}
}
