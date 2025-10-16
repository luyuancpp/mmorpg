package kafka

import (
	"context"
	db_config "db/internal/config"
	"db/internal/logic/pkg/proto_sql"
	db_proto "db/proto/service/go/grpc/db"
	"errors"
	"fmt"
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

// --------------- 新增：补全缺失的常量和配置结构体 ---------------
// expandStatusExpireDuration 扩容状态过期时间（修复报错：之前未定义）
const expandStatusExpireDuration = 30 * time.Minute

// KafkaConsumerConfig Kafka消费者配置结构体（映射YAML配置）
type KafkaConsumerConfig struct {
	BootstrapServers string `yaml:"bootstrapServers"` // Kafka地址
	GroupID          string `yaml:"groupID"`          // 消费组ID
	Topic            string `yaml:"topic"`            // 消费主题
	PartitionCount   int32  `yaml:"partitionCount"`   // 分区数
	IsOfflineExpand  bool   `yaml:"isOfflineExpand"`  // 停服扩容开关：true=停服模式（不走锁和状态检测）
}

// --------------- 原有结构体保留，新增开关字段 ---------------
// KeyOrderedKafkaConsumer 支持动态锁的有序消费者
type KeyOrderedKafkaConsumer struct {
	consumer             sarama.ConsumerGroup
	redisClient          redis.Cmdable
	topic                string
	groupID              string // 消费组ID
	partitionCount       int32  // 分区数
	isOfflineExpand      bool   // 新增：停服扩容开关（修复场景切换需求）
	workers              map[int32]*worker
	wg                   *sync.WaitGroup
	ctx                  context.Context
	cancel               context.CancelFunc
	locker               *locker.RedisLocker
	retryQueueKey        string        // 重试队列Redis Key
	retryConsumeInterval time.Duration // 重试队列消费间隔（补全重试逻辑）
	retryMaxTimes        int           // 消息最大重试次数（补全重试逻辑）
}

// worker 单个分区的处理协程
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

// --------------- 初始化函数：注入配置开关 ---------------
// NewKeyOrderedKafkaConsumer 创建消费者实例（修复：接收配置结构体，注入开关）
func NewKeyOrderedKafkaConsumer(
	cfg db_config.Config, // 传入配置（含停服扩容开关）
	redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	// 1. 配置Sarama消费者
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Consumer.Return.Errors = true
	config.Consumer.Offsets.Initial = sarama.OffsetOldest // 从最老消息开始消费

	// 2. 创建消费组
	consumerGroup, err := sarama.NewConsumerGroup(
		[]string{cfg.ServerConfig.Kafka.Brokers},
		cfg.ServerConfig.Kafka.GroupID,
		config,
	)
	if err != nil {
		return nil, fmt.Errorf("create consumer group failed: groupID=%s, err=%w", cfg.ServerConfig.Kafka.GroupID, err)
	}

	// 3. 初始化上下文和WaitGroup
	ctx, cancel := context.WithCancel(context.Background())
	wg := &sync.WaitGroup{}

	// 4. 初始化重试相关配置（补全重试逻辑，修复StartRetryConsumer报错）
	retryQueueKey := fmt.Sprintf("kafka:retry:queue:%s", cfg.ServerConfig.Kafka.Topic)
	lockerIns := locker.NewRedisLocker(redisClient)

	// 5. 初始化worker池
	workers := make(map[int32]*worker)
	for i := int32(0); i < cfg.ServerConfig.Kafka.PartitionCnt; i++ {
		workers[i] = &worker{
			partition:     i,
			msgCh:         make(chan *sarama.ConsumerMessage, 1000), // 缓冲1000条消息，避免阻塞
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
		isOfflineExpand:      cfg.ServerConfig.Kafka.IsOfflineExpand, // 注入停服扩容开关
		workers:              workers,
		wg:                   wg,
		ctx:                  ctx,
		cancel:               cancel,
		locker:               lockerIns,
		retryQueueKey:        retryQueueKey,
		retryConsumeInterval: 1 * time.Second, // 重试队列1秒消费一次
		retryMaxTimes:        3,               // 消息最大重试3次
	}, nil
}

// --------------- 核心方法：启动消费者（修复worker启动传参） ---------------
// Start 启动消费者
func (c *KeyOrderedKafkaConsumer) Start() error {
	// 1. 启动所有worker：传递停服扩容开关（修复processDBTaskMessage传参）
	for _, w := range c.workers {
		c.wg.Add(1)
		go w.start(processDBTaskMessage, c.isOfflineExpand)
	}

	// 2. 启动重试队列消费（补全方法，修复undefined报错）
	c.StartRetryConsumer()

	// 3. 启动消费组监听：拉取Kafka消息分发到worker
	go func() {
		for {
			// 消费组持续拉取消息，异常时自动重试
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, &consumerGroupHandler{consumer: c}); err != nil {
				logx.Errorf("consumer group consume failed: groupID=%s, topic=%s, err=%v", c.groupID, c.topic, err)
				time.Sleep(1 * time.Second) // 异常后睡眠1秒再重试，避免高频报错
			}
			// 上下文取消（如调用Stop），则退出循环
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

// --------------- 补全：重试队列消费方法（修复undefined报错） ---------------
// StartRetryConsumer 启动重试队列消费协程
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
				c.consumeRetryQueue() // 定时消费重试队列
			}
		}
	}()
	logx.Infof("retry consumer started: topic=%s, interval=%v, maxRetryTimes=%d",
		c.topic, c.retryConsumeInterval, c.retryMaxTimes)
}

