package kafka

import (
	"context"
	db_config "db/internal/config"
	"db/internal/logic/pkg/proto_sql"
	db_proto "db/proto/service/go/grpc/db"
	"errors"
	"fmt"
	"github.com/luyuancpp/proto2mysql"
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

// 核心改造1：定义操作处理函数签名
type dbOpHandler func(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string

// 核心改造2：初始化操作-函数映射表
var dbOpHandlers = map[string]dbOpHandler{
	"read":  handleDBReadOp,
	"write": handleDBWriteOp,
}

// 核心改造3：read操作处理函数
func handleDBReadOp(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string {
	// 执行数据库查询
	if err := proto_sql.DB.SqlModel.FindOneByWhereClause(msg, task.WhereCase); err != nil && !errors.Is(err, proto2mysql.ErrNoRowsFound) {
		return fmt.Sprintf("db read failed: %v", err)
	}

	// 序列化查询结果
	resultData, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Sprintf("marshal read result failed: %v", err)
	}

	// 写入Redis结果（仅read操作需要）
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
		if err := redisClient.Expire(ctx, resultKey, 5*time.Minute).Err(); err != nil {
			return fmt.Sprintf("set expire for result key failed: %v", err)
		}
	}

	return ""
}

// 核心改造4：write操作处理函数
func handleDBWriteOp(
	ctx context.Context,
	redisClient redis.Cmdable,
	task *db_proto.DBTask,
	msg proto.Message,
) string {
	if err := proto_sql.DB.SqlModel.Save(msg); err != nil {
		return fmt.Sprintf("db write failed: %v", err)
	}
	return ""
}

// NewKeyOrderedKafkaConsumer 初始化消费者（原逻辑不变）
func NewKeyOrderedKafkaConsumer(
	cfg db_config.Config,
	redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
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

// Start 启动消费者（原逻辑不变）
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

// StartRetryConsumer 启动重试消费者（原逻辑不变）
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

// consumeRetryQueue 消费重试队列（原逻辑不变）
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

// worker.start 启动工作协程（原逻辑不变）
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

// consumerGroupHandler 实现消费者组接口（原逻辑不变）
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
		worker, ok := h.consumer.workers[partition]
		if !ok {
			logx.Errorf("no worker found for partition: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			session.MarkMessage(msg, "")
			continue
		}

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

// processDBTaskMessage 处理DB任务消息（原逻辑不变）
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

// tryLockAndProcess 尝试加锁并处理任务（原逻辑不变）
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

// 核心改造5：重构processTaskWithoutLock，使用函数映射替代switch
func processTaskWithoutLock(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask) error {
	// 1. 解析消息类型
	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		return fmt.Errorf("find message type failed: type=%s, taskID=%s, err=%w", task.MsgType, task.TaskId, err)
	}

	// 2. 反序列化任务体
	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		return fmt.Errorf("unmarshal task body failed: taskID=%s, err=%w", task.TaskId, err)
	}

	// 3. 通过映射表获取处理函数并执行
	handler, ok := dbOpHandlers[task.Op]
	var resultErr string
	if !ok {
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	} else {
		resultErr = handler(ctx, redisClient, task, msg)
	}

	// 4. 输出处理日志
	logx.Infof("task processed: taskID=%s, op=%s, success=%v, err=%s",
		task.TaskId, task.Op, resultErr == "", resultErr)

	// 5. 若有错误，返回错误信息（供重试逻辑使用）
	if resultErr != "" {
		return fmt.Errorf(resultErr)
	}
	return nil
}

// saveToRetryQueue 保存任务到重试队列（原逻辑不变）
func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask) error {
	taskBytes, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task for retry failed: taskID=%s, err=%w", task.TaskId, err)
	}
	return redisClient.LPush(ctx, retryQueueKey, taskBytes).Err()
}

// Stop 停止消费者（原逻辑不变）
func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()
	c.wg.Wait()
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("close consumer group failed: groupID=%s, err=%v", c.groupID, err)
	} else {
		logx.Info("consumer group closed: groupID=%s, topic=%s", c.groupID, c.topic)
	}
}
