package kafka

import (
	"context"
	"fmt"
	"login/internal/config"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/internal/logic/pkg/consistent"
	db_proto "login/proto/service/go/grpc/db"
)

// ProducerMeta 消息元数据，用于payload回收
type ProducerMeta struct {
	producer *KeyOrderedKafkaProducer
	payload  []byte
}

// KeyOrderedKafkaProducer 基于一致性哈希的有序Kafka生产者（使用SyncProducer支持幂等性）
type KeyOrderedKafkaProducer struct {
	producer     sarama.SyncProducer // 切换为SyncProducer支持事务
	client       sarama.Client       // 用于元数据查询
	topic        string
	partitionCnt int
	mu           sync.Mutex // 保护临界资源
	ctx          context.Context
	cancel       context.CancelFunc
	consistent   *consistent.Consistent
	closed       bool
	payloadPool  sync.Pool // 复用[]byte减少GC

	// 消息统计计数器
	successCount int64
	errorCount   int64

	// 容错配置
	unavailablePartitions map[int32]time.Time // 记录不可用分区及恢复时间
	retryInterval         time.Duration       // 不可用分区重试间隔
}

// NewKeyOrderedKafkaProducer 初始化支持幂等性的生产者（使用SyncProducer）
func NewKeyOrderedKafkaProducer(cfg config.KafkaConfig) (*KeyOrderedKafkaProducer, error) {
	// 1. 配置Kafka客户端（幂等性必需参数）
	config := sarama.NewConfig()
	config.Version = sarama.V3_6_0_0                                           // 匹配Kafka版本（需≥0.11.0.0）
	config.Net.DialTimeout = cfg.DialTimeout                                   // 拨号超时
	config.Net.ReadTimeout = cfg.ReadTimeout                                   // 读取超时
	config.Net.WriteTimeout = cfg.WriteTimeout                                 // 写入超时
	config.Producer.Return.Successes = true                                    // 启用成功回调
	config.Producer.Return.Errors = true                                       // 启用错误回调
	config.Producer.Retry.Max = cfg.RetryMax                                   // 最大重试次数
	config.Producer.Retry.Backoff = cfg.RetryBackoff                           // 重试间隔
	config.Producer.RequiredAcks = sarama.WaitForAll                           // 本地写成功即返回
	config.ChannelBufferSize = cfg.ChannelBuffer                               // 通道缓冲
	config.Producer.Compression = sarama.CompressionCodec(cfg.CompressionType) // 压缩类型
	config.Producer.Idempotent = cfg.Idempotent                                // 启用幂等性
	config.Net.MaxOpenRequests = 1                                             // 幂等性强制要求为1

	// 幂等性必需：设置唯一事务ID
	if cfg.Idempotent {
		config.Producer.Transaction.ID = fmt.Sprintf("kafka-producer-%d", time.Now().UnixNano())
	}

	// 验证配置合法性
	if err := config.Validate(); err != nil {
		logx.Errorf("Kafka配置无效: %v", err)
		return nil, err
	}

	// 2. 创建Client（用于查询分区等元数据）
	client, err := sarama.NewClient([]string{cfg.BootstrapServers}, config)
	if err != nil {
		logx.Errorf("创建Kafka客户端失败: %v", err)
		return nil, fmt.Errorf("创建客户端失败: %w", err)
	}

	// 3. 创建SyncProducer（支持事务操作，满足SyncProducer接口）
	producer, err := sarama.NewSyncProducerFromClient(client)
	if err != nil {
		client.Close() // 失败时关闭Client
		logx.Errorf("创建Kafka同步生产者失败: %v", err)
		return nil, fmt.Errorf("创建生产者失败: %w", err)
	}

	// 4. 初始化事务（幂等性必需步骤）
	if cfg.Idempotent {
		// 检查是否为事务型生产者
		if !producer.IsTransactional() {
			producer.Close()
			client.Close()
			return nil, fmt.Errorf("幂等性生产者需要事务支持，请检查Kafka版本≥0.11.0.0")
		}
		// 初始化事务
		if err := producer.BeginTxn(); err != nil {
			producer.Close()
			client.Close()
			logx.Errorf("开启事务失败: %v", err)
			return nil, fmt.Errorf("开启事务失败: %w", err)
		}
	}

	// 5. 初始化一致性哈希（用于分区路由）
	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(cfg.InitialPartition); i++ {
		consistentHash.AddPartition(i)
	}

	// 6. 初始化上下文和资源
	ctx, cancel := context.WithCancel(context.Background())
	payloadPool := sync.Pool{
		New: func() interface{} {
			return make([]byte, 0, 1024) // 预设容量减少扩容
		},
	}

	// 7. 初始化容错相关字段
	unavailableParts := make(map[int32]time.Time)
	retryInterval := 10 * time.Second // 不可用分区默认重试间隔

	// 8. 构造生产者实例
	kp := &KeyOrderedKafkaProducer{
		producer:              producer,
		client:                client,
		topic:                 cfg.Topic,
		partitionCnt:          cfg.InitialPartition,
		ctx:                   ctx,
		cancel:                cancel,
		consistent:            consistentHash,
		closed:                false,
		payloadPool:           payloadPool,
		unavailablePartitions: unavailableParts,
		retryInterval:         retryInterval,
	}

	// 9. 启动辅助协程
	go kp.syncPartitions(cfg.SyncInterval)
	go kp.monitorStats(cfg.StatsInterval)
	go kp.checkUnavailablePartitions()
	go kp.sendTestMessage()

	return kp, nil
}

