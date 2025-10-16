package kafka

import (
	"context"
	"crypto/sha1"
	"fmt"
	"sync"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	db_proto "login/proto/service/go/grpc/db"
)

// KeyOrderedKafkaProducer 异步Kafka生产者，保证相同key的消息发送到固定分区
type KeyOrderedKafkaProducer struct {
	producer     sarama.AsyncProducer         // 异步生产者核心实例
	topic        string                       // 目标Kafka主题
	partitionCnt int                          // 主题分区数量
	mu           sync.Mutex                   // 并发安全锁（保护生产者操作）
	successCh    chan *sarama.ProducerMessage // 发送成功结果通道
	errorCh      chan *sarama.ProducerError   // 发送失败结果通道
	ctx          context.Context              // 生命周期控制上下文
	cancel       context.CancelFunc           // 上下文取消函数
}

// NewKeyOrderedKafkaProducer 创建异步有序生产者实例
// 参数说明：
// - bootstrapServers: Kafka集群地址（如 "127.0.0.1:9092"）
// - topic: 消息发送的目标主题
// - partitionCnt: 主题的分区总数（需与Kafka实际配置一致）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	partitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// 1. 配置Kafka生产者参数（适配异步模式+高可靠性）
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0                       // 匹配Kafka集群版本（避免兼容性问题）
	config.Producer.Return.Successes = true                // 启用发送成功回调（必须开启，否则无成功通知）
	config.Producer.Return.Errors = true                   // 启用发送失败回调（便于排查问题）
	config.Producer.RequiredAcks = sarama.WaitForAll       // 等待所有副本确认（最高可靠性，避免数据丢失）
	config.Producer.Retry.Max = 3                          // 临时失败重试次数（应对网络抖动）
	config.Producer.Retry.Backoff = 100 * time.Millisecond // 重试间隔（避免频繁重试压垮Kafka）
	config.Producer.Idempotent = true                      // 开启幂等性（防止重试导致消息重复）
	config.ChannelBufferSize = 2048                        // Input通道缓冲大小（避免短时间高并发导致阻塞）
	config.Net.MaxOpenRequests = 1                         // 与幂等性配合使用（必须设为1）

	// 2. 创建异步生产者实例
	producer, err := sarama.NewAsyncProducer([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("创建异步生产者失败: %w", err)
	}

	// 3. 初始化生命周期控制与结果通道
	ctx, cancel := context.WithCancel(context.Background())
	successCh := make(chan *sarama.ProducerMessage, 1024) // 带缓冲，避免回调协程阻塞
	errorCh := make(chan *sarama.ProducerError, 1024)

	// 4. 启动协程监听“发送成功”结果
	go func() {
		for {
			select {
			case msg, ok := <-producer.Successes():
				if !ok {
					logx.Info("发送成功通道已关闭，停止监听")
					return
				}
				successCh <- msg
				// 解析TaskID并打印详细日志（便于追踪）
				task := &db_proto.DBTask{}
				if payload, ok := msg.Value.(sarama.ByteEncoder); ok {
					if err := proto.Unmarshal(payload, task); err == nil {
						logx.Infof("消息发送成功 | TaskID: %s | 分区: %d | 偏移量: %d",
							task.TaskId, msg.Partition, msg.Offset)
					}
				}
			case <-ctx.Done():
				logx.Info("收到关闭信号，停止成功结果监听")
				return
			}
		}
	}()

	// 5. 启动协程监听“发送失败”结果
	go func() {
		for {
			select {
			case errMsg, ok := <-producer.Errors():
				if !ok {
					logx.Info("发送失败通道已关闭，停止监听")
					return
				}
				errorCh <- errMsg
				// 解析TaskID并打印错误日志（便于排查）
				task := &db_proto.DBTask{}
				if payload, ok := errMsg.Msg.Value.(sarama.ByteEncoder); ok {
					if err := proto.Unmarshal(payload, task); err == nil {
						logx.Errorf("消息发送失败 | TaskID: %s | 分区: %d | 原因: %w",
							task.TaskId, errMsg.Msg.Partition, errMsg.Err)
					} else {
						logx.Errorf("消息发送失败 | 解析TaskID失败 | 原因: %w", errMsg.Err)
					}
				}
			case <-ctx.Done():
				logx.Info("收到关闭信号，停止失败结果监听")
				return
			}
		}
	}()

	return &KeyOrderedKafkaProducer{
		producer:     producer,
		topic:        topic,
		partitionCnt: partitionCnt,
		successCh:    successCh,
		errorCh:      errorCh,
		ctx:          ctx,
		cancel:       cancel,
	}, nil
}

