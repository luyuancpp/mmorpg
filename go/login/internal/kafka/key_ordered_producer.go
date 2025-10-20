package kafka

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/consistent"
	db_proto "login/proto/service/go/grpc/db"
)

// ProducerMeta 消息元数据，绑定生产者和原始payload（用于回收）
type ProducerMeta struct {
	producer *KeyOrderedKafkaProducer
	payload  []byte
}

// KeyOrderedKafkaProducer 基于一致性哈希的有序Kafka生产者
type KeyOrderedKafkaProducer struct {
	producer     sarama.AsyncProducer
	client       sarama.Client // 新增：用于元数据查询（获取分区列表）
	topic        string
	partitionCnt int
	mu           sync.Mutex // 保护producer、client和partitionCnt
	successCh    chan *sarama.ProducerMessage
	errorCh      chan *sarama.ProducerError
	ctx          context.Context
	cancel       context.CancelFunc
	consistent   *consistent.Consistent
	closed       bool
	payloadPool  sync.Pool // 复用[]byte减少GC

	// 消息统计计数器
	successCount int64
	errorCount   int64
}

// NewKeyOrderedKafkaProducer 初始化生产者（修复分区查询逻辑）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	initialPartitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// 1. 配置Kafka客户端（匹配Kafka 3.3+版本）
	config := sarama.NewConfig()
	config.Version = sarama.V3_6_0_0          // 匹配Kafka版本
	config.Net.DialTimeout = 10 * time.Second // 网络超时配置
	config.Net.ReadTimeout = 10 * time.Second
	config.Net.WriteTimeout = 10 * time.Second
	config.Producer.Return.Successes = true                // 启用成功回调
	config.Producer.Return.Errors = true                   // 启用错误回调
	config.Producer.RequiredAcks = sarama.WaitForLocal     // 本地写成功即返回
	config.Producer.Retry.Max = 3                          // 重试次数
	config.Producer.Retry.Backoff = 100 * time.Millisecond // 重试间隔
	config.ChannelBufferSize = 1024                        // 子通道缓冲大小
	config.Producer.Idempotent = false                     // 简化模式：关闭幂等性
	config.Producer.Compression = sarama.CompressionNone   // 关闭压缩

	// 验证配置合法性
	if err := config.Validate(); err != nil {
		logx.Errorf("Kafka配置无效: %v", err)
		return nil, err
	}

	// 2. 先创建Client（用于查询分区等元数据）
	client, err := sarama.NewClient([]string{bootstrapServers}, config)
	if err != nil {
		logx.Errorf("创建Kafka客户端失败: %v", err)
		return nil, fmt.Errorf("创建客户端失败: %w", err)
	}

	// 3. 通过Client创建AsyncProducer（共享连接和配置）
	producer, err := sarama.NewAsyncProducerFromClient(client)
	if err != nil {
		client.Close() // 失败时关闭Client，避免资源泄漏
		logx.Errorf("创建Kafka生产者失败: %v", err)
		return nil, fmt.Errorf("创建生产者失败: %w", err)
	}

	// 4. 初始化一致性哈希（用于分区路由）
	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(initialPartitionCnt); i++ {
		consistentHash.AddPartition(i)
	}

	// 5. 初始化上下文和结果通道
	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, 2048)
	errorCh := make(chan *sarama.ProducerError, 2048)

	// 6. 初始化payload池（复用[]byte）
	payloadPool := sync.Pool{
		New: func() interface{} {
			return make([]byte, 0, 1024) // 预设容量减少扩容
		},
	}

	// 7. 构造生产者实例（包含Client）
	kp := &KeyOrderedKafkaProducer{
		producer:     producer,
		client:       client, // 保存Client实例
		topic:        topic,
		partitionCnt: initialPartitionCnt,
		successCh:    successCh,
		errorCh:      errorCh,
		ctx:          ctx,
		cancel:       cancel,
		consistent:   consistentHash,
		closed:       false,
		payloadPool:  payloadPool,
	}

	// 8. 启动成功/失败监听
	go kp.listenSuccess()
	go kp.listenError()

	// 9. 启动Kafka内部错误监听
	go kp.listenProducerErrors()

	// 10. 启动分区同步（通过Client获取分区，修复核心错误）
	go kp.syncPartitions()

	// 11. 启动消息统计监控
	go kp.monitorStats()

	// 12. 发送测试消息验证流程
	go kp.sendTestMessage()

	return kp, nil
}