// consumeRetryQueue 消费重试队列消息
func (c *KeyOrderedKafkaConsumer) consumeRetryQueue() {
	// 从队尾弹出消息（FIFO，保证顺序）
	msgBytes, err := c.redisClient.RPop(c.ctx, c.retryQueueKey).Bytes()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return // 队列空，无需处理
		}
		logx.Errorf("pop retry queue failed: queueKey=%s, err=%v", c.retryQueueKey, err)
		return
	}

	// 解析重试消息
	var task db_proto.DBTask
	if err := proto.Unmarshal(msgBytes, &task); err != nil {
		logx.Errorf("unmarshal retry task failed: err=%v, taskBytes=%v", err, msgBytes)
		return
	}

	// 检查重试次数（需确保db_proto.DBTask已添加RetryCount字段）
	// 若未添加：需在proto文件中添加 "int32 retry_count = xx;" 并重新编译
	if task.RetryCount >= int32(c.retryMaxTimes) {
		deadQueueKey := fmt.Sprintf("kafka:dead:queue:%s", c.topic)
		_ = c.redisClient.LPush(c.ctx, deadQueueKey, msgBytes) // 存入死信队列，后续人工处理
		logx.Errorf("retry task exceed max times: taskID=%s, retryCount=%d, move to dead queue: %s",
			task.TaskId, task.RetryCount, deadQueueKey)
		return
	}

	// 重试次数+1，重新处理
	task.RetryCount++
	if err := processTaskWithoutLock(c.ctx, c.redisClient, &task); err != nil {
		logx.Errorf("retry process task failed: taskID=%s, retryCount=%d, err=%v", task.TaskId, task.RetryCount, err)
		// 处理失败，重新存入重试队列
		retryTaskBytes, _ := proto.Marshal(&task)
		_ = c.redisClient.LPush(c.ctx, c.retryQueueKey, retryTaskBytes)
		return
	}

	logx.Infof("retry process task success: taskID=%s, retryCount=%d", task.TaskId, task.RetryCount)
}

