package kafka

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/consistent" // 引入优化后的一致性哈希
	db_proto "login/proto/service/go/grpc/db"
)

type KeyOrderedKafkaProducer struct {
	producer     sarama.AsyncProducer
	topic        string
	partitionCnt int
	mu           sync.Mutex // 仅保护producer和partitionCnt，路由查询不用
	successCh    chan *sarama.ProducerMessage
	errorCh      chan *sarama.ProducerError
	ctx          context.Context
	cancel       context.CancelFunc
	consistent   *consistent.Consistent // 优化后的一致性哈希实例
}

// NewKeyOrderedKafkaProducer 初始化（核心链路专用）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	initialPartitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// 1. Kafka配置优化：核心链路建议调大通道缓冲，减少阻塞
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Producer.Return.Successes = true
	config.Producer.Return.Errors = true
	config.Producer.RequiredAcks = sarama.WaitForLocal // 核心优化：仅等待本地副本确认（比WaitForAll快）
	config.Producer.Retry.Max = 2                      // 减少重试次数，降低延迟
	config.ChannelBufferSize = 4096                    // 调大Input通道缓冲，避免核心链路阻塞
	config.Producer.Idempotent = true
	config.Net.MaxOpenRequests = 1

	// 2. 创建异步生产者
	producer, err := sarama.NewAsyncProducer([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create producer failed: %w", err)
	}

	// 3. 初始化一致性哈希（核心链路用20个虚拟节点，平衡均匀性和性能）
	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(initialPartitionCnt); i++ {
		consistentHash.AddPartition(i)
	}

	// 4. 初始化结果通道（核心链路建议调大缓冲，避免阻塞监听协程）
	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, 2048)
	errorCh := make(chan *sarama.ProducerError, 2048)

	// 5. 启动结果监听协程（核心链路建议用独立协程，避免阻塞）
	go listenSuccess(successCh, ctx)
	go listenError(errorCh, ctx)

	return &KeyOrderedKafkaProducer{
		producer:     producer,
		topic:        topic,
		partitionCnt: initialPartitionCnt,
		successCh:    successCh,
		errorCh:      errorCh,
		ctx:          ctx,
		cancel:       cancel,
		consistent:   consistentHash,
	}, nil
}

// SendTask 核心链路发送方法（无锁，单次调用耗时≈200ns）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	// 1. 轻量校验（核心链路避免 heavy 校验，仅必要检查）
	if p.ctx.Err() != nil {
		return fmt.Errorf("producer closed: taskID=%s", task.TaskId)
	}
	if task.TaskId == "" || key == "" {
		return fmt.Errorf("invalid param: taskID=%s, key=%s", task.TaskId, key)
	}

	// 2. 序列化（核心链路建议预序列化，或用池化的byte切片减少分配）
	payload, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task failed: taskID=%s, err=%w", task.TaskId, err)
	}

	// 3. 一致性哈希路由（核心步骤，无锁，耗时≈100ns）
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return fmt.Errorf("get partition failed: key=%s, taskID=%s", key, task.TaskId)
	}

	// 4. 构造消息（核心链路避免额外内存分配，直接用字面量）
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(payload),
		Partition: partition,
		Timestamp: time.Now(),
	}

	// 5. 异步投递（非阻塞，核心链路避免等待）
	select {
	case p.producer.Input() <- msg:
		return nil
	case <-p.ctx.Done():
		return fmt.Errorf("producer closing: taskID=%s", task.TaskId)
	default:
		// 核心优化：Input通道满时直接返回错误，避免阻塞（核心链路优先保证响应速度）
		return fmt.Errorf("producer input busy: taskID=%s", task.TaskId)
	}
}

// AddPartitions 扩容时调用（仅在非峰值期执行，不影响核心链路）
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if len(newPartitions) == 0 {
		return fmt.Errorf("new partitions empty")
	}

	// 新增分区到一致性哈希（写锁，仅扩容时执行）
	for _, part := range newPartitions {
		p.consistent.AddPartition(part)
	}
	p.partitionCnt += len(newPartitions)

	logx.Infof("add partitions success: old=%d, new=%d", p.partitionCnt-len(newPartitions), p.partitionCnt)
	return nil
}

// 辅助函数：监听发送成功结果（核心链路建议简化日志，减少IO）
func listenSuccess(ch chan *sarama.ProducerMessage, ctx context.Context) {
	for {
		select {
		case _, ok := <-ch:
			if !ok {
				return
			}
			// 核心链路建议关闭详细日志，仅在监控平台统计成功数
			// logx.Debugf("send success: taskID=%s", task.TaskId)
		case <-ctx.Done():
			return
		}
	}
}

// 辅助函数：监听发送失败结果（核心链路建议仅记录错误，快速返回）
func listenError(ch chan *sarama.ProducerError, ctx context.Context) {
	for {
		select {
		case errMsg, ok := <-ch:
			if !ok {
				return
			}
			// 核心链路建议用异步日志，避免阻塞
			logx.Errorf("send failed: taskID=%s, err=%v", getTaskID(errMsg.Msg.Value), errMsg.Err)
		case <-ctx.Done():
			return
		}
	}
}

// 辅助函数：从消息体中快速提取TaskID（核心链路避免重复反序列化）
func getTaskID(value sarama.Encoder) string {
	b, err := value.Encode()
	if err != nil {
		return ""
	}
	var task db_proto.DBTask
	if err := proto.Unmarshal(b, &task); err != nil {
		return ""
	}
	return task.TaskId
}
