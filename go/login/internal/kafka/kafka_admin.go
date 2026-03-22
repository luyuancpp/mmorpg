package kafka

import (
	"context"
	"fmt"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// KafkaAdmin manages Kafka partition expansion.
type KafkaAdmin struct {
	adminClient  sarama.ClusterAdmin
	topic        string
	redisClient  redis.Cmdable
	producer     *KeyOrderedKafkaProducer
	saramaClient sarama.Client
}

// NewKafkaAdmin creates a KafkaAdmin.
func NewKafkaAdmin(
	bootstrapServers, topic string,
	redisClient redis.Cmdable,
	producer *KeyOrderedKafkaProducer,
) (*KafkaAdmin, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0

	adminClient, err := sarama.NewClusterAdmin([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create cluster admin failed: %w", err)
	}

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

// ExpandPartitions expands to the target total partition count.
func (a *KafkaAdmin) ExpandPartitions(ctx context.Context, targetTotalPartitions int32) error {
	currentPartitionCount, err := GetCurrentPartitionCount(a.saramaClient, a.topic)
	if err != nil {
		return fmt.Errorf("get current partition count failed: %w", err)
	}
	if targetTotalPartitions <= currentPartitionCount {
		return fmt.Errorf("target count=%d must be > current count=%d", targetTotalPartitions, currentPartitionCount)
	}
	if targetTotalPartitions > 1000 { // max partition limit
		return fmt.Errorf("target count=%d exceeds max limit 1000", targetTotalPartitions)
	}

	logx.Infof("start active expand: topic=%s, current=%d, target=%d",
		a.topic, currentPartitionCount, targetTotalPartitions)

	partitionCountToAdd := targetTotalPartitions - currentPartitionCount
	err = a.adminClient.CreatePartitions(a.topic, partitionCountToAdd, nil, false)
	if err != nil {
		return fmt.Errorf("create partitions failed: addCount=%d, err=%w", partitionCountToAdd, err)
	}
	logx.Infof("create partitions success: topic=%s, addCount=%d", a.topic, partitionCountToAdd)

	if err := SetExpandStatus(ctx, a.redisClient, a.topic, ExpandStatusExpanding, targetTotalPartitions); err != nil {
		return fmt.Errorf("set expanding status failed: err=%w", err)
	}

	newPartitions := GetNewPartitionIDs(currentPartitionCount, targetTotalPartitions)
	a.producer.AddPartitions(newPartitions)

	if err := WaitOldPartitionsConsumed(ctx, a.saramaClient, a.topic, currentPartitionCount); err != nil {
		return fmt.Errorf("wait old partitions consumed failed: err=%w", err)
	}

	if err := SetExpandStatus(ctx, a.redisClient, a.topic, ExpandStatusCompleted, targetTotalPartitions); err != nil {
		return fmt.Errorf("set completed status failed: err=%w", err)
	}

	logx.Infof("active expand success: topic=%s, currentPartitionCount=%d", a.topic, targetTotalPartitions)
	return nil
}

// Close closes the admin client.
func (a *KafkaAdmin) Close() error {
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
