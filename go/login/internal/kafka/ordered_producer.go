package kafka

import (
	"crypto/sha1"
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	db_proto "login/proto/service/go/grpc/db"
	"sync"
)

// KeyOrderedKafkaProducer 独立的Kafka生产者，保证相同key的消息发送到固定分区
type KeyOrderedKafkaProducer struct {
	producer     sarama.SyncProducer // 同步生产者，确保消息发送结果可知
	topic        string              // 目标主题
	partitionCnt int                 // 主题分区数量
	mu           sync.Mutex          // 保护生产者操作的并发安全
}

// NewKeyOrderedKafkaProducer 创建生产者实例
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	partitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// 配置生产者参数
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0                       // 匹配Kafka集群版本
	config.Producer.Return.Successes = true                // 启用成功回调
	config.Producer.RequiredAcks = sarama.WaitForAll       // 等待所有副本确认（最高可靠性）
	config.Producer.Retry.Max = 3                          // 失败重试次数
	config.Producer.Retry.Backoff = 100 * time.Millisecond // 重试间隔
	config.Producer.Idempotent = true                      // 开启幂等性，避免重复发送

	// 创建同步生产者
	producer, err := sarama.NewSyncProducer([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("创建同步生产者失败: %v", err)
	}

	return &KeyOrderedKafkaProducer{
		producer:     producer,
		topic:        topic,
		partitionCnt: partitionCnt,
	}, nil
}

// SendTask 发送消息到指定分区（相同key会路由到同一分区）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 序列化Protobuf消息
	payload, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("消息序列化失败: %v", err)
	}

	// 根据key计算目标分区
	partition := p.getPartitionByKey(key)

	// 构造Kafka消息
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key), // 保留key用于追踪和分区计算
		Value:     sarama.ByteEncoder(payload),
		Partition: partition, // 显式指定分区，确保有序性
	}

	// 发送消息并获取结果
	partitionOut, offset, err := p.producer.SendMessage(msg)
	if err != nil {
		return fmt.Errorf("消息发送失败: %v", err)
	}

	logx.Infof("消息发送成功，TaskID: %s，分区: %d，偏移量: %d", task.TaskId, partitionOut, offset)
	return nil
}

// getPartitionByKey 基于key的哈希值计算分区，保证相同key路由到固定分区
func (p *KeyOrderedKafkaProducer) getPartitionByKey(key string) int32 {
	h := sha1.New()
	h.Write([]byte(key))
	hashBytes := h.Sum(nil)

	// 取哈希值前4字节转换为uint32，避免溢出
	uintHash := uint32(hashBytes[0])<<24 |
		uint32(hashBytes[1])<<16 |
		uint32(hashBytes[2])<<8 |
		uint32(hashBytes[3])

	// 对分区数取模，得到目标分区
	return int32(uintHash % uint32(p.partitionCnt))
}

// Close 关闭生产者连接，释放资源
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.producer != nil {
		return p.producer.Close()
	}
	return nil
}
