package kafka

import (
	"context"
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// ExternalExpandMonitor 外部工具扩容的监控器：定时查询分区数量，被动感知扩容
type ExternalExpandMonitor struct {
	kafkaClient   sarama.Client            // Kafka客户端：用于查询分区数量
	redisClient   redis.Cmdable            // Redis客户端：用于存储扩容状态
	topic         string                   // 监控的Kafka主题
	producer      *KeyOrderedKafkaProducer // 生产者：扩容后需更新其分区配置
	checkInterval time.Duration            // 定时检查间隔（默认5秒，可调整）
	lastPartCount int32                    // 上一次查询的分区数量（用于对比变化）
	ctx           context.Context
	cancel        context.CancelFunc
	isExpanding   bool // 标记是否正在处理扩容（避免重复触发）
}

// NewExternalExpandMonitor 创建外部扩容监控器
func NewExternalExpandMonitor(
	kafkaBrokerList []string,          // Kafka broker地址列表
	topic string,                      // 待监控的主题
	redisClient redis.Cmdable,         // Redis客户端
	producer *KeyOrderedKafkaProducer, // 关联的生产者（扩容后更新分区）
	checkInterval ...time.Duration,    // 可选：检查间隔，默认5秒
) (*ExternalExpandMonitor, error) {
	// 1. 初始化Kafka客户端（仅用于查询元数据，无需生产/消费权限）
	config := sarama.NewConfig()
	config.Version = sarama.V3_5_0_0 // 匹配你的Kafka版本
	kafkaClient, err := sarama.NewClient(kafkaBrokerList, config)
	if err != nil {
		return nil, fmt.Errorf("创建Kafka客户端失败: %w", err)
	}

	// 2. 获取初始分区数量（作为后续对比的基准）
	initialPartCount, err := GetCurrentPartitionCount(kafkaClient, topic)
	if err != nil {
		return nil, fmt.Errorf("查询初始分区数量失败: %w", err)
	}

	// 3. 处理检查间隔（默认5秒）
	interval := 5 * time.Second
	if len(checkInterval) > 0 && checkInterval[0] > 0 {
		interval = checkInterval[0]
	}

	// 4. 初始化上下文
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

// Start 启动监控器：定时检查分区数量变化
func (m *ExternalExpandMonitor) Start() {
	logx.Infof("外部扩容监控器启动 | 主题: %s | 初始分区数: %d | 检查间隔: %v",
		m.topic, m.lastPartCount, m.checkInterval)

	// 启动定时任务
	ticker := time.NewTicker(m.checkInterval)
	defer ticker.Stop()

	go func() {
		for {
			select {
			case <-m.ctx.Done():
				logx.Infof("外部扩容监控器停止 | 主题: %s", m.topic)
				return
			case <-ticker.C:
				m.checkPartitionChange() // 每次触发检查
			}
		}
	}()
}

// checkPartitionChange 核心逻辑：检查分区数量变化，触发扩容处理
func (m *ExternalExpandMonitor) checkPartitionChange() {
	// 避免并发处理扩容（防止多次触发）
	if m.isExpanding {
		logx.Debugf("外部扩容监控器：已有扩容任务在处理，跳过本次检查 | 主题: %s", m.topic)
		return
	}

	// 1. 查询当前Kafka集群的实际分区数量（外部工具扩容后，这里会变化）
	currentPartCount, err := GetCurrentPartitionCount(m.kafkaClient, m.topic)
	if err != nil {
		logx.Errorf("外部扩容监控器：查询当前分区数量失败 | 主题: %s | 错误: %v", m.topic, err)
		return
	}

	// 2. 对比分区数量：无变化则跳过
	if currentPartCount == m.lastPartCount {
		return
	}

	// 3. 分区数量减少：Kafka不支持主动减分区，大概率是异常，仅告警
	if currentPartCount < m.lastPartCount {
		logx.Errorf("外部扩容监控器：检测到分区数量减少（异常）| 主题: %s | 旧数量: %d | 新数量: %d",
			m.topic, m.lastPartCount, currentPartCount)
		return
	}

	// 4. 分区数量增加：确认是外部扩容，开始处理
	logx.Infof("外部扩容监控器：检测到外部工具扩容 | 主题: %s | 旧数量: %d | 新数量: %d",
		m.topic, m.lastPartCount, currentPartCount)
	m.isExpanding = true                     // 标记为正在处理扩容
	defer func() { m.isExpanding = false }() // 处理完后重置

	// 5. 复用工具函数：设置“扩容中”状态（通知消费端加锁）
	if err := SetExpandStatus(m.ctx, m.redisClient, m.topic, ExpandStatusExpanding, currentPartCount); err != nil {
		logx.Errorf("外部扩容监控器：设置扩容状态失败 | 主题: %s | 错误: %v", m.topic, err)
		return
	}

	// 6. 复用工具函数：等待旧分区消息消费完毕（避免跨分区顺序问题）
	oldPartCount := m.lastPartCount
	if err := WaitOldPartitionsConsumed(m.ctx, m.kafkaClient, m.topic, oldPartCount); err != nil {
		logx.Errorf("外部扩容监控器：等待旧分区消费失败 | 主题: %s | 错误: %v", m.topic, err)
		// 失败后回滚状态，避免消费端一直加锁
		setErr := SetExpandStatus(m.ctx, m.redisClient, m.topic, ExpandStatusNormal, currentPartCount)
		if setErr != nil {
			logx.Errorf("外部扩容监控器：回滚扩容状态失败 | 主题: %s | 错误: %v", m.topic, setErr)
		}
		return
	}

	// 7. 更新生产者的分区配置（复用工具函数生成新分区ID）
	newPartIDs := GetNewPartitionIDs(oldPartCount, currentPartCount)
	if err := m.producer.AddPartitions(newPartIDs); err != nil {
		logx.Errorf("外部扩容监控器：更新生产者分区失败 | 主题: %s | 新分区ID: %v | 错误: %v",
			m.topic, newPartIDs, err)
		return
	}

	// 8. 复用工具函数：设置“扩容完成”状态（通知消费端解锁）
	if err := SetExpandStatus(m.ctx, m.redisClient, m.topic, ExpandStatusCompleted, currentPartCount); err != nil {
		logx.Errorf("外部扩容监控器：设置扩容完成状态失败 | 主题: %s | 错误: %v", m.topic, err)
		return
	}

	// 9. 更新基准分区数量（下次检查用）
	m.lastPartCount = currentPartCount
	logx.Infof("外部扩容监控器：外部工具扩容处理完成 | 主题: %s | 最终分区数: %d", m.topic, currentPartCount)
}

// Stop 停止监控器
func (m *ExternalExpandMonitor) Stop() {
	m.cancel()
	if err := m.kafkaClient.Close(); err != nil {
		logx.Errorf("外部扩容监控器：关闭Kafka客户端失败 | 错误: %v", err)
	} else {
		logx.Infof("外部扩容监控器：Kafka客户端关闭成功")
	}
}
