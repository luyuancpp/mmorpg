package task

import (
	"context"
	"crypto/sha1"
	"fmt"
	"log"
	"login/internal/config"
	"login/internal/kafka" // 替换成你 KeyOrderedKafkaConsumer 的包路径
	db_proto "login/proto/service/go/grpc/db"
)

// EnqueueTaskWithIDKafka 使用 Kafka 发送任务，按 playerID 分区
func EnqueueTaskWithIDKafka(
	ctx context.Context,
	consumer *kafka.KeyOrderedKafkaConsumer,
	playerID uint64,
	taskID string,
	payload []byte,
) (string, error) {
	if taskID == "" {
		return "", fmt.Errorf("taskID cannot be empty")
	}
	if len(payload) == 0 {
		return "", fmt.Errorf("payload cannot be empty")
	}
	if consumer == nil {
		return "", fmt.Errorf("Kafka consumer is nil")
	}

	// 构造 DBTask
	task := &db_proto.DBTask{
		TaskId:  taskID,
		Op:      "write",       // 根据需要改成 write/read
		MsgType: "YourMessage", // protobuf 消息类型名称
		Body:    payload,
	}

	// 计算 key，保证同 playerID 落到同一个分区
	key := fmt.Sprintf("player:%d", playerID)

	// 发送到 Kafka
	if err := consumer.SendTask(ctx, task, key); err != nil {
		log.Printf("Kafka enqueue task failed: %v", err)
		return "", err
	}

	return taskID, nil
}

// 可选：如果需要像原来 Asynq 那样按 shard 名称
func getKafkaPartitionKey(playerID uint64) string {
	shardCount := config.AppConfig.Node.QueueShardCount
	return fmt.Sprintf("shard:%d", playerID%uint64(shardCount))
}

// 可选：hash key 到 int32 分区（可和 KeyOrderedKafkaConsumer.getPartitionByKey 对齐）
func hashKeyToPartition(key string, partitionCnt int32) int32 {
	h := sha1.New()
	h.Write([]byte(key))
	sum := h.Sum(nil)
	uintHash := uint32(sum[0])<<24 | uint32(sum[1])<<16 | uint32(sum[2])<<8 | uint32(sum[3])
	return int32(uintHash % uint32(partitionCnt))
}
