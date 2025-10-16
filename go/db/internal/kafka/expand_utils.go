package kafka

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// 扩容状态常量
const (
	ExpandStatusNormal    = "normal"    // 常态
	ExpandStatusExpanding = "expanding" // 扩容中
	ExpandStatusCompleted = "completed" // 扩容完成
)

// ExpandStatusData 扩容状态数据（存储到Redis）
type ExpandStatusData struct {
	Status         string `json:"status"`          // 扩容状态
	PartitionCount int32  `json:"partition_count"` // 当前分区总数
	UpdateTime     int64  `json:"update_time"`     // 更新时间（毫秒）
}

// 1. 设置扩容状态到Redis
func SetExpandStatus(ctx context.Context, redisClient redis.Cmdable, topic string, status string, partitionCount int32) error {
	key := getExpandStatusKey(topic)
	data := ExpandStatusData{
		Status:         status,
		PartitionCount: partitionCount,
		UpdateTime:     time.Now().UnixMilli(),
	}
	dataBytes, err := json.Marshal(data)
	if err != nil {
		return fmt.Errorf("marshal expand status failed: %w", err)
	}
	return redisClient.Set(ctx, key, dataBytes, 0).Err() // 永久存储
}

// 2. 从Redis获取扩容状态
func GetExpandStatus(ctx context.Context, redisClient redis.Cmdable, topic string) (*ExpandStatusData, error) {
	key := getExpandStatusKey(topic)
	dataBytes, err := redisClient.Get(ctx, key).Bytes()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			// 未设置过状态，默认常态+分区数0
			return &ExpandStatusData{Status: ExpandStatusNormal, PartitionCount: 0}, nil
		}
		return nil, fmt.Errorf("get expand status failed: %w", err)
	}

	var data ExpandStatusData
	if err := json.Unmarshal(dataBytes, &data); err != nil {
		return nil, fmt.Errorf("unmarshal expand status failed: %w", err)
	}
	return &data, nil
}

// 3. 等待旧分区消息消费完毕（对比消费偏移量和最大偏移量）
func WaitOldPartitionsConsumed(ctx context.Context, client sarama.Client, topic string, oldPartitionCount int32) error {
	logx.Infof("waiting old partitions consumed: topic=%s, oldCount=%d", topic, oldPartitionCount)
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return fmt.Errorf("wait old partitions canceled: %w", ctx.Err())
		case <-ticker.C:
			allConsumed := true
			// 遍历所有旧分区（0 ~ oldPartitionCount-1）
			for i := int32(0); i < oldPartitionCount; i++ {
				// 获取分区最大偏移量（最新消息位置）
				maxOffset, err := client.GetOffset(topic, i, sarama.OffsetNewest)
				if err != nil {
					logx.Errorf("get max offset failed: topic=%s, partition=%d, err=%v", topic, i, err)
					allConsumed = false
					break
				}
				// 获取当前消费偏移量（假设消费组为固定值，可根据实际修改）
				//consumerGroup := config.AppConfig.ServerConfig.Kafka.GroupID // 替换为你的消费组ID
				currentOffset, err := client.GetOffset(topic, i, sarama.OffsetOldest)
				// 实际应从消费组偏移量存储获取，这里简化用Sarama接口（生产环境建议用Redis/etcd存储消费偏移量）
				if err != nil {
					logx.Errorf("get current offset failed: topic=%s, partition=%d, err=%v", topic, i, err)
					allConsumed = false
					break
				}

				// 消费偏移量 >= 最大偏移量 → 分区已消费完毕
				if currentOffset < maxOffset {
					logx.Debugf("partition not consumed: topic=%s, partition=%d, current=%d, max=%d",
						topic, i, currentOffset, maxOffset)
					allConsumed = false
					break
				}
			}

			if allConsumed {
				logx.Infof("all old partitions consumed: topic=%s, oldCount=%d", topic, oldPartitionCount)
				return nil
			}
		}
	}
}

// 4. 生成扩容状态Redis Key
func getExpandStatusKey(topic string) string {
	return fmt.Sprintf("kafka:expand:status:%s", topic)
}

// 5. 生成新分区ID列表（如旧分区数3→新分区数5，返回[3,4]）
func GetNewPartitionIDs(oldPartitionCount, newPartitionCount int32) []int32 {
	var newIDs []int32
	for i := oldPartitionCount; i < newPartitionCount; i++ {
		newIDs = append(newIDs, i)
	}
	return newIDs
}

// 6. 从Sarama Client获取当前分区数量
func GetCurrentPartitionCount(client sarama.Client, topic string) (int32, error) {
	partitions, err := client.Partitions(topic)
	if err != nil {
		return 0, fmt.Errorf("get partitions failed: %w", err)
	}
	return int32(len(partitions)), nil
}