// SendTasks 批量发送任务（使用SyncProducer的SendMessages接口）
func (p *KeyOrderedKafkaProducer) SendTasks(tasks []*db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("生产者已关闭: 批量发送失败")
	}
	if len(tasks) == 0 || key == "" {
		return fmt.Errorf("无效参数: 任务数=%d, 键=%s", len(tasks), key)
	}

	// 获取可用分区
	partition, ok := p.getAvailablePartition(key)
	if !ok {
		return fmt.Errorf("获取可用分区失败: 键=%s, 无可用分区", key)
	}

	// 批量序列化消息
	var msgs []*sarama.ProducerMessage
	for _, task := range tasks {
		if task.TaskId == "" {
			p.recycleMessages(msgs)
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

		// 构造消息
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

	// 使用SyncProducer的批量发送接口
	ctx, cancel := context.WithTimeout(p.ctx, 5*time.Second)
	defer cancel()

	// 同步发送（带超时控制）
	select {
	case <-ctx.Done():
		p.recycleMessages(msgs)
		p.markPartitionUnavailable(partition)
		return fmt.Errorf("批量发送超时: %v", ctx.Err())
	default:
		// 调用SyncProducer的SendMessages接口
		if err := p.producer.SendMessages(msgs); err != nil {
			p.recycleMessages(msgs)
			atomic.AddInt64(&p.errorCount, int64(len(msgs)))
			logx.Errorf("批量发送失败: 分区=%d, 错误=%v", partition, err)

			// 标记不可用分区
			if isPartitionUnavailableErr(err) {
				p.markPartitionUnavailable(partition)
			}

			// 幂等性模式下重启事务
			if p.producer.IsTransactional() {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("事务重启失败: %v", err)
				}
			}
			return err
		}

		// 幂等性模式下提交事务并开启新事务
		if p.producer.IsTransactional() {
			if err := p.producer.CommitTxn(); err != nil {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("事务提交失败后重启失败: %v", err)
				}
				return fmt.Errorf("提交事务失败: %w", err)
			}
			if err := p.producer.BeginTxn(); err != nil {
				return fmt.Errorf("开启新事务失败: %w", err)
			}
		}

		// 统计成功
		atomic.AddInt64(&p.successCount, int64(len(msgs)))
		return nil
	}
}