// SendTask 异步发送任务消息（相同key路由到固定分区，保证有序性）
// 参数说明：
// - task: 待发送的数据库任务Proto结构体
// - key: 业务key（如playerId字符串，用于计算固定分区）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 1. 校验生产者状态（避免已关闭后继续发送）
	if p.producer == nil || p.ctx.Err() != nil {
		return fmt.Errorf("生产者已关闭，无法发送消息 | TaskID: %s", task.TaskId)
	}

	// 2. 序列化Proto消息（转为字节流）
	payload, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("消息序列化失败 | TaskID: %s | 原因: %w", task.TaskId, err)
	}

	// 3. 计算固定分区（核心逻辑：相同key→固定分区，保证有序性）
	partition := p.getPartitionByKey(key)

	// 4. 构造Kafka消息（显式指定分区，不依赖默认路由）
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),   // 保留key用于追踪和Kafka审计
		Value:     sarama.ByteEncoder(payload), // 消息体（序列化后的Proto字节流）
		Partition: partition,                   // 显式指定分区（关键：保障有序性）
		Timestamp: time.Now(),                  // 记录发送时间（便于日志分析）
	}

	// 5. 异步投递消息到Input通道（非阻塞，避免阻塞上游业务）
	select {
	case p.producer.Input() <- msg:
		logx.Debugf("消息已投递到Input通道 | TaskID: %s | 目标分区: %d", task.TaskId, partition)
		return nil
	case <-p.ctx.Done():
		return fmt.Errorf("生产者正在关闭，消息投递失败 | TaskID: %s", task.TaskId)
	case <-time.After(500 * time.Millisecond): // 超时保护：避免通道满导致长时间阻塞
		return fmt.Errorf("Input通道繁忙，消息投递超时 | TaskID: %s", task.TaskId)
	}
}

// getPartitionByKey 基于业务key计算固定分区（SHA-1哈希+取模，保证相同key→固定分区）
func (p *KeyOrderedKafkaProducer) getPartitionByKey(key string) int32 {
	// 1. SHA-1哈希计算（哈希值分布均匀，减少分区倾斜）
	h := sha1.New()
	h.Write([]byte(key))
	hashBytes := h.Sum(nil)

	// 2. 哈希值转为uint32（取前4字节，避免溢出）
	uintHash := uint32(hashBytes[0])<<24 |
		uint32(hashBytes[1])<<16 |
		uint32(hashBytes[2])<<8 |
		uint32(hashBytes[3])

	// 3. 对分区数取模（得到0~partitionCnt-1的分区号）
	return int32(uintHash % uint32(p.partitionCnt))
}

// Close 优雅关闭生产者（释放资源，停止所有协程）
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 1. 已关闭则直接返回
	if p.producer == nil {
		return nil
	}

	// 2. 取消上下文，停止结果监听协程
	p.cancel()

	// 3. 关闭异步生产者（阻塞直到所有缓存消息处理完成，避免数据丢失）
	closeErr := p.producer.Close()
	if closeErr != nil {
		closeErr = fmt.Errorf("关闭异步生产者失败: %w", closeErr)
	} else {
		logx.Info("异步生产者关闭成功")
	}

	// 4. 标记生产者为nil（避免重复关闭）
	p.producer = nil

	// 5. 关闭结果通道
	close(p.successCh)
	close(p.errorCh)

	return closeErr
}

// GetSuccessCh 暴露成功结果通道（可选：业务方需自定义处理成功结果时使用）
func (p *KeyOrderedKafkaProducer) GetSuccessCh() <-chan *sarama.ProducerMessage {
	return p.successCh
}

// GetErrorCh 暴露失败结果通道（可选：业务方需自定义处理失败结果时使用）
func (p *KeyOrderedKafkaProducer) GetErrorCh() <-chan *sarama.ProducerError {
	return p.errorCh
}
