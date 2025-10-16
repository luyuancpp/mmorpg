package kafka

import (
	"context"
	"db/internal/logic/pkg/proto_sql"
	db_proto "db/proto/service/go/grpc/db"
	"fmt"
	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
	"sync"
)

// KeyOrderedKafkaConsumer 保证相同key的任务由固定goroutine按顺序处理
type KeyOrderedKafkaConsumer struct {
	consumer     sarama.ConsumerGroup
	redisClient  redis.Cmdable
	topic        string
	partitionCnt int
	workers      map[int32]*worker
	wg           *sync.WaitGroup // 优化：改为指针类型，避免值类型指针共享风险
	ctx          context.Context
	cancel       context.CancelFunc
}

// worker 单个分区的处理goroutine
type worker struct {
	partition   int32
	msgCh       chan *sarama.ConsumerMessage
	ctx         context.Context
	redisClient redis.Cmdable
	wg          *sync.WaitGroup // 保留wg指针，接收消费者的WaitGroup实例
}

// NewKeyOrderedKafkaConsumer 创建有序消费者实例
func NewKeyOrderedKafkaConsumer(
	bootstrapServers, groupID, topic string,
	partitionCnt int, redisClient redis.Cmdable,
) (*KeyOrderedKafkaConsumer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Consumer.Return.Errors = true
	config.Consumer.Offsets.Initial = sarama.OffsetOldest

	// 创建 GroupID 对应的消费者组
	consumerGroup, err := sarama.NewConsumerGroup([]string{bootstrapServers}, groupID, config)
	if err != nil {
		return nil, fmt.Errorf("创建消费者失败: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())

	// 初始化worker：按分区数量创建，每个分区绑定专属worker
	workers := make(map[int32]*worker)
	for i := int32(0); i < int32(partitionCnt); i++ {
		workers[i] = &worker{
			partition:   i,
			msgCh:       make(chan *sarama.ConsumerMessage, 1000), // 带缓冲，避免消息阻塞
			ctx:         ctx,
			redisClient: redisClient,
		}
	}

	// 优化：初始化WaitGroup指针，后续所有worker共享同一个实例（安全）
	return &KeyOrderedKafkaConsumer{
		consumer:     consumerGroup,
		redisClient:  redisClient,
		topic:        topic,
		partitionCnt: partitionCnt,
		workers:      workers,
		wg:           &sync.WaitGroup{}, // 关键：创建WaitGroup实例，赋值给指针
		ctx:          ctx,
		cancel:       cancel,
	}, nil
}

// Start 启动消费者：启动所有worker+消费者组监听
func (c *KeyOrderedKafkaConsumer) Start() error {
	// 启动所有worker：每个worker绑定独立的WaitGroup计数
	for _, w := range c.workers {
		c.wg.Add(1)                      // 每个worker对应1次Add，计数与worker数量一致
		w.wg = c.wg                      // 直接赋值WaitGroup指针（c.wg已是指针，无需&）
		go w.start(processDBTaskMessage) // 启动worker协程，处理消息
	}

	// 创建消费者组处理器，负责消息分发
	handler := &consumerGroupHandler{
		c: c,
	}

	// 启动协程监听Kafka消息（循环重启机制：消费失败后自动重试）
	go func() {
		for {
			// 监听指定主题的消息，异常时打印日志并重试
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, handler); err != nil {
				logx.Errorf("GroupID=%s 消费消息失败: %v", c.consumer.GroupID(), err)
			}
			// 若上下文已取消（如调用Stop），则退出循环
			if c.ctx.Err() != nil {
				logx.Infof("消费者组=%s 收到关闭信号，停止监听", c.consumer.GroupID())
				return
			}
		}
	}()

	logx.Infof("Kafka消费者启动成功 | 主题: %s | 分区数: %d | 消费组: %s",
		c.topic, c.partitionCnt, c.consumer.GroupID())
	return nil
}

// consumerGroupHandler 实现 sarama.ConsumerGroupHandler 接口：负责消息分发
type consumerGroupHandler struct {
	c *KeyOrderedKafkaConsumer
}

// Setup 消费者组准备就绪时调用（此处无特殊逻辑）
func (h *consumerGroupHandler) Setup(sarama.ConsumerGroupSession) error { return nil }

// Cleanup 消费者组关闭时调用（此处无特殊逻辑）
func (h *consumerGroupHandler) Cleanup(sarama.ConsumerGroupSession) error { return nil }

// ConsumeClaim 处理单个分区的消息：将消息分发到对应worker的通道
func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	// 循环读取分区的消息
	for msg := range claim.Messages() {
		partition := msg.Partition
		// 根据消息分区找到对应的worker，将消息投递到worker的通道
		if worker, ok := h.c.workers[partition]; ok {
			select {
			case worker.msgCh <- msg: // 消息投递到worker通道
			case <-worker.ctx.Done(): // 若worker已关闭，退出投递
			}
		}
		// 标记消息已消费（更新Kafka消费偏移量）
		session.MarkMessage(msg, "")
	}
	return nil
}

