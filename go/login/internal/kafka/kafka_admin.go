package kafka

import (
	"context"
	kafka2 "db/internal/kafka"
	"fmt"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// KafkaAdmin Kafka管理员（主动触发扩容）
type KafkaAdmin struct {
	adminClient  sarama.ClusterAdmin      // Sarama集群管理客户端
	topic        string                   // 操作的主题
	redisClient  redis.Cmdable            // Redis客户端（存储扩容状态）
	producer     *KeyOrderedKafkaProducer // 生产者（用于新增分区）
	saramaClient sarama.Client            // Sarama客户端（用于查询分区）
}

// NewKafkaAdmin 创建Kafka管理员
func NewKafkaAdmin(
	bootstrapServers, topic string,
	redisClient redis.Cmdable,
	producer *KeyOrderedKafkaProducer,
) (*KafkaAdmin, error) {
	// 1. 配置Sarama
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0

	// 2. 创建ClusterAdmin（需要Kafka管理员权限）
	adminClient, err := sarama.NewClusterAdmin([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create cluster admin failed: %w", err)
	}

	// 3. 创建Sarama客户端（用于查询分区）
	saramaClient, err := sarama.NewClient([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create sarama client failed: %w", err)
	}

	return &KafkaAdmin{
		adminClient:  adminClient,
		topic:        topic,
		redisClient:  redisClient,
		producer:     producer,
		saramaClient: saramaClient,
	}, nil
}

// ExpandPartitions 主动扩容分区（指定目标总分区数）
func (a *KafkaAdmin) ExpandPartitions(ctx context.Context, targetTotalPartitions int32) error {
	// 1. 检查目标分区数合法性
	currentPartitionCount, err := kafka2.GetCurrentPartitionCount(a.saramaClient, a.topic)
	if err != nil {
		return fmt.Errorf("get current partition count failed: %w", err)
	}
	if targetTotalPartitions <= currentPartitionCount {
		return fmt.Errorf("target count=%d must be > current count=%d", targetTotalPartitions, currentPartitionCount)
	}
	if targetTotalPartitions > 1000 { // 限制最大分区数（可根据业务调整）
		return fmt.Errorf("target count=%d exceeds max limit 1000", targetTotalPartitions)
	}

	logx.Infof("start active expand: topic=%s, current=%d, target=%d",
		a.topic, currentPartitionCount, targetTotalPartitions)

	// 2. 调用Kafka Admin API创建分区
	partitionCountToAdd := targetTotalPartitions - currentPartitionCount
	err = a.adminClient.CreatePartitions(a.topic, partitionCountToAdd, nil, false)
	if err != nil {
		return fmt.Errorf("create partitions failed: addCount=%d, err=%w", partitionCountToAdd, err)
	}
	logx.Infof("create partitions success: topic=%s, addCount=%d", a.topic, partitionCountToAdd)

	// 3. 设置“扩容中”状态
	if err := kafka2.SetExpandStatus(ctx, a.redisClient, a.topic, kafka2.ExpandStatusExpanding, targetTotalPartitions); err != nil {
		return fmt.Errorf("set expanding status failed: err=%w", err)
	}

	// 4. 通知生产者新增分区
	newPartitions := kafka2.GetNewPartitionIDs(currentPartitionCount, targetTotalPartitions)
	a.producer.AddPartitions(newPartitions)

	// 5. 等待旧分区消息消费完毕
	if err := kafka2.WaitOldPartitionsConsumed(ctx, a.saramaClient, a.topic, currentPartitionCount); err != nil {
		return fmt.Errorf("wait old partitions consumed failed: err=%w", err)
	}

	// 6. 设置“扩容完成”状态
	if err := kafka2.SetExpandStatus(ctx, a.redisClient, a.topic, kafka2.ExpandStatusCompleted, targetTotalPartitions); err != nil {
		return fmt.Errorf("set completed status failed: err=%w", err)
	}

	logx.Infof("active expand success: topic=%s, currentPartitionCount=%d", a.topic, targetTotalPartitions)
	return nil
}

// Close 关闭管理员
func (a *KafkaAdmin) Close() error {
	// 关闭ClusterAdmin和SaramaClient
	adminErr := a.adminClient.Close()
	clientErr := a.saramaClient.Close()

	if adminErr != nil && clientErr != nil {
		return fmt.Errorf("close admin failed: %w, close client failed: %w", adminErr, clientErr)
	}
	if adminErr != nil {
		return fmt.Errorf("close admin failed: %w", adminErr)
	}
	if clientErr != nil {
		return fmt.Errorf("close client failed: %w", clientErr)
	}

	logx.Info("kafka admin closed success")
	return nil
}
