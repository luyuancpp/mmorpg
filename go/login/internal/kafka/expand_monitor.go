package kafka

import (
	"context"
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// ExpandMonitor 扩容监控器（被动感知外部扩容）
type ExpandMonitor struct {
	client            sarama.Client            // Sarama客户端（用于查询分区）
	topic             string                   // 监控的主题
	redisClient       redis.Cmdable            // Redis客户端（存储扩容状态）
	producer          *KeyOrderedKafkaProducer // 生产者（用于新增分区）
	checkInterval     time.Duration            // 检查间隔
	oldPartitionCount int32                    // 旧分区数量
	ctx               context.Context
	cancel            context.CancelFunc
}

// NewExpandMonitor 创建扩容监控器
func NewExpandMonitor(
	bootstrapServers, topic string,
	redisClient redis.Cmdable,
	producer *KeyOrderedKafkaProducer,
	checkInterval time.Duration,
) (*ExpandMonitor, error) {
	// 1. 初始化Sarama客户端（仅用于查询元数据）
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0
	client, err := sarama.NewClient([]string{bootstrapServers}, config)
	if err != nil {
		return nil, fmt.Errorf("create sarama client failed: %w", err)
	}

	// 2. 获取初始分区数量
	oldPartitionCount, err := GetCurrentPartitionCount(client, topic)
	if err != nil {
		return nil, fmt.Errorf("get initial partition count failed: %w", err)
	}

	// 3. 初始化上下文
	ctx, cancel := context.WithCancel(context.Background())
	if checkInterval <= 0 {
		checkInterval = 5 * time.Second // 默认5秒检查一次
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

// Start 启动监控器
func (m *ExpandMonitor) Start() {
	logx.Infof("expand monitor started: topic=%s, checkInterval=%v, initialPartitionCount=%d",
		m.topic, m.checkInterval, m.oldPartitionCount)

	// 启动定时检查协程
	ticker := time.NewTicker(m.checkInterval)
	defer ticker.Stop()

	go func() {
		for {
			select {
			case <-m.ctx.Done():
				logx.Info("expand monitor stopped: topic=%s", m.topic)
				return
			case <-ticker.C:
				m.checkAndHandleExpand()
			}
		}
	}()
}

// checkAndHandleExpand 检查分区变化并处理扩容
func (m *ExpandMonitor) checkAndHandleExpand() {
	// 1. 获取当前分区数量
	currentPartitionCount, err := GetCurrentPartitionCount(m.client, m.topic)
	if err != nil {
		logx.Errorf("check partition count failed: topic=%s, err=%v", m.topic, err)
		return
	}

	// 2. 无变化 → 跳过
	if currentPartitionCount == m.oldPartitionCount {
		return
	}

	// 3. 分区数量减少 → 告警（Kafka不支持减少分区，可能是异常）
	if currentPartitionCount < m.oldPartitionCount {
		logx.Errorf("partition count decreased: topic=%s, old=%d, current=%d",
			m.topic, m.oldPartitionCount, currentPartitionCount)
		return
	}

	// 4. 分区数量增加 → 处理扩容
	logx.Infof("detected partition expand: topic=%s, old=%d, current=%d",
		m.topic, m.oldPartitionCount, currentPartitionCount)

	// 4.1 设置“扩容中”状态
	if err := SetExpandStatus(m.ctx, m.redisClient, m.topic, ExpandStatusExpanding, currentPartitionCount); err != nil {
		logx.Errorf("set expanding status failed: topic=%s, err=%v", m.topic, err)
		return
	}

	// 4.2 通知生产者新增分区
	newPartitions := GetNewPartitionIDs(m.oldPartitionCount, currentPartitionCount)
	m.producer.AddPartitions(newPartitions)

	// 4.3 等待旧分区消息消费完毕
	if err := WaitOldPartitionsConsumed(m.ctx, m.client, m.topic, m.oldPartitionCount); err != nil {
		logx.Errorf("wait old partitions consumed failed: topic=%s, err=%v", m.topic, err)
		return
	}

	// 4.4 设置“扩容完成”状态
	if err := SetExpandStatus(m.ctx, m.redisClient, m.topic, ExpandStatusCompleted, currentPartitionCount); err != nil {
		logx.Errorf("set completed status failed: topic=%s, err=%v", m.topic, err)
		return
	}

	// 4.5 更新旧分区数量
	m.oldPartitionCount = currentPartitionCount
	logx.Infof("handle expand success: topic=%s, currentPartitionCount=%d", m.topic, currentPartitionCount)
}

// Stop 停止监控器
func (m *ExpandMonitor) Stop() {
	m.cancel()
	if err := m.client.Close(); err != nil {
		logx.Errorf("close sarama client failed: err=%v", err)
	} else {
		logx.Info("sarama client closed success")
	}
}
