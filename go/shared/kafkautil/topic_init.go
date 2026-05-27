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
			// Topic exists — update retention AND grow partitions if our spec
			// asks for more than the broker currently has.
			//
			// History: this branch used to update only retention.ms. The first
			// boot ever auto-created topics with the broker default
			// num.partitions=1 (we were on sarama auto-create back then), so
			// every subsequent run found the topic and skipped partition
			// allocation forever. db_task_zone_N silently stayed at 1
			// partition no matter what the spec said, which serialized every
			// db worker onto a single partition and capped EnterGame
			// throughput at one db SELECT at a time. Stress run 2026-05-24
			// surfaced this when 5000-robot smoke runs piled 22k tasks on
			// partition 0 and timed out (see
			// docs/design/stress-3zone-2026-05-23-postmortem.md §G.4).
			//
			// Note: Kafka does NOT rebalance existing keys when partitions
			// grow. New keys hash across the larger space, but messages
			// already in the log stay where they were. That's fine for
			// db_task (work queue, no per-key ordering survives a partition
			// add anyway), but be careful applying this to topics where
			// per-key ordering matters across boots — in those cases the
			// safer fix is to rename the topic.
			existingDetail := existing[spec.Name]
			if spec.Partitions > 0 && existingDetail.NumPartitions < spec.Partitions {
				if err := admin.CreatePartitions(spec.Name, spec.Partitions, nil, false); err != nil {
					logx.Errorf("kafka grow partitions %s %d->%d: %v",
						spec.Name, existingDetail.NumPartitions, spec.Partitions, err)
				} else {
					logx.Infof("kafka topic %s partitions grown: %d -> %d",
						spec.Name, existingDetail.NumPartitions, spec.Partitions)
				}
			}

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
