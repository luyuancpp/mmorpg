package kafka

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/consistent"
	db_proto "login/proto/service/go/grpc/db"
)

type KeyOrderedKafkaProducer struct {
	producer     sarama.AsyncProducer
	topic        string
	partitionCnt int
	mu           sync.Mutex // 仅保护producer和partitionCnt
	successCh    chan *sarama.ProducerMessage
	errorCh      chan *sarama.ProducerError
	ctx          context.Context
	cancel       context.CancelFunc
	consistent   *consistent.Consistent
	closed       bool
	// 新增：byte切片池，减少批量序列化GC
	payloadPool sync.Pool
}

// NewKeyOrderedKafkaProducer 初始化（新增payloadPool）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	initialPartitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	config.Producer.Return.Successes = true
	config.Producer.Return.Errors = true
	config.Producer.RequiredAcks = sarama.WaitForAll
	config.Producer.Retry.Max = 3
	config.ChannelBufferSize = 4096 // 大缓冲适配批量投递
	config.Producer.Idempotent = true
	config.Net.MaxOpenRequests = 1

	producer, err := sarama.NewAsyncProducer([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create producer failed: %w", err)
	}

	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(initialPartitionCnt); i++ {
		consistentHash.AddPartition(i)
	}

	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, 2048)
	errorCh := make(chan *sarama.ProducerError, 2048)

	// 启动结果监听
	go listenSuccess(successCh, ctx)
	go listenError(errorCh, ctx)

	// 初始化payload池：预设1024字节容量，减少扩容
	payloadPool := sync.Pool{
		New: func() interface{} {
			return make([]byte, 0, 1024)
		},
	}

	return &KeyOrderedKafkaProducer{
		producer:     producer,
		topic:        topic,
		partitionCnt: initialPartitionCnt,
		successCh:    successCh,
		errorCh:      errorCh,
		ctx:          ctx,
		cancel:       cancel,
		consistent:   consistentHash,
		closed:       false,
		payloadPool:  payloadPool,
	}, nil
}

// 新增：批量发送任务（适配一帧多发）
func (p *KeyOrderedKafkaProducer) SendTasks(tasks []*db_proto.DBTask, key string) error {
	// 1. 前置状态校验（批量只校验1次，减少重复判断）
	if p.closed {
		return fmt.Errorf("producer already closed: batch send failed")
	}
	if p.ctx.Err() != nil {
		return fmt.Errorf("producer closing: batch send failed")
	}
	if len(tasks) == 0 || key == "" {
		return fmt.Errorf("invalid batch param: tasks len=%d, key=%s", len(tasks), key)
	}

	// 2. 批量计算分区（一帧同key，只算1次哈希，减少路由耗时）
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return fmt.Errorf("get partition failed: key=%s", key)
	}

	// 3. 批量序列化（复用payload池，减少GC）
	var msgs []*sarama.ProducerMessage
	for _, task := range tasks {
		if task.TaskId == "" {
			return fmt.Errorf("invalid taskID in batch: task=%+v", task)
		}

		// 从池获取切片，清空后复用
		payload := p.payloadPool.Get().([]byte)
		payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
		if err != nil {
			p.payloadPool.Put(payload) // 失败时放回池
			return fmt.Errorf("marshal task %s failed: %w", task.TaskId, err)
		}

		// 构造消息（字面量初始化，无额外内存分配）
		msgs = append(msgs, &sarama.ProducerMessage{
			Topic:     p.topic,
			Key:       sarama.StringEncoder(key),
			Value:     sarama.ByteEncoder(payload),
			Partition: partition,
			Timestamp: time.Now(),
		})

		// 暂不归还池：消息发送后，在success/error监听中回收（避免提前复用导致数据污染）
	}

	// 4. 批量投递（循环写入通道，非阻塞，通道满时快速失败）
	for _, msg := range msgs {
		select {
		case p.producer.Input() <- msg:
			// 发送成功：后续由监听协程回收payload
		case <-p.ctx.Done():
			// 上下文关闭：归还未发送的payload
			for i := range msgs {
				if byteVal, ok := msgs[i].Value.(sarama.ByteEncoder); ok {
					p.payloadPool.Put(byteVal)
				}
			}
			return fmt.Errorf("producer closed during batch send")
		default:
			// 通道满：归还所有payload
			for i := range msgs {
				if byteVal, ok := msgs[i].Value.(sarama.ByteEncoder); ok {
					p.payloadPool.Put(byteVal)
				}
			}
			return fmt.Errorf("producer input busy (batch send failed): key=%s", key)
		}
	}

	return nil
}

// 原有单条发送方法（保持不变）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("producer already closed: taskID=%s", task.TaskId)
	}
	if p.ctx.Err() != nil {
		return fmt.Errorf("producer closing: taskID=%s", task.TaskId)
	}
	if task.TaskId == "" || key == "" {
		return fmt.Errorf("invalid param: taskID=%s, key=%s", task.TaskId, key)
	}

	// 复用payload池
	payload := p.payloadPool.Get().([]byte)
	payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
	if err != nil {
		p.payloadPool.Put(payload)
		return fmt.Errorf("marshal task failed: taskID=%s, err=%w", task.TaskId, err)
	}

	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		p.payloadPool.Put(payload)
		return fmt.Errorf("get partition failed: key=%s, taskID=%s", key, task.TaskId)
	}

	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(payload),
		Partition: partition,
		Timestamp: time.Now(),
	}

	select {
	case p.producer.Input() <- msg:
		return nil
	case <-p.ctx.Done():
		p.payloadPool.Put(payload)
		return fmt.Errorf("producer closed during send: taskID=%s", task.TaskId)
	default:
		p.payloadPool.Put(payload)
		return fmt.Errorf("producer input busy: taskID=%s", task.TaskId)
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
func listenSuccess(ch chan *sarama.ProducerMessage, ctx context.Context) {
	for {
		select {
		case msg, ok := <-ch:
			if !ok {
				logx.Debug("success channel closed: exit listen")
				return
			}
			// 回收payload到池
			if byteVal, ok := msg.Value.(sarama.ByteEncoder); ok {
				if p, ok := getProducerFromMsg(msg); ok { // 需实现从msg反向获取producer（或通过闭包传递）
					p.payloadPool.Put(byteVal)
				}
			}
			taskID := getTaskID(msg.Value)
			logx.Debugf("send success: taskID=%s, partition=%d, offset=%d", taskID, msg.Partition, msg.Offset)
		case <-ctx.Done():
			logx.Debug("context canceled: exit success listen")
			return
		}
	}
}

// 辅助：从消息反向获取producer（需根据实际场景调整，或在启动监听时通过闭包传递producer）
func getProducerFromMsg(msg *sarama.ProducerMessage) (*KeyOrderedKafkaProducer, bool) {
	// 实际项目中，可通过msg.Metadata绑定producer引用，此处为示例
	if meta, ok := msg.Metadata.(*ProducerMeta); ok {
		return meta.producer, true
	}
	return nil, false
}

// 新增：消息元数据结构体，用于绑定producer
type ProducerMeta struct {
	producer *KeyOrderedKafkaProducer
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

// 其他原有方法（AddPartitions、Close、listenError、getTaskID）保持不变
