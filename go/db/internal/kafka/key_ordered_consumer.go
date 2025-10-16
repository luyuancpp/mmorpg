package kafka

import (
	"context"
	"db/internal/logic/pkg/proto_sql"
	db_proto "db/proto/service/go/grpc/db"
	"fmt"
	"strconv"
	"sync"
	"time"

	"db/internal/locker" // 复用你之前的Redis锁工具
	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
)

// KeyOrderedKafkaConsumer 支持动态锁的有序消费者
type KeyOrderedKafkaConsumer struct {
	consumer       sarama.ConsumerGroup
	redisClient    redis.Cmdable
	topic          string
	partitionCount int32
	workers        map[int32]*worker
	wg             *sync.WaitGroup
	ctx            context.Context
	cancel         context.CancelFunc
	locker         *locker.RedisLocker // 分布式锁工具
	retryQueueKey  string              // 重试队列Redis Key
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

// NewKeyOrderedKafkaConsumer 创建消费者实例
func NewKeyOrderedKafkaConsumer(
	bootstrapServers, groupID, topic string,
	partitionCount int32, redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	// 1. 配置Sarama消费者
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Consumer.Return.Errors = true
	config.Consumer.Offsets.Initial = sarama.OffsetOldest

	// 2. 创建消费组
	consumerGroup, err := sarama.NewConsumerGroup([]string{bootstrapServers}, groupID, config)
	if err != nil {
		return nil, fmt.Errorf("create consumer group failed: %w", err)
	}

	// 3. 初始化上下文和WaitGroup
	ctx, cancel := context.WithCancel(context.Background())
	wg := &sync.WaitGroup{}

	// 4. 初始化worker池
	workers := make(map[int32]*worker)
	locker := locker.NewRedisLocker(redisClient)                // 初始化分布式锁
	retryQueueKey := fmt.Sprintf("kafka:retry:queue:%s", topic) // 重试队列Key

	for i := int32(0); i < partitionCount; i++ {
		workers[i] = &worker{
			partition:     i,
			msgCh:         make(chan *sarama.ConsumerMessage, 1000),
			ctx:           ctx,
			redisClient:   redisClient,
			locker:        locker,
			topic:         topic,
			retryQueueKey: retryQueueKey,
			wg:            wg,
		}
	}

	return &KeyOrderedKafkaConsumer{
		consumer:       consumerGroup,
		redisClient:    redisClient,
		topic:          topic,
		partitionCount: partitionCount,
		workers:        workers,
		wg:             wg,
		ctx:            ctx,
		cancel:         cancel,
		locker:         locker,
		retryQueueKey:  retryQueueKey,
	}, nil
}

// Start 启动消费者
func (c *KeyOrderedKafkaConsumer) Start() error {
	// 1. 启动所有worker
	for _, w := range c.workers {
		c.wg.Add(1)
		go w.start(processDBTaskMessage)
	}

	// 2. 创建消费组处理器
	handler := &consumerGroupHandler{
		consumer: c,
	}

	// 3. 启动消费组监听
	go func() {
		for {
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, handler); err != nil {
				logx.Errorf("consumer group consume failed: groupID=%s, err=%v", c.consumer.GroupID(), err)
			}
			if c.ctx.Err() != nil {
				logx.Info("consumer group stopped: groupID=%s", c.consumer.GroupID())
				return
			}
		}
	}()

	logx.Infof("consumer started: topic=%s, groupID=%s, partitionCount=%d",
		c.topic, c.consumer.GroupID(), c.partitionCount)
	return nil
}

// Stop 停止消费者
func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()
	c.wg.Wait()
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("close consumer group failed: groupID=%s, err=%v", c.consumer.GroupID(), err)
	} else {
		logx.Info("consumer group closed success: groupID=%s", c.consumer.GroupID())
	}
}

// consumerGroupHandler 实现Sarama消费组接口
type consumerGroupHandler struct {
	consumer *KeyOrderedKafkaConsumer
}

func (h *consumerGroupHandler) Setup(sarama.ConsumerGroupSession) error   { return nil }
func (h *consumerGroupHandler) Cleanup(sarama.ConsumerGroupSession) error { return nil }
func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	for msg := range claim.Messages() {
		partition := msg.Partition
		if worker, ok := h.consumer.workers[partition]; ok {
			select {
			case worker.msgCh <- msg:
			case <-worker.ctx.Done():
			}
		}
		session.MarkMessage(msg, "") // 标记消息已消费
	}
	return nil
}

// worker的start方法：处理消息
func (w *worker) start(processFunc func(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage) error) {
	defer func() {
		close(w.msgCh)
		w.wg.Done()
		logx.Infof("worker stopped: partition=%d", w.partition)
	}()

	logx.Infof("worker started: partition=%d", w.partition)

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
			// 处理消息（异常仅打印日志，不退出worker）
			if err := processFunc(w.ctx, w, msg); err != nil {
				logx.Errorf("worker process msg failed: partition=%d, offset=%d, err=%v",
					w.partition, msg.Offset, err)
			}
		}
	}
}