// SendTasks 批量发送任务（逻辑不变）
func (p *KeyOrderedKafkaProducer) SendTasks(tasks []*db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("生产者已关闭: 批量发送失败")
	}
	if len(tasks) == 0 || key == "" {
		return fmt.Errorf("无效参数: 任务数=%d, 键=%s", len(tasks), key)
	}

	// 计算分区（确保与Kafka实际分区匹配）
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return fmt.Errorf("获取分区失败: 键=%s", key)
	}

	// 批量序列化并发送
	var msgs []*sarama.ProducerMessage
	for _, task := range tasks {
		if task.TaskId == "" {
			p.recycleMessages(msgs) // 回收已生成消息
			return fmt.Errorf("任务ID为空: %+v", task)
		}

		// 从池获取缓冲区并序列化
		payload := p.payloadPool.Get().([]byte)
		payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
		if err != nil {
			p.payloadPool.Put(payload)
			p.recycleMessages(msgs)
			return fmt.Errorf("序列化任务失败: %s, %w", task.TaskId, err)
		}

		// 构造消息（绑定元数据用于回收）
		msgs = append(msgs, &sarama.ProducerMessage{
			Topic:     p.topic,
			Key:       sarama.StringEncoder(key),
			Value:     sarama.ByteEncoder(payload),
			Partition: partition,
			Timestamp: time.Now(),
			Metadata: &ProducerMeta{
				producer: p,
				payload:  payload,
			},
		})
	}

	// 批量发送（带超时控制）
	ctx, cancel := context.WithTimeout(p.ctx, 5*time.Second)
	defer cancel()
	for _, msg := range msgs {
		select {
		case p.producer.Input() <- msg:
			// 发送成功
		case <-ctx.Done():
			p.recycleMessages(msgs)
			return fmt.Errorf("批量发送超时: %v", ctx.Err())
		case <-p.ctx.Done():
			p.recycleMessages(msgs)
			return fmt.Errorf("生产者已关闭: 批量发送失败")
		}
	}

	return nil
}

// SendTask 单条发送任务（逻辑不变）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("生产者已关闭: 任务=%s", task.TaskId)
	}
	if task.TaskId == "" || key == "" {
		return fmt.Errorf("无效参数: 任务ID=%s, 键=%s", task.TaskId, key)
	}

	// 从池获取缓冲区并序列化
	payload := p.payloadPool.Get().([]byte)
	payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
	if err != nil {
		p.payloadPool.Put(payload)
		return fmt.Errorf("序列化任务失败: %s, %w", task.TaskId, err)
	}

	// 计算分区
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		p.payloadPool.Put(payload)
		return fmt.Errorf("获取分区失败: 键=%s, 任务=%s", key, task.TaskId)
	}

	// 构造消息
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(payload),
		Partition: partition,
		Timestamp: time.Now(),
		Metadata: &ProducerMeta{
			producer: p,
			payload:  payload,
		},
	}

	// 发送（带超时控制，避免非阻塞误判）
	ctx, cancel := context.WithTimeout(p.ctx, 5*time.Second)
	defer cancel()
	select {
	case p.producer.Input() <- msg:
		return nil
	case <-ctx.Done():
		p.payloadPool.Put(payload)
		return fmt.Errorf("发送超时: 任务=%s, %v", task.TaskId, ctx.Err())
	case <-p.ctx.Done():
		p.payloadPool.Put(payload)
		return fmt.Errorf("生产者已关闭: 任务=%s", task.TaskId)
	}
}

// AddPartitions 手动添加分区（优化去重逻辑）
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		return fmt.Errorf("生产者已关闭: 无法添加分区")
	}
	if len(newPartitions) == 0 {
		return fmt.Errorf("新分区列表为空")
	}

	// 去重添加（避免重复添加相同分区）
	added := 0
	existing := make(map[int32]bool)
	// 先收集已有的分区
	for _, part := range p.consistent.GetPartitions() {
		existing[part] = true
	}
	// 只添加新分区
	for _, part := range newPartitions {
		if !existing[part] {
			p.consistent.AddPartition(part)
			existing[part] = true
			added++
		}
	}
	p.partitionCnt += added

	logx.Infof("分区添加完成: 主题=%s, 原数量=%d, 新增=%d, 总数量=%d",
		p.topic, p.partitionCnt-added, added, p.partitionCnt)
	return nil
}

// Close 优雅关闭生产者（同时关闭Client）
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		logx.Errorf("生产者已关闭: 主题=%s", p.topic)
		return nil
	}

	// 标记关闭状态
	p.closed = true
	p.cancel() // 取消上下文

	// 先关闭Producer，再关闭Client（顺序不能反）
	producerErr := p.producer.Close()
	clientErr := p.client.Close()

	// 关闭结果通道
	close(p.successCh)
	close(p.errorCh)

	// 合并错误信息
	var finalErr error
	if producerErr != nil {
		finalErr = fmt.Errorf("producer关闭失败: %w", producerErr)
	}
	if clientErr != nil {
		if finalErr == nil {
			finalErr = fmt.Errorf("client关闭失败: %w", clientErr)
		} else {
			finalErr = fmt.Errorf("%v; client关闭失败: %w", finalErr, clientErr)
		}
	}

	if finalErr != nil {
		logx.Errorf("关闭资源失败: 主题=%s, 错误=%v", p.topic, finalErr)
		return finalErr
	}
	logx.Infof("生产者关闭成功: 主题=%s", p.topic)
	return nil
}

