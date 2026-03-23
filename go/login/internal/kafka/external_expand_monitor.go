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

// ExternalExpandMonitor monitors external partition expansion by polling.
type ExternalExpandMonitor struct {
	kafkaClient   sarama.Client
	redisClient   redis.Cmdable
	topic         string
	producer      *KeyOrderedKafkaProducer
	checkInterval time.Duration
	lastPartCount int32
	ctx           context.Context
	cancel        context.CancelFunc
	isExpanding   bool // true while handling an expansion
}

// NewExternalExpandMonitor creates an ExternalExpandMonitor.
func NewExternalExpandMonitor(
	kafkaBrokerList []string,
	topic string,
	redisClient redis.Cmdable,
	producer *KeyOrderedKafkaProducer,
	checkInterval ...time.Duration,
) (*ExternalExpandMonitor, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	kafkaClient, err := sarama.NewClient(kafkaBrokerList, config)
	if err != nil {
		return nil, fmt.Errorf("failed to create Kafka client: %w", err)
	}

	initialPartCount, err := kafkautil.GetCurrentPartitionCount(kafkaClient, topic)
	if err != nil {
		return nil, fmt.Errorf("failed to get initial partition count: %w", err)
	}

	interval := 5 * time.Second
	if len(checkInterval) > 0 && checkInterval[0] > 0 {
		interval = checkInterval[0]
	}

	ctx, cancel := context.WithCancel(context.Background())

	return &ExternalExpandMonitor{
		kafkaClient:   kafkaClient,
		redisClient:   redisClient,
		topic:         topic,
		producer:      producer,
		checkInterval: interval,
		lastPartCount: initialPartCount,
		ctx:           ctx,
		cancel:        cancel,
		isExpanding:   false,
	}, nil
}

// Start begins periodic partition polling.
func (m *ExternalExpandMonitor) Start() {
	logx.Infof("external expand monitor started | topic: %s | initialPartitions: %d | checkInterval: %v",
		m.topic, m.lastPartCount, m.checkInterval)

	ticker := time.NewTicker(m.checkInterval)
	defer ticker.Stop()

	go func() {
		for {
			select {
			case <-m.ctx.Done():
				logx.Infof("external expand monitor stopped | topic: %s", m.topic)
				return
			case <-ticker.C:
				m.checkPartitionChange()
			}
		}
	}()
}

// checkPartitionChange checks for partition count changes and handles expansion.
func (m *ExternalExpandMonitor) checkPartitionChange() {
	if m.isExpanding {
		logx.Debugf("external expand monitor: expansion in progress, skipping | topic: %s", m.topic)
		return
	}

	currentPartCount, err := kafkautil.GetCurrentPartitionCount(m.kafkaClient, m.topic)
	if err != nil {
		logx.Errorf("external expand monitor: failed to get partition count | topic: %s | err: %v", m.topic, err)
		return
	}

	if currentPartCount == m.lastPartCount {
		return
	}

	if currentPartCount < m.lastPartCount {
		logx.Errorf("external expand monitor: partition count decreased (anomaly) | topic: %s | old: %d | new: %d",
			m.topic, m.lastPartCount, currentPartCount)
		return
	}

	logx.Infof("external expand monitor: detected expansion | topic: %s | old: %d | new: %d",
		m.topic, m.lastPartCount, currentPartCount)
	m.isExpanding = true
	defer func() { m.isExpanding = false }()

	if err := kafkautil.SetExpandStatus(m.ctx, m.redisClient, m.topic, kafkautil.ExpandStatusExpanding, currentPartCount); err != nil {
		logx.Errorf("external expand monitor: failed to set expanding status | topic: %s | err: %v", m.topic, err)
		return
	}

	oldPartCount := m.lastPartCount
	if err := kafkautil.WaitOldPartitionsConsumed(m.ctx, m.kafkaClient, m.topic, oldPartCount); err != nil {
		logx.Errorf("external expand monitor: failed to wait for old partitions | topic: %s | err: %v", m.topic, err)
		setErr := kafkautil.SetExpandStatus(m.ctx, m.redisClient, m.topic, kafkautil.ExpandStatusNormal, currentPartCount)
		if setErr != nil {
			logx.Errorf("external expand monitor: failed to rollback expand status | topic: %s | err: %v", m.topic, setErr)
		}
		return
	}

	newPartIDs := kafkautil.GetNewPartitionIDs(oldPartCount, currentPartCount)
	if err := m.producer.AddPartitions(newPartIDs); err != nil {
		logx.Errorf("external expand monitor: failed to update producer partitions | topic: %s | newIDs: %v | err: %v",
			m.topic, newPartIDs, err)
		return
	}

	if err := kafkautil.SetExpandStatus(m.ctx, m.redisClient, m.topic, kafkautil.ExpandStatusCompleted, currentPartCount); err != nil {
		logx.Errorf("external expand monitor: failed to set completed status | topic: %s | err: %v", m.topic, err)
		return
	}

	m.lastPartCount = currentPartCount
	logx.Infof("external expand monitor: expansion complete | topic: %s | finalPartitions: %d", m.topic, currentPartCount)
}

// Stop stops the monitor.
func (m *ExternalExpandMonitor) Stop() {
	m.cancel()
	if err := m.kafkaClient.Close(); err != nil {
		logx.Errorf("external expand monitor: failed to close Kafka client | err: %v", err)
	} else {
		logx.Infof("external expand monitor: Kafka client closed")
	}
}