// processDBTaskMessage 处理DB任务消息（含动态锁逻辑）
func processDBTaskMessage(ctx context.Context, worker *worker, msg *sarama.ConsumerMessage) error {
	// 1. 反序列化DB任务
	var task db_proto.DBTask
	if err := proto.Unmarshal(msg.Value, &task); err != nil {
		return fmt.Errorf("unmarshal task failed: offset=%d, err=%w", msg.Offset, err)
	}
	key := strconv.FormatUint(task.Key, 10) // 业务Key（如playerId）
	logx.Debugf("received db task: taskID=%s, key=%s, partition=%d", task.TaskId, key, msg.Partition)

	// 2. 获取当前扩容状态，判断是否需要加锁
	expandStatus, err := GetExpandStatus(ctx, worker.redisClient, worker.topic)
	if err != nil {
		logx.Errorf("get expand status failed: key=%s, err=%v", key, err)
		// 降级：加锁避免有序性问题
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	// 3. 扩容中 → 加锁处理；常态/完成 → 直接处理
	if expandStatus.Status == ExpandStatusExpanding {
		logx.Debugf("expanding, try lock: key=%s, taskID=%s", key, task.TaskId)
		return tryLockAndProcess(ctx, worker, key, &task, msg.Offset)
	}

	// 4. 非扩容状态，直接处理任务
	return processTaskWithoutLock(ctx, worker.redisClient, &task)
}

// tryLockAndProcess 尝试加锁并处理任务
func tryLockAndProcess(ctx context.Context, worker *worker, key string, task *db_proto.DBTask, offset int64) error {
	lockKey := fmt.Sprintf("kafka:consumer:lock:%s", key)
	lockTTL := 5 * time.Second // 锁超时时间（根据任务处理耗时调整）

	// 1. 尝试获取锁
	tryLock, err := worker.locker.TryLock(ctx, lockKey, lockTTL)
	if err != nil {
		return fmt.Errorf("try lock failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
	}
	if !tryLock.IsLocked() {
		// 2. 锁获取失败，存入重试队列
		if err := saveToRetryQueue(ctx, worker.redisClient, worker.retryQueueKey, task); err != nil {
			return fmt.Errorf("save to retry queue failed: key=%s, taskID=%s, err=%w", key, task.TaskId, err)
		}
		logx.Debugf("lock occupied, saved to retry queue: key=%s, taskID=%s", key, task.TaskId)
		return nil
	}

	// 3. 延迟释放锁
	defer func() {
		if _, err := tryLock.Release(ctx); err != nil {
			logx.Errorf("release lock failed: key=%s, taskID=%s, err=%v", key, task.TaskId, err)
		}
	}()

	// 4. 加锁成功，处理任务
	return processTaskWithoutLock(ctx, worker.redisClient, task)
}

// processTaskWithoutLock 无锁处理任务（核心业务逻辑）
func processTaskWithoutLock(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask) error {
	// 1. 根据消息类型获取Protobuf描述符
	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		return fmt.Errorf("find message type failed: type=%s, taskID=%s, err=%w", task.MsgType, task.TaskId, err)
	}

	// 2. 反序列化任务体
	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		return fmt.Errorf("unmarshal task body failed: taskID=%s, err=%w", task.TaskId, err)
	}

	// 3. 执行DB操作
	var resultData []byte
	var resultErr string
	switch task.Op {
	case "read":
		if err := proto_sql.DB.SqlGenerator.LoadOneByWhereCase(msg, task.WhereCase); err != nil {
			resultErr = fmt.Sprintf("db read failed: %v", err)
		} else {
			if resultData, err = proto.Marshal(msg); err != nil {
				resultErr = fmt.Sprintf("marshal result failed: %v", err)
			}
		}
	case "write":
		if err := proto_sql.DB.SqlGenerator.Save(msg); err != nil {
			resultErr = fmt.Sprintf("db write failed: %v", err)
		}
	default:
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	}

	// 4. 存储任务结果到Redis
	if task.TaskId != "" {
		result := &db_proto.TaskResult{
			Success: resultErr == "",
			Data:    resultData,
			Error:   resultErr,
		}
		resBytes, err := proto.Marshal(result)
		if err != nil {
			return fmt.Errorf("marshal task result failed: taskID=%s, err=%w", task.TaskId, err)
		}
		if err := redisClient.LPush(ctx, task.TaskId, resBytes).Err(); err != nil {
			return fmt.Errorf("save task result to redis failed: taskID=%s, err=%w", task.TaskId, err)
		}
	}

	logx.Infof("task processed success: taskID=%s, op=%s, success=%v", task.TaskId, task.Op, resultErr == "")
	return nil
}

// saveToRetryQueue 将任务存入重试队列
func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask) error {
	taskBytes, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task for retry failed: taskID=%s, err=%w", task.TaskId, err)
	}
	// LPush：从队列头部插入，后续重试时从尾部弹出（FIFO）
	return redisClient.LPush(ctx, retryQueueKey, taskBytes).Err()
}