// processDBTaskMessage 处理DB任务消息：反序列化+执行DB操作+结果存Redis
func processDBTaskMessage(redisClient redis.Cmdable, msgBytes []byte) error {
	// 1. 反序列化DB任务
	var task db_proto.DBTask
	if err := proto.Unmarshal(msgBytes, &task); err != nil {
		logx.Errorf("反序列化DBTask失败: %v | 消息字节数: %d", err, len(msgBytes))
		return fmt.Errorf("unmarshal DBTask failed: %v", err)
	}
	logx.Debugf("收到DB任务 | TaskID: %s | 操作类型: %s | 消息类型: %s",
		task.TaskId, task.Op, task.MsgType)

	// 2. 根据消息类型找到对应的Protobuf描述符
	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		logx.Errorf("未知Protobuf消息类型: %s | TaskID: %s", task.MsgType, task.TaskId)
		return fmt.Errorf("unknown proto message type: %s", task.MsgType)
	}

	// 3. 反序列化任务体（动态创建Protobuf实例）
	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		logx.Errorf("反序列化任务体失败 | TaskID: %s | 原因: %v", task.TaskId, err)
		return fmt.Errorf("unmarshal proto message failed: %v", err)
	}

	// 4. 执行DB操作（读/写）
	var resultData []byte
	var resultErr string
	switch task.Op {
	case "read":
		// 执行DB读取操作
		if err := proto_sql.DB.SqlGenerator.LoadOneByWhereCase(msg, task.WhereCase); err != nil {
			resultErr = fmt.Sprintf("DB read failed: %v", err)
			logx.Errorf("DB读取失败 | TaskID: %s | 条件: %s | 原因: %v", task.TaskId, task.WhereCase, err)
		} else {
			// 序列化读取结果
			if resultData, err = proto.Marshal(msg); err != nil {
				resultErr = fmt.Sprintf("marshal result failed: %v", err)
				logx.Errorf("序列化DB结果失败 | TaskID: %s | 原因: %v", task.TaskId, err)
			}
		}
	case "write":
		// 执行DB写入操作
		if err := proto_sql.DB.SqlGenerator.Save(msg); err != nil {
			resultErr = fmt.Sprintf("DB write failed: %v", err)
			logx.Errorf("DB写入失败 | TaskID: %s | 原因: %v", task.TaskId, err)
		}
	default:
		resultErr = fmt.Sprintf("不支持的操作类型: %s", task.Op)
		logx.Errorf("未知操作类型 | TaskID: %s | Op: %s", task.TaskId, task.Op)
	}

	// 5. 将任务结果存入Redis（供上游模块查询）
	if task.TaskId != "" {
		result := &db_proto.TaskResult{
			Success: resultErr == "", // 无错误则标记为成功
			Data:    resultData,
			Error:   resultErr,
		}
		// 序列化结果
		resBytes, err := proto.Marshal(result)
		if err != nil {
			logx.Errorf("序列化TaskResult失败 | TaskID: %s | 原因: %v", task.TaskId, err)
			return fmt.Errorf("marshal TaskResult failed: %v", err)
		}
		// 存入Redis（LPush：上游用BLPop读取）
		if err := redisClient.LPush(context.Background(), task.TaskId, resBytes).Err(); err != nil {
			logx.Errorf("存储任务结果到Redis失败 | TaskID: %s | 原因: %v", task.TaskId, err)
			return fmt.Errorf("Redis set TaskResult failed: %v", err)
		}
		logx.Debugf("任务处理完成 | TaskID: %s | 成功: %v", task.TaskId, result.Success)
	}

	return nil
}

// start worker启动方法：循环读取消息并处理
func (w *worker) start(processFunc func(redisClient redis.Cmdable, msgBytes []byte) error) {
	// 延迟执行：关闭通道+标记WaitGroup完成
	defer func() {
		close(w.msgCh) // 关闭消息通道，避免内存泄漏
		w.wg.Done()    // 标记当前worker完成，对应Start中的Add(1)
		logx.Infof("Worker停止 | 分区: %d", w.partition)
	}()

	logx.Infof("Worker启动 | 分区: %d", w.partition)

	// 循环读取消息通道：处理消息或响应关闭信号
	for {
		select {
		case <-w.ctx.Done(): // 收到关闭信号（如调用Stop）
			logx.Infof("Worker收到关闭信号 | 分区: %d", w.partition)
			return
		case msg, ok := <-w.msgCh: // 读取待处理的消息
			if !ok { // 消息通道已关闭（如defer中close）
				logx.Infof("Worker消息通道已关闭 | 分区: %d", w.partition)
				return
			}
			// 调用处理函数，处理消息体（异常时仅打印日志，不退出worker）
			if err := processFunc(w.redisClient, msg.Value); err != nil {
				logx.Errorf("Worker处理消息失败 | 分区: %d | 偏移量: %d | 原因: %v",
					w.partition, msg.Offset, err)
			}
		}
	}
}

// Stop 停止消费者：优雅关闭所有资源
func (c *KeyOrderedKafkaConsumer) Stop() {
	// 1. 取消上下文，通知所有worker和消费组停止
	c.cancel()
	// 2. 等待所有worker处理完当前消息（避免任务中断）
	c.wg.Wait()
	// 3. 关闭消费者组，释放Kafka连接
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("关闭Kafka消费者组失败 | 消费组: %s | 原因: %v", c.consumer.GroupID(), err)
	} else {
		logx.Infof("Kafka消费者组关闭成功 | 消费组: %s", c.consumer.GroupID())
	}
	logx.Infof("Kafka消费者已完全停止 | 主题: %s", c.topic)
}
