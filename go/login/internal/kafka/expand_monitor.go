package kafka

import (
	"context"
	"fmt"
	"time"

	"shared/kafkautil"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// ExpandMonitor detects external partition expansions.
type ExpandMonitor struct {
	client            sarama.Client
	topic             string
	redisClient       redis.Cmdable
	producer          *KeyOrderedKafkaProducer
	checkInterval     time.Duration
	oldPartitionCount int32
	ctx               context.Context
	cancel            context.CancelFunc
}

// NewExpandMonitor creates an ExpandMonitor.
func NewExpandMonitor(
	brokers []string, topic string,
	redisClient redis.Cmdable,
	producer *KeyOrderedKafkaProducer,
	checkInterval time.Duration,
) (*ExpandMonitor, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	client, err := sarama.NewClient(brokers, config)
	if err != nil {
		return nil, fmt.Errorf("create sarama client failed: %w", err)
	}

	oldPartitionCount, err := kafkautil.GetCurrentPartitionCount(client, topic)
	if err != nil {
		return nil, fmt.Errorf("get initial partition count failed: %w", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	if checkInterval <= 0 {
		checkInterval = 5 * time.Second
	}

	return &ExpandMonitor{
		client:            client,
		topic:             topic,
		redisClient:       redisClient,
		producer:          producer,
		checkInterval:     checkInterval,
		oldPartitionCount: oldPartitionCount,
		ctx:               ctx,
		cancel:            cancel,
	}, nil
}

// Start begins periodic partition monitoring.
func (m *ExpandMonitor) Start() {
	logx.Infof("expand monitor started: topic=%s, checkInterval=%v, initialPartitionCount=%d",
		m.topic, m.checkInterval, m.oldPartitionCount)

	go func() {
		ticker := time.NewTicker(m.checkInterval)
		defer ticker.Stop()

		for {
			select {
			case <-m.ctx.Done():
				logx.Infof("expand monitor stopped: topic=%s", m.topic)
				return
			case <-ticker.C:
				m.checkAndHandleExpand()
			}
		}
	}()
}

// checkAndHandleExpand checks for partition changes and handles expansion.
func (m *ExpandMonitor) checkAndHandleExpand() {
	currentPartitionCount, err := kafkautil.GetCurrentPartitionCount(m.client, m.topic)
	if err != nil {
		logx.Errorf("check partition count failed: topic=%s, err=%v", m.topic, err)
		return
	}

	if currentPartitionCount == m.oldPartitionCount {
		return
	}

	if currentPartitionCount < m.oldPartitionCount {
		logx.Errorf("partition count decreased: topic=%s, old=%d, current=%d",
			m.topic, m.oldPartitionCount, currentPartitionCount)
		return
	}

	logx.Infof("detected partition expand: topic=%s, old=%d, current=%d",
		m.topic, m.oldPartitionCount, currentPartitionCount)

	if err := kafkautil.SetExpandStatus(m.ctx, m.redisClient, m.topic, kafkautil.ExpandStatusExpanding, currentPartitionCount); err != nil {
		logx.Errorf("set expanding status failed: topic=%s, err=%v", m.topic, err)
		return
	}

	newPartitions := kafkautil.GetNewPartitionIDs(m.oldPartitionCount, currentPartitionCount)
	m.producer.AddPartitions(newPartitions)

	if err := kafkautil.WaitOldPartitionsConsumed(m.ctx, m.client, m.topic, m.oldPartitionCount); err != nil {
		logx.Errorf("wait old partitions consumed failed: topic=%s, err=%v", m.topic, err)
		return
	}

	if err := kafkautil.SetExpandStatus(m.ctx, m.redisClient, m.topic, kafkautil.ExpandStatusCompleted, currentPartitionCount); err != nil {
		logx.Errorf("set completed status failed: topic=%s, err=%v", m.topic, err)
		return
	}

	m.oldPartitionCount = currentPartitionCount
	logx.Infof("handle expand success: topic=%s, currentPartitionCount=%d", m.topic, currentPartitionCount)
}

// Stop stops the monitor.
func (m *ExpandMonitor) Stop() {
	m.cancel()
	if err := m.client.Close(); err != nil {
		logx.Errorf("close sarama client failed: err=%v", err)
	} else {
		logx.Info("sarama client closed success")
	}
}
