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

// ProducerMeta 消息元数据，绑定生产者和原始payload（用于回收）
type ProducerMeta struct {
	producer *KeyOrderedKafkaProducer
	payload  []byte
}

// ProducerConfig 可定制化生产者配置，支持外部注入参数

// KeyOrderedKafkaProducer 基于一致性哈希的有序Kafka生产者
type KeyOrderedKafkaProducer struct {
	producer     sarama.AsyncProducer
	client       sarama.Client // 用于元数据查询（获取分区列表）
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

	// 改进：容错配置
	unavailablePartitions map[int32]time.Time // 记录不可用分区及恢复时间
	retryInterval         time.Duration       // 不可用分区重试间隔
}

// NewKeyOrderedKafkaProducer 初始化生产者（支持可定制化配置）
func NewKeyOrderedKafkaProducer(cfg config.KafkaConfig) (*KeyOrderedKafkaProducer, error) {
	// 1. 配置Kafka客户端（使用外部注入的配置）
	config := sarama.NewConfig()
	config.Version = sarama.V3_6_0_0                                           // 匹配Kafka版本
	config.Net.DialTimeout = cfg.DialTimeout                                   // 外部配置：拨号超时
	config.Net.ReadTimeout = cfg.ReadTimeout                                   // 外部配置：读取超时
	config.Net.WriteTimeout = cfg.WriteTimeout                                 // 外部配置：写入超时
	config.Producer.Return.Successes = true                                    // 启用成功回调
	config.Producer.Return.Errors = true                                       // 启用错误回调
	config.Producer.RequiredAcks = sarama.WaitForAll                           // 本地写成功即返回
	config.Producer.Retry.Max = cfg.RetryMax                                   // 外部配置：最大重试次数
	config.Producer.Retry.Backoff = cfg.RetryBackoff                           // 外部配置：重试间隔
	config.ChannelBufferSize = cfg.ChannelBuffer                               // 外部配置：通道缓冲
	config.Producer.Idempotent = cfg.Idempotent                                // 外部配置：幂等性开关
	config.Producer.Compression = sarama.CompressionCodec(cfg.CompressionType) // 外部配置：压缩类型
	config.Net.MaxOpenRequests = cfg.MaxOpenRequests

	// 改进：启用幂等性时必须开启事务ID（避免重复消息）
	if cfg.Idempotent {
		config.Producer.Transaction.ID = fmt.Sprintf("kafka-producer-%d", time.Now().UnixNano())
	}

	// 验证配置合法性
	if err := config.Validate(); err != nil {
		logx.Errorf("Kafka配置无效: %v", err)
		return nil, err
	}

	// 2. 先创建Client（用于查询分区等元数据）
	client, err := sarama.NewClient([]string{cfg.BootstrapServers}, config)
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
	for i := int32(0); i < int32(cfg.InitialPartition); i++ {
		consistentHash.AddPartition(i)
	}

	// 5. 初始化上下文和结果通道
	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, cfg.ChannelBuffer)
	errorCh := make(chan *sarama.ProducerError, cfg.ChannelBuffer)

	// 6. 初始化payload池（复用[]byte）
	payloadPool := sync.Pool{
		New: func() interface{} {
			return make([]byte, 0, 1024) // 预设容量减少扩容
		},
	}

	// 7. 改进：初始化容错相关字段
	unavailableParts := make(map[int32]time.Time)
	retryInterval := 10 * time.Second // 不可用分区默认重试间隔

	// 8. 构造生产者实例（包含Client）
	kp := &KeyOrderedKafkaProducer{
		producer:     producer,
		client:       client,
		topic:        cfg.Topic,
		partitionCnt: cfg.InitialPartition,
		successCh:    successCh,
		errorCh:      errorCh,
		ctx:          ctx,
		cancel:       cancel,
		consistent:   consistentHash,
		closed:       false,
		payloadPool:  payloadPool,
		// 改进：容错字段赋值
		unavailablePartitions: unavailableParts,
		retryInterval:         retryInterval,
	}

	// 9. 启动成功/失败监听
	go kp.listenSuccess()
	go kp.listenError()

	// 10. 启动Kafka内部错误监听
	go kp.listenProducerErrors()

	// 11. 改进：使用外部配置的同步间隔，启动分区同步
	go kp.syncPartitions(cfg.SyncInterval)

	// 12. 改进：使用外部配置的统计间隔，启动消息统计监控
	go kp.monitorStats(cfg.StatsInterval)

	// 13. 改进：启动不可用分区恢复检查
	go kp.checkUnavailablePartitions()

	// 14. 发送测试消息验证流程
	go kp.sendTestMessage()

	return kp, nil
}

// SendTasks 批量发送任务（新增容错逻辑）
func (p *KeyOrderedKafkaProducer) SendTasks(tasks []*db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("生产者已关闭: 批量发送失败")
	}
	if len(tasks) == 0 || key == "" {
		return fmt.Errorf("无效参数: 任务数=%d, 键=%s", len(tasks), key)
	}

	// 改进：获取可用分区（跳过不可用分区）
	partition, ok := p.getAvailablePartition(key)
	if !ok {
		return fmt.Errorf("获取可用分区失败: 键=%s, 无可用分区", key)
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
			// 改进：标记分区为不可用
			p.markPartitionUnavailable(partition)
			return fmt.Errorf("批量发送超时: 分区=%d, %v", partition, ctx.Err())
		case <-p.ctx.Done():
			p.recycleMessages(msgs)
			return fmt.Errorf("生产者已关闭: 批量发送失败")
		}
	}

	return nil
}

