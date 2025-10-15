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
	wg           sync.WaitGroup
	ctx          context.Context
	cancel       context.CancelFunc
}

// worker 单个分区的处理goroutine
type worker struct {
	partition   int32
	msgCh       chan *sarama.ConsumerMessage
	ctx         context.Context
	redisClient redis.Cmdable
	wg          *sync.WaitGroup // 新增wg指针，指向消费者的WaitGroup
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

	// 创建 ConsumerGroup
	consumerGroup, err := sarama.NewConsumerGroup([]string{bootstrapServers}, groupID, config)
	if err != nil {
		return nil, fmt.Errorf("创建消费者失败: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())

	// 初始化worker
	workers := make(map[int32]*worker)
	for i := int32(0); i < int32(partitionCnt); i++ {
		workers[i] = &worker{
			partition:   i,
			msgCh:       make(chan *sarama.ConsumerMessage, 1000),
			ctx:         ctx,
			redisClient: redisClient,
		}
	}

	return &KeyOrderedKafkaConsumer{
		consumer:     consumerGroup,
		redisClient:  redisClient,
		topic:        topic,
		partitionCnt: partitionCnt,
		workers:      workers,
		ctx:          ctx,
		cancel:       cancel,
	}, nil
}

// Start 启动消费者
func (c *KeyOrderedKafkaConsumer) Start() error {
	// 启动所有worker，并传递WaitGroup指针
	for _, w := range c.workers {
		c.wg.Add(1)
		w.wg = &c.wg // 将消费者的wg指针赋值给worker
		go w.start(processDBTaskMessage)
	}

	handler := &consumerGroupHandler{
		c: c,
	}

	go func() {
		for {
			if err := c.consumer.Consume(c.ctx, []string{c.topic}, handler); err != nil {
				logx.Errorf("ConsumerGroup consume error: %v", err)
			}
			if c.ctx.Err() != nil {
				return
			}
		}
	}()

	logx.Infof("Kafka消费者启动成功，主题: %s，分区数: %d", c.topic, c.partitionCnt)
	return nil
}

// consumerGroupHandler 实现 sarama.ConsumerGroupHandler
type consumerGroupHandler struct {
	c *KeyOrderedKafkaConsumer
}

func (h *consumerGroupHandler) Setup(sarama.ConsumerGroupSession) error   { return nil }
func (h *consumerGroupHandler) Cleanup(sarama.ConsumerGroupSession) error { return nil }
func (h *consumerGroupHandler) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	for msg := range claim.Messages() {
		partition := msg.Partition
		if worker, ok := h.c.workers[partition]; ok {
			select {
			case worker.msgCh <- msg:
			case <-worker.ctx.Done():
			}
		}
		session.MarkMessage(msg, "")
	}
	return nil
}

// processDBTaskMessage 处理DB任务消息
func processDBTaskMessage(redisClient redis.Cmdable, msgBytes []byte) error {
	var task db_proto.DBTask
	if err := proto.Unmarshal(msgBytes, &task); err != nil {
		logx.Errorf("Failed to unmarshal DBTask payload: %v", err)
		return fmt.Errorf("unmarshal DBTask failed: %v", err)
	}
	logx.Infof("Received DB task: ID=%s, Op=%s, MsgType=%s", task.TaskId, task.Op, task.MsgType)

	mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
	if err != nil {
		logx.Errorf("Unknown proto message type: %s", task.MsgType)
		return fmt.Errorf("unknown proto message type: %s", task.MsgType)
	}

	msg := dynamicpb.NewMessage(mt.Descriptor())
	if err := proto.Unmarshal(task.Body, msg); err != nil {
		logx.Errorf("Failed to unmarshal proto message body: %v", err)
		return fmt.Errorf("unmarshal proto message failed: %v", err)
	}

	var resultData []byte
	var resultErr string

	switch task.Op {
	case "read":
		if err := proto_sql.DB.SqlGenerator.LoadOneByWhereCase(msg, task.WhereCase); err != nil {
			resultErr = fmt.Sprintf("DB read failed: %v", err)
		} else {
			resultData, err = proto.Marshal(msg)
			if err != nil {
				resultErr = fmt.Sprintf("marshal result failed: %v", err)
			}
		}
	case "write":
		if err := proto_sql.DB.SqlGenerator.Save(msg); err != nil {
			resultErr = fmt.Sprintf("DB write failed: %v", err)
		}
	default:
		resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
	}

	if task.TaskId != "" {
		result := &db_proto.TaskResult{
			Success: resultErr == "",
			Data:    resultData,
			Error:   resultErr,
		}
		resBytes, err := proto.Marshal(result)
		if err != nil {
			logx.Errorf("Marshal TaskResult failed for TaskID=%s: %v", task.TaskId, err)
			return fmt.Errorf("marshal TaskResult failed: %v", err)
		}
		if err := redisClient.LPush(context.Background(), task.TaskId, resBytes).Err(); err != nil {
			logx.Errorf("Failed to write TaskResult to Redis for TaskID=%s: %v", task.TaskId, err)
			return fmt.Errorf("Redis set TaskResult failed: %v", err)
		}
	}

	logx.Infof("DB Task %s completed. Success: %v", task.TaskId, resultErr == "")
	return nil
}

// worker 启动函数
func (w *worker) start(processFunc func(redisClient redis.Cmdable, msgBytes []byte) error) {
	defer func() {
		close(w.msgCh)
		logx.Infof("Worker停止，分区: %d", w.partition)
		w.wg.Done() // 使用worker的wg指针
	}()

	logx.Infof("Worker启动，分区: %d", w.partition)

	for {
		select {
		case <-w.ctx.Done():
			logx.Infof("Worker收到关闭信号，分区: %d", w.partition)
			return
		case msg, ok := <-w.msgCh:
			if !ok {
				return
			}

			if err := processFunc(w.redisClient, msg.Value); err != nil {
				logx.Errorf("Task processing failed: %v", err)
			}
		}
	}
}

// Stop 停止消费者
func (c *KeyOrderedKafkaConsumer) Stop() {
	c.cancel()
	c.wg.Wait()
	if err := c.consumer.Close(); err != nil {
		logx.Errorf("关闭ConsumerGroup失败: %v", err)
	}
	logx.Infof("Kafka消费者已完全停止")
}