// SendTask 单条发送任务（使用SyncProducer的SendMessage接口）
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

	// 获取可用分区
	partition, ok := p.getAvailablePartition(key)
	if !ok {
		p.payloadPool.Put(payload)
		return fmt.Errorf("获取可用分区失败: 键=%s, 任务=%s", key, task.TaskId)
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

	// 同步发送（带超时控制）
	ctx, cancel := context.WithTimeout(p.ctx, 5*time.Second)
	defer cancel()

	select {
	case <-ctx.Done():
		p.payloadPool.Put(payload)
		p.markPartitionUnavailable(partition)
		return fmt.Errorf("发送超时: 任务=%s, %v", task.TaskId, ctx.Err())
	default:
		// 调用SyncProducer的SendMessage接口
		_, _, err := p.producer.SendMessage(msg)
		if err != nil {
			p.payloadPool.Put(payload)
			atomic.AddInt64(&p.errorCount, 1)
			logx.Errorf("消息发送失败: 任务=%s, 分区=%d, 错误=%v", task.TaskId, partition, err)

			// 标记不可用分区
			if isPartitionUnavailableErr(err) {
				p.markPartitionUnavailable(partition)
			}

			// 幂等性模式下重启事务
			if p.producer.IsTransactional() {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("事务重启失败: %v", err)
				}
			}
			return err
		}

		// 幂等性模式下提交事务并开启新事务
		if p.producer.IsTransactional() {
			if err := p.producer.CommitTxn(); err != nil {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("事务提交失败后重启失败: %v", err)
				}
				return fmt.Errorf("提交事务失败: %w", err)
			}
			if err := p.producer.BeginTxn(); err != nil {
				return fmt.Errorf("开启新事务失败: %w", err)
			}
		}

		// 回收payload（同步发送成功后回收）
		p.payloadPool.Put(payload)
		atomic.AddInt64(&p.successCount, 1)
		logx.Debugf("消息发送成功: 任务=%s, 分区=%d", task.TaskId, partition)
		return nil
	}
}

// AddPartitions 手动添加分区（逻辑不变）
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		return fmt.Errorf("生产者已关闭: 无法添加分区")
	}
	if len(newPartitions) == 0 {
		return fmt.Errorf("新分区列表为空")
	}

	added := 0
	existing := make(map[int32]bool)
	for _, part := range p.consistent.GetPartitions() {
		existing[part] = true
	}
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

