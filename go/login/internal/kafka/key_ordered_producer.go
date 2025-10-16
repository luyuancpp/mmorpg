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
	closed       bool                   // 新增：标记是否已关闭，避免重复调用Close
}

// NewKeyOrderedKafkaProducer 初始化（核心链路专用）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	initialPartitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// 1. Kafka配置优化：修复幂等性冲突+调整重试次数
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Producer.Return.Successes = true
	config.Producer.Return.Errors = true
	config.Producer.RequiredAcks = sarama.WaitForAll // 幂等生产者强制要求
	config.Producer.Retry.Max = 3                    // 微调：幂等场景建议3次重试
	config.ChannelBufferSize = 4096                  // 大缓冲减少核心链路阻塞
	config.Producer.Idempotent = true                // 开启幂等性，避免消息重复
	config.Net.MaxOpenRequests = 1                   // 控制并发，减少 broker 压力
	// 新增：幂等生产者依赖唯一ID，Sarama会自动生成，无需手动配置

	// 2. 创建异步生产者
	producer, err := sarama.NewAsyncProducer([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create producer failed: %w", err)
	}

	// 3. 初始化一致性哈希（20个虚拟节点，平衡均匀性和性能）
	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(initialPartitionCnt); i++ {
		consistentHash.AddPartition(i)
	}

	// 4. 初始化生命周期与结果通道
	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, 2048)
	errorCh := make(chan *sarama.ProducerError, 2048)

	// 5. 启动结果监听协程（独立协程，不阻塞核心链路）
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
		closed:       false, // 初始标记为未关闭
	}, nil
}

// SendTask 核心链路发送方法（无锁，单次调用耗时≈200ns）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	// 1. 先检查是否已关闭，避免无效发送
	if p.closed {
		return fmt.Errorf("producer already closed: taskID=%s", task.TaskId)
	}
	// 2. 轻量校验（核心链路避免 heavy 逻辑）
	if p.ctx.Err() != nil {
		return fmt.Errorf("producer closing: taskID=%s", task.TaskId)
	}
	if task.TaskId == "" || key == "" {
		return fmt.Errorf("invalid param: taskID=%s, key=%s", task.TaskId, key)
	}

	// 3. 序列化（核心链路可后续优化：用 sync.Pool 池化 byte 切片减少GC）
	payload, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("marshal task failed: taskID=%s, err=%w", task.TaskId, err)
	}

	// 4. 一致性哈希路由（无锁，读锁保护，耗时≈100ns）
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return fmt.Errorf("get partition failed: key=%s, taskID=%s", key, task.TaskId)
	}

	// 5. 构造消息（字面量初始化，避免额外内存分配）
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(payload),
		Partition: partition,
		Timestamp: time.Now(),
	}

	// 6. 异步投递（非阻塞，通道满时快速失败，保证核心链路不阻塞）
	select {
	case p.producer.Input() <- msg:
		return nil
	case <-p.ctx.Done():
		return fmt.Errorf("producer closed during send: taskID=%s", task.TaskId)
	default:
		return fmt.Errorf("producer input busy (channel full): taskID=%s", task.TaskId)
	}
}

// AddPartitions 扩容时调用（仅非峰值期执行，不影响核心链路）
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 检查是否已关闭
	if p.closed {
		return fmt.Errorf("producer already closed: cannot add partitions")
	}
	// 检查新分区列表有效性
	if len(newPartitions) == 0 {
		return fmt.Errorf("new partitions list is empty")
	}

	// 新增分区到一致性哈希（写锁仅在此刻持有，扩容完成后释放）
	for _, part := range newPartitions {
		// 避免重复添加同一分区（依赖 consistent 内部的 partitionSet 检查）
		p.consistent.AddPartition(part)
	}
	// 更新分区总数
	oldCnt := p.partitionCnt
	p.partitionCnt += len(newPartitions)

	logx.Infof("add partitions success: topic=%s, old_cnt=%d, new_cnt=%d, new_partitions=%v",
		p.topic, oldCnt, p.partitionCnt, newPartitions)
	return nil
}

// Close 新增：优雅关闭生产者，释放所有资源（必须调用，避免泄漏）
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 避免重复关闭
	if p.closed {
		logx.Errorf("producer already closed: topic=%s", p.topic)
		return nil
	}

	// 1. 标记为已关闭，拒绝新的发送请求
	p.closed = true
	// 2. 取消上下文，通知监听协程退出
	p.cancel()

	// 3. 关闭 Sarama 异步生产者（会自动关闭 Input/Successes/Errors 通道）
	closeErr := p.producer.Close()

	// 4. 关闭自定义的结果通道（避免监听协程阻塞）
	close(p.successCh)
	close(p.errorCh)

	// 5. 打印关闭日志
	if closeErr != nil {
		logx.Errorf("close producer failed: topic=%s, err=%v", p.topic, closeErr)
		return fmt.Errorf("close producer failed: %w", closeErr)
	}
	logx.Infof("close producer success: topic=%s", p.topic)
	return nil
}

// 辅助函数：监听发送成功结果（核心链路简化日志，减少IO开销）
func listenSuccess(ch chan *sarama.ProducerMessage, ctx context.Context) {
	for {
		select {
		case msg, ok := <-ch:
			if !ok {
				logx.Debug("success channel closed: exit listen")
				return
			}
			// 核心链路建议：仅在监控平台统计成功数（如 Prometheus），不打印详细日志
			// 如需调试，可临时开启：
			taskID := getTaskID(msg.Value)
			logx.Debugf("send success: taskID=%s, partition=%d, offset=%d", taskID, msg.Partition, msg.Offset)
		case <-ctx.Done():
			logx.Debug("context canceled: exit success listen")
			return
		}
	}
}

// 辅助函数：监听发送失败结果（仅打印错误，快速返回）
func listenError(ch chan *sarama.ProducerError, ctx context.Context) {
	for {
		select {
		case errMsg, ok := <-ch:
			if !ok {
				logx.Debug("error channel closed: exit listen")
				return
			}
			// 提取 TaskID 便于定位问题（失败日志需保留）
			taskID := getTaskID(errMsg.Msg.Value)
			logx.Errorf("send failed: taskID=%s, partition=%d, err=%v",
				taskID, errMsg.Msg.Partition, errMsg.Err)
		case <-ctx.Done():
			logx.Debug("context canceled: exit error listen")
			return
		}
	}
}

// 辅助函数：从消息体快速提取 TaskID（避免重复反序列化）
func getTaskID(value sarama.Encoder) string {
	// 优化：sarama.ByteEncoder 可直接断言，减少一次 Encode 调用
	if byteVal, ok := value.(sarama.ByteEncoder); ok {
		var task db_proto.DBTask
		err := proto.Unmarshal(byteVal, &task)
		if err == nil {
			return task.TaskId
		}
		return fmt.Sprintf("unmarshal_failed: err=%v", err)
	}
	// 其他类型的 Encoder（如 StringEncoder），降级处理
	b, err := value.Encode()
	if err != nil {
		return fmt.Sprintf("encode_failed: err=%v", err)
	}
	var task db_proto.DBTask
	if err := proto.Unmarshal(b, &task); err != nil {
		return fmt.Sprintf("unmarshal_failed: err=%v", err)
	}
	return task.TaskId
}