// --------------- 补全：worker启动方法（修复传参） ---------------
// start 启动worker协程，处理分区消息
func (w *worker) start(
	processFunc func(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage, isOfflineExpand bool) error,
	isOfflineExpand bool,
) {
	defer func() {
		close(w.msgCh)
		w.wg.Done()
		logx.Infof("worker stopped: partition=%d, topic=%s", w.partition, w.topic)
	}()

	// 捕获panic，避免worker异常退出（增强稳定性）
	defer func() {
		if r := recover(); r != nil {
			logx.Errorf("worker panic recovered: partition=%d, panic=%v, stack=%s",
				w.partition, r, string(debug.Stack()))
		}
	}()

	logx.Infof("worker started: partition=%d, topic=%s, isOfflineExpand=%v", w.partition, w.topic, isOfflineExpand)

	// 循环处理消息
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
			// 记录处理耗时，便于排查慢消息
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

// --------------- 消费组处理器：分发消息到worker ---------------
type consumerGroupHandler struct {
	consumer *KeyOrderedKafkaConsumer
}

func (h *consumerGroupHandler) Setup(sarama.ConsumerGroupSession) error   { return nil }
func (h *consumerGroupHandler) Cleanup(sarama.ConsumerGroupSession) error { return nil }

// ConsumeClaim 拉取分区消息，分发到对应worker的msgCh
func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	for msg := range claim.Messages() {
		partition := msg.Partition
		// 找到对应分区的worker
		worker, ok := h.consumer.workers[partition]
		if !ok {
			logx.Errorf("no worker found for partition: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			session.MarkMessage(msg, "") // 标记已消费，避免重复拉取
			continue
		}

		// 消息通道满时的保护：避免阻塞ConsumeClaim协程
		select {
		case worker.msgCh <- msg:
		case <-worker.ctx.Done():
			logx.Info("worker context canceled, stop sending msg: topic=%s, partition=%d",
				h.consumer.topic, partition)
			return nil
		case <-time.After(100 * time.Millisecond): // 100ms超时保护
			logx.Errorf("worker msg channel is full, retry later: topic=%s, partition=%d, offset=%d",
				h.consumer.topic, partition, msg.Offset)
			time.Sleep(50 * time.Millisecond)
			// 重试一次，仍失败则标记已消费（避免死循环）
			select {
			case worker.msgCh <- msg:
			default:
				logx.Errorf("worker msg channel still full, drop msg: topic=%s, partition=%d, offset=%d",
					h.consumer.topic, partition, msg.Offset)
				session.MarkMessage(msg, "")
			}
		}

		// 标记消息已消费（告知Kafka提交偏移量）
		session.MarkMessage(msg, "")
	}
	return nil
}

// --------------- 消息处理核心逻辑（修复开关判断） ---------------
// processDBTaskMessage 处理DB任务消息：根据开关判断是否走锁和状态检测
func processDBTaskMessage(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage, isOfflineExpand bool) error {
	// 1. 解析DB任务
	var task db_proto.DBTask
	if err := proto.Unmarshal(msg.Value, &task); err != nil {
		return fmt.Errorf("unmarshal task failed: offset=%d, err=%w", msg.Offset, err)
	}
	key := strconv.FormatUint(task.Key, 10)
	logx.Debugf("received db task: taskID=%s, key=%s, partition=%d, isOfflineExpand=%v",
		task.TaskId, key, msg.Partition, isOfflineExpand)

	// 2. 停服扩容模式：直接跳过锁和状态检测，快速处理消息
	if isOfflineExpand {
		logx.Debugf("offline expand mode: skip lock/status check, process task: taskID=%s", task.TaskId)
		return processTaskWithoutLock(ctx, worker.redisClient, &task)
	}

	// 3. 动态扩容模式：检测扩容状态，必要时加锁
	expandStatus, err := GetExpandStatus(ctx, worker.redisClient, worker.topic)
	if err != nil {
		logx.Errorf("get expand status failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	// 4. 检查扩容状态是否过期（避免长期处于扩容中状态）
	currentTime := time.Now().UnixMilli()
	if expandStatus.Status == ExpandStatusExpanding &&
		(expandStatus.UpdateTime == 0 || currentTime-expandStatus.UpdateTime > expandStatusExpireDuration.Milliseconds()) {
		logx.Errorf("expand status expired, switch to normal: topic=%s, key=%s, lastUpdate=%d",
			worker.topic, key, expandStatus.UpdateTime)
		expandStatus.Status = ExpandStatusNormal
		// 自动更新Redis中的过期状态，避免其他实例继续加锁
		_ = SetExpandStatus(ctx, worker.redisClient, worker.topic, ExpandStatusNormal, expandStatus.PartitionCount)
	}

	// 5. 扩容中：加锁处理，保证消息有序
	if expandStatus.Status == ExpandStatusExpanding {
		logx.Debugf("expanding mode: try lock for task: taskID=%s, key=%s", task.TaskId, key)
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	// 6. 常态：直接处理消息
	return processTaskWithoutLock(ctx, worker.redisClient, &task)
}

// --------------- 辅助方法：加锁处理消息 ---------------
func tryLockAndProcess(ctx context.Context, worker *worker, key string, task *db_proto.DBTask, offset int64) error {
	lockKey := fmt.Sprintf("kafka:consumer:lock:%s", key)
	lockTTL := 5 * time.Second // 锁过期时间：大于单次任务处理耗时

	// 尝试获取分布式锁
	tryLock, err := worker.locker.TryLock(ctx, lockKey, lockTTL)
	if err != nil {
		return fmt.Errorf("try lock failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
	}
	// 锁被占用：存入重试队列，后续再试
	if !tryLock.IsLocked() {
		if err := saveToRetryQueue(ctx, worker.redisClient, worker.retryQueueKey, task); err != nil {
			return fmt.Errorf("save to retry queue failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
		}
		logx.Debugf("lock occupied: task saved to retry queue: taskID=%s, key=%s", task.TaskId, key)
		return nil
	}

	// 延迟释放锁：确保任务处理完后释放
	defer func() {
		if _, err := tryLock.Release(ctx); err != nil {
			logx.Errorf("release lock failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		}
	}()

	// 加锁成功，处理任务
	return processTaskWithoutLock(ctx, worker.redisClient, task)
}

// --------------- 辅助方法：无锁处理消息 ---------------
func processTaskWithoutLock(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask) error {
	// 1. 查找消息对应的Proto类型
	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		return fmt.Errorf("find message type failed: type=%s, taskID=%s, err=%w", task.MsgType, task.TaskId, err)
	}

	// 2. 解析任务体
	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		return fmt.Errorf("unmarshal task body failed: taskID=%s, err=%w", task.TaskId, err)
	}

	// 3. 执行DB操作（读/写）
	var resultErr string
	var resultData []byte
	switch task.Op {
	case "read":
		if err := proto_sql.DB.SqlGenerator.LoadOneByWhereCase(msg, task.WhereCase); err != nil {
			resultErr = fmt.Sprintf("db read failed: %v", err)
		} else {
			// 读取成功，序列化结果
			if resultData, err = proto.Marshal(msg); err != nil {
				resultErr = fmt.Sprintf("marshal read result failed: %v", err)
			}
		}
	case "write":
		if err := proto_sql.DB.SqlGenerator.Save(msg); err != nil {
			resultErr = fmt.Sprintf("db write failed: %v", err)
		}
	default:
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	}

	// 4. 保存任务结果（若有TaskId）
	// 4. 仅read操作存Redis结果（write操作无需存）
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
		// 存入Redis供生产者查询
		if err := redisClient.LPush(ctx, task.TaskId, resBytes).Err(); err != nil {
			return fmt.Errorf("save read result to redis failed: taskID=%s, err=%w", task.TaskId, err)
		}
	}

	logx.Infof("task processed: taskID=%s, op=%s, success=%v, err=%s",
		task.TaskId, task.Op, resultErr == "", resultErr)
	return nil
}

// --------------- 辅助方法：存入重试队列 ---------------
func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask) error {
	taskBytes, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task for retry failed: taskID=%s, err=%w", task.TaskId, err)
	}
	// 从队列头部插入，保证FIFO顺序
	return redisClient.LPush(ctx, retryQueueKey, taskBytes).Err()
}

// --------------- 停止消费者：释放资源 ---------------
// Stop 优雅停止消费者
func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()  // 取消上下文，通知所有协程退出
	c.wg.Wait() // 等待所有worker协程完成
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("close consumer group failed: groupID=%s, err=%v", c.groupID, err)
	} else {
		logx.Info("consumer group closed: groupID=%s, topic=%s", c.groupID, c.topic)
	}
}