// Close 优雅关闭生产者（实现事务关闭流程）
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		logx.Errorf("生产者已关闭: 主题=%s", p.topic)
		return nil
	}

	// 标记关闭状态
	p.closed = true
	p.cancel()

	// 幂等性模式下提交事务
	var txnErr error
	if p.producer.IsTransactional() {
		txnErr = p.producer.CommitTxn()
	}

	// 关闭生产者和客户端
	producerErr := p.producer.Close()
	clientErr := p.client.Close()

	// 合并错误信息
	var finalErr error
	if txnErr != nil {
		finalErr = fmt.Errorf("事务提交失败: %w", txnErr)
	}
	if producerErr != nil {
		if finalErr == nil {
			finalErr = fmt.Errorf("producer关闭失败: %w", producerErr)
		} else {
			finalErr = fmt.Errorf("%v; producer关闭失败: %w", finalErr, producerErr)
		}
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

// 定期同步Kafka实际分区（逻辑不变）
func (p *KeyOrderedKafkaProducer) syncPartitions(interval time.Duration) {
	if interval <= 0 {
		interval = 30 * time.Second
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			partitions, err := p.client.Partitions(p.topic)
			if err != nil {
				logx.Errorf("获取Kafka分区失败: 主题=%s, 错误=%v", p.topic, err)
				continue
			}
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
func (p *KeyOrderedKafkaProducer) monitorStats(interval time.Duration) {
	if interval <= 0 {
		interval = 5 * time.Second
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			success := atomic.SwapInt64(&p.successCount, 0)
			errs := atomic.SwapInt64(&p.errorCount, 0)
			unavailableCnt := p.getUnavailablePartitionCount()
			logx.Infof("消息统计: 主题=%s, 统计间隔=%v, 成功=%d, 失败=%d, 不可用分区数=%d",
				p.topic, interval, success, errs, unavailableCnt)
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出统计监控")
			return
		}
	}
}

// 发送测试消息验证流程
func (p *KeyOrderedKafkaProducer) sendTestMessage() {
	time.Sleep(3 * time.Second) // 等待初始化完成
	testTask := &db_proto.DBTask{
		TaskId: fmt.Sprintf("test-%d", time.Now().Unix()),
	}
	if err := p.SendTask(testTask, "test-key"); err != nil {
		logx.Errorf("测试消息发送失败: %v", err)
	} else {
		logx.Infof("测试消息已发送: 任务ID=%s", testTask.TaskId)
	}
}

// 回收消息payload
func (p *KeyOrderedKafkaProducer) recycleMessages(msgs []*sarama.ProducerMessage) {
	for _, msg := range msgs {
		if meta, ok := msg.Metadata.(*ProducerMeta); ok {
			meta.producer.payloadPool.Put(meta.payload)
		}
	}
}

// 从消息中提取TaskID
func getTaskID(value sarama.Encoder) string {
	if byteVal, ok := value.(sarama.ByteEncoder); ok {
		var task db_proto.DBTask
		err := proto.Unmarshal(byteVal, &task)
		if err == nil {
			return task.TaskId
		}
		return fmt.Sprintf("解析失败: %v", err)
	}
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

// 标记分区为不可用（逻辑不变）
func (p *KeyOrderedKafkaProducer) markPartitionUnavailable(partition int32) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.unavailablePartitions[partition] = time.Now()
	logx.Errorf("分区标记为不可用: 主题=%s, 分区=%d, 恢复检查间隔=%v",
		p.topic, partition, p.retryInterval)
}

// 获取可用分区（逻辑不变）
func (p *KeyOrderedKafkaProducer) getAvailablePartition(key string) (int32, bool) {
	p.mu.Lock()
	defer p.mu.Unlock()

	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return 0, false
	}

	if _, isUnavailable := p.unavailablePartitions[partition]; !isUnavailable {
		return partition, true
	}

	allPartitions := p.consistent.GetPartitions()
	for _, part := range allPartitions {
		if _, isUnavailable := p.unavailablePartitions[part]; !isUnavailable {
			logx.Errorf("原始分区不可用，重新映射: 键=%s, 原分区=%d, 新分区=%d",
				key, partition, part)
			return part, true
		}
	}

	return 0, false
}

// 定期检查不可用分区（逻辑不变）
func (p *KeyOrderedKafkaProducer) checkUnavailablePartitions() {
	ticker := time.NewTicker(p.retryInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			p.mu.Lock()
			now := time.Now()
			for part, unavailableTime := range p.unavailablePartitions {
				if now.Sub(unavailableTime) >= p.retryInterval {
					delete(p.unavailablePartitions, part)
					logx.Infof("分区恢复可用: 主题=%s, 分区=%d, 不可用时长=%v",
						p.topic, part, now.Sub(unavailableTime))
				}
			}
			p.mu.Unlock()
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出不可用分区检查")
			return
		}
	}
}

// 判断是否为分区不可用错误（逻辑不变）
func isPartitionUnavailableErr(err error) bool {
	if prodErr, ok := err.(*sarama.ProducerError); ok {
		errMsg := prodErr.Error()
		if strings.Contains(errMsg, "partition") {
			switch prodErr.Err {
			case sarama.ErrUnknownTopicOrPartition:
			case sarama.ErrLeaderNotAvailable:
			case sarama.ErrOffsetNotAvailable:
			case sarama.ErrReplicaNotAvailable:
				return true
			}
			return strings.Contains(errMsg, "unavailable")
		}
	}

	errMsg := err.Error()
	return strings.Contains(errMsg, "partition") && strings.Contains(errMsg, "unavailable")
}

// 获取不可用分区数量（逻辑不变）
func (p *KeyOrderedKafkaProducer) getUnavailablePartitionCount() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return len(p.unavailablePartitions)
}
