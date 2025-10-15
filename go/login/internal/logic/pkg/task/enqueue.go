package task

import (
	"fmt"
	"log"
	"login/internal/kafka" // 替换成你 KeyOrderedKafkaConsumer 的包路径
	db_proto "login/proto/service/go/grpc/db"
)

// EnqueueTaskWithIDKafka 使用 Kafka 发送任务，按 playerID 分区
func EnqueueTaskWithIDKafka(
	producer *kafka.KeyOrderedKafkaProducer,
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
	if producer == nil {
		return "", fmt.Errorf("Kafka producer is nil")
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
	if err := producer.SendTask(task, key); err != nil {
		log.Printf("Kafka enqueue task failed: %v", err)
		return "", err
	}

	return taskID, nil
}
