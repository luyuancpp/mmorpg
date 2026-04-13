package kafkautil

import (
	"fmt"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
)

// TopicSpec describes a topic to ensure exists with specific retention.
type TopicSpec struct {
	Name          string
	Partitions    int32
	RetentionMs   int64 // -1 = use broker default
	ReplicaFactor int16 // 0 = use default (1)
}

// EnsureTopics creates topics if they don't exist and applies retention config.
// For existing topics, it updates the retention.ms config to match the spec.
func EnsureTopics(brokers []string, specs []TopicSpec) error {
	cfg := sarama.NewConfig()
	cfg.Version = sarama.V3_0_0_0

	admin, err := sarama.NewClusterAdmin(brokers, cfg)
	if err != nil {
		return fmt.Errorf("kafka admin connect: %w", err)
	}
	defer admin.Close()

	existing, err := admin.ListTopics()
	if err != nil {
		return fmt.Errorf("kafka list topics: %w", err)
	}

	for _, spec := range specs {
		replica := spec.ReplicaFactor
		if replica <= 0 {
			replica = 1
		}
		partitions := spec.Partitions
		if partitions <= 0 {
			partitions = 1
		}

		retentionStr := fmt.Sprintf("%d", spec.RetentionMs)

		if _, exists := existing[spec.Name]; !exists {
			// Create topic
			topicDetail := &sarama.TopicDetail{
				NumPartitions:     partitions,
				ReplicationFactor: replica,
			}
			if spec.RetentionMs > 0 {
				topicDetail.ConfigEntries = map[string]*string{
					"retention.ms": &retentionStr,
				}
			}
			if err := admin.CreateTopic(spec.Name, topicDetail, false); err != nil {
				logx.Errorf("kafka create topic %s: %v", spec.Name, err)
				return fmt.Errorf("kafka create topic %s: %w", spec.Name, err)
			}
			logx.Infof("kafka topic created: %s (partitions=%d, retention=%dms)", spec.Name, partitions, spec.RetentionMs)
		} else {
			// Topic exists — update retention if specified
			if spec.RetentionMs > 0 {
				entries := map[string]*string{
					"retention.ms": &retentionStr,
				}
				if err := admin.AlterConfig(sarama.TopicResource, spec.Name, entries, false); err != nil {
					logx.Errorf("kafka alter topic %s retention: %v", spec.Name, err)
				} else {
					logx.Infof("kafka topic %s retention updated to %dms", spec.Name, spec.RetentionMs)
				}
			}
		}
	}

	return nil
}