// 监听成功回调（逻辑不变）
func (p *KeyOrderedKafkaProducer) listenSuccess() {
	for {
		select {
		case msg, ok := <-p.successCh:
			if !ok {
				logx.Debug("成功通道已关闭")
				return
			}
			// 回收payload
			if meta, ok := msg.Metadata.(*ProducerMeta); ok {
				meta.producer.payloadPool.Put(meta.payload)
			}
			// 统计成功数
			atomic.AddInt64(&p.successCount, 1)
			// 打印调试日志
			taskID := getTaskID(msg.Value)
			logx.Debugf("消息发送成功: 任务=%s, 分区=%d, 偏移量=%d",
				taskID, msg.Partition, msg.Offset)
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出成功监听")
			return
		}
	}
}

// 监听错误回调（逻辑不变）
func (p *KeyOrderedKafkaProducer) listenError() {
	for {
		select {
		case errMsg, ok := <-p.errorCh:
			if !ok {
				logx.Debug("错误通道已关闭")
				return
			}
			// 回收payload
			if meta, ok := errMsg.Msg.Metadata.(*ProducerMeta); ok {
				meta.producer.payloadPool.Put(meta.payload)
			}
			// 统计错误数
			atomic.AddInt64(&p.errorCount, 1)
			// 打印错误日志
			taskID := getTaskID(errMsg.Msg.Value)
			logx.Errorf("消息发送失败: 任务=%s, 分区=%d, 错误=%v",
				taskID, errMsg.Msg.Partition, errMsg.Err)
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出错误监听")
			return
		}
	}
}

// 监听Sarama内部错误（逻辑不变）
func (p *KeyOrderedKafkaProducer) listenProducerErrors() {
	for {
		select {
		case err, ok := <-p.producer.Errors():
			if !ok {
				logx.Debug("Kafka内部错误通道已关闭")
				return
			}
			// 转发到自定义错误通道，统一处理
			p.errorCh <- err
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出内部错误监听")
			return
		}
	}
}

// 定期同步Kafka实际分区（核心修复：通过Client获取分区）
func (p *KeyOrderedKafkaProducer) syncPartitions() {
	ticker := time.NewTicker(30 * time.Second) // 每30秒同步一次
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			// 修复：通过Client获取分区列表（替代producer.Partitions）
			partitions, err := p.client.Partitions(p.topic)
			if err != nil {
				logx.Errorf("获取Kafka分区失败: 主题=%s, 错误=%v", p.topic, err)
				continue
			}
			// 同步到一致性哈希
			if err := p.AddPartitions(partitions); err != nil {
				logx.Errorf("同步分区失败: %v", err)
			}
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出分区同步")
			return
		}
	}
}

// 监控消息统计（逻辑不变）
func (p *KeyOrderedKafkaProducer) monitorStats() {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			success := atomic.SwapInt64(&p.successCount, 0)
			errs := atomic.SwapInt64(&p.errorCount, 0)
			logx.Infof("消息统计: 主题=%s, 5秒内成功=%d, 失败=%d",
				p.topic, success, errs)
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出统计监控")
			return
		}
	}
}

// 发送测试消息验证流程（逻辑不变）
func (p *KeyOrderedKafkaProducer) sendTestMessage() {
	time.Sleep(3 * time.Second) // 等待初始化完成
	testTask := &db_proto.DBTask{
		TaskId: fmt.Sprintf("test-%d", time.Now().Unix()),
	}
	if err := p.SendTask(testTask, "test-key"); err != nil {
		logx.Errorf("测试消息发送失败: %v", err)
	} else {
		logx.Infof("测试消息已发送: 任务ID=%s (等待回调确认)", testTask.TaskId)
	}
}

// 回收消息payload（逻辑不变）
func (p *KeyOrderedKafkaProducer) recycleMessages(msgs []*sarama.ProducerMessage) {
	for _, msg := range msgs {
		if meta, ok := msg.Metadata.(*ProducerMeta); ok {
			meta.producer.payloadPool.Put(meta.payload)
		}
	}
}

// 从消息中提取TaskID（逻辑不变）
func getTaskID(value sarama.Encoder) string {
	// 直接断言ByteEncoder，减少性能损耗
	if byteVal, ok := value.(sarama.ByteEncoder); ok {
		var task db_proto.DBTask
		err := proto.Unmarshal(byteVal, &task)
		if err == nil {
			return task.TaskId
		}
		return fmt.Sprintf("解析失败: %v", err)
	}
	// 降级处理其他Encoder类型
	b, err := value.Encode()
	if err != nil {
		return fmt.Sprintf("编码失败: %v", err)
	}
	var task db_proto.DBTask
	if err := proto.Unmarshal(b, &task); err != nil {
		return fmt.Sprintf("解析失败: %v", err)
	}
	return task.TaskId
}