// SendTask 单条发送任务（新增容错逻辑）
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

	// 改进：获取可用分区（跳过不可用分区）
	partition, ok := p.getAvailablePartition(key)
	if !ok {
		p.payloadPool.Put(payload)
		return fmt.Errorf("获取可用分区失败: 键=%s, 任务=%s, 无可用分区", key, task.TaskId)
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
		// 改进：标记分区为不可用
		p.markPartitionUnavailable(partition)
		return fmt.Errorf("发送超时: 任务=%s, 分区=%d, %v", task.TaskId, partition, ctx.Err())
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

// 监听错误回调（新增：标记不可用分区）
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

			// 改进：判断是否为分区不可用错误，标记对应分区
			if isPartitionUnavailableErr(errMsg.Err) {
				p.markPartitionUnavailable(errMsg.Msg.Partition)
			}
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

// 定期同步Kafka实际分区（支持外部配置同步间隔）
func (p *KeyOrderedKafkaProducer) syncPartitions(interval time.Duration) {
	if interval <= 0 {
		interval = 30 * time.Second // 默认30秒同步一次
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			// 通过Client获取分区列表（替代producer.Partitions）
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

// 监控消息统计（支持外部配置统计间隔）
func (p *KeyOrderedKafkaProducer) monitorStats(interval time.Duration) {
	if interval <= 0 {
		interval = 5 * time.Second // 默认5秒打印一次
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			success := atomic.SwapInt64(&p.successCount, 0)
			errs := atomic.SwapInt64(&p.errorCount, 0)
			// 改进：增加不可用分区数量统计
			unavailableCnt := p.getUnavailablePartitionCount()
			logx.Infof("消息统计: 主题=%s, 统计间隔=%v, 成功=%d, 失败=%d, 不可用分区数=%d",
				p.topic, interval, success, errs, unavailableCnt)
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

// 改进1：标记分区为不可用
func (p *KeyOrderedKafkaProducer) markPartitionUnavailable(partition int32) {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 记录分区不可用时间（用于后续恢复检查）
	p.unavailablePartitions[partition] = time.Now()
	logx.Errorf("分区标记为不可用: 主题=%s, 分区=%d, 恢复检查间隔=%v",
		p.topic, partition, p.retryInterval)
}

// 改进2：获取可用分区（跳过不可用分区）
func (p *KeyOrderedKafkaProducer) getAvailablePartition(key string) (int32, bool) {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 1. 先通过一致性哈希获取原始分区
	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return 0, false
	}

	// 2. 检查分区是否可用
	if _, isUnavailable := p.unavailablePartitions[partition]; !isUnavailable {
		return partition, true
	}

	// 3. 若原始分区不可用，尝试重新映射到其他可用分区
	allPartitions := p.consistent.GetPartitions()
	for _, part := range allPartitions {
		if _, isUnavailable := p.unavailablePartitions[part]; !isUnavailable {
			logx.Errorf("原始分区不可用，重新映射: 键=%s, 原分区=%d, 新分区=%d",
				key, partition, part)
			return part, true
		}
	}

	// 4. 无可用分区
	return 0, false
}

// 改进3：定期检查不可用分区，恢复可用状态
func (p *KeyOrderedKafkaProducer) checkUnavailablePartitions() {
	ticker := time.NewTicker(p.retryInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			p.mu.Lock()
			// 遍历所有不可用分区，检查是否超过恢复间隔
			for part, unavailableTime := range p.unavailablePartitions {
				if time.Since(unavailableTime) >= p.retryInterval {
					// 恢复分区可用状态
					delete(p.unavailablePartitions, part)
					logx.Infof("分区恢复可用: 主题=%s, 分区=%d, 不可用时长=%v",
						p.topic, part, time.Since(unavailableTime))
				}
			}
			p.mu.Unlock()
		case <-p.ctx.Done():
			logx.Debug("上下文已关闭: 退出不可用分区检查")
			return
		}
	}
}

// 改进4：判断是否为分区不可用错误
// 改进：修正分区不可用错误的判断逻辑
func isPartitionUnavailableErr(err error) bool {
	// 1. 先尝试将错误转换为sarama的ProducerError（最外层错误）
	if prodErr, ok := err.(*sarama.ProducerError); ok {
		// ProducerError的Err字段可能是PartitionError（结构体）
		// 通过错误信息字符串匹配判断是否为分区错误
		errMsg := prodErr.Error()
		if strings.Contains(errMsg, "partition") {
			// 2. 提取Kafka错误码（sarama.KError是int16类型）
			// 从错误信息中解析或直接比较已知错误码
			switch prodErr.Err {
			case sarama.ErrUnknownTopicOrPartition: // 3
			case sarama.ErrLeaderNotAvailable: // 6
			case sarama.ErrOffsetNotAvailable: // 19
			case sarama.ErrReplicaNotAvailable: // 9
				return true
			}
			// 3. 字符串匹配兜底（处理未知错误码但明确是分区不可用的情况）
			return strings.Contains(errMsg, "unavailable")
		}
	}

	// 4. 直接通过错误信息判断（非ProducerError但可能是分区问题）
	errMsg := err.Error()
	return strings.Contains(errMsg, "partition") && strings.Contains(errMsg, "unavailable")
}

// 辅助函数：判断字符串是否包含子串
func contains(s, substr string) bool {
	return len(substr) == 0 || index(s, substr) != -1
}

// 辅助函数：查找子串在字符串中的索引
func index(s, substr string) int {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return i
		}
	}
	return -1
}

// 改进5：获取不可用分区数量（用于统计）
func (p *KeyOrderedKafkaProducer) getUnavailablePartitionCount() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return len(p.unavailablePartitions)
}
