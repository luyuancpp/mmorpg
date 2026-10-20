package kafkautil

import (
	"errors"
	"strings"
	"testing"

	"github.com/IBM/sarama"
)

func durableTopicDetail(partitions int32, replicas int16) sarama.TopicDetail {
	detail := sarama.TopicDetail{NumPartitions: partitions, ReplicationFactor: replicas,
		ReplicaAssignment: make(map[int32][]int32)}
	for partition := int32(0); partition < partitions; partition++ {
		for broker := int16(0); broker < replicas; broker++ {
			detail.ReplicaAssignment[partition] = append(detail.ReplicaAssignment[partition], int32(broker))
		}
	}
	return detail
}

func durableTopicSpec() TopicSpec {
	return TopicSpec{Name: "match-results", Partitions: 2, ReplicaFactor: 3, RequireDurability: true}
}

func TestDurableTopicsRequireAnExplicitReplicationContract(t *testing.T) {
	for _, replication := range []int16{0, 1, 2} {
		spec := durableTopicSpec()
		spec.ReplicaFactor = replication
		admin := newTopicAdminFake(t, topicMetadataReply{})
		err := ensureTopics(admin, []TopicSpec{spec}, 3, admin.clock, admin.sleep)
		if err == nil || !strings.Contains(err.Error(), "durability") {
			t.Fatalf("strict topic must reject implicit or insufficient RF=%d, got %v", replication, err)
		}
		if len(admin.created) != 0 || admin.alterCalls != 0 {
			t.Fatalf("invalid durable contract must not mutate topics: %+v", admin.created)
		}
	}
}

func TestDurableTopicsRejectUnknownOrIncompleteReplicaMetadata(t *testing.T) {
	for _, name := range []string{"unknown", "RF1", "missing_partition", "short_partition", "duplicate_broker"} {
		t.Run(name, func(t *testing.T) {
			spec := durableTopicSpec()
			detail := durableTopicDetail(spec.Partitions, spec.ReplicaFactor)
			switch name {
			case "unknown":
				detail.ReplicationFactor = 0
			case "RF1":
				detail = durableTopicDetail(spec.Partitions, 1)
			case "missing_partition":
				delete(detail.ReplicaAssignment, 1)
			case "short_partition":
				detail.ReplicaAssignment[1] = []int32{0}
			case "duplicate_broker":
				detail.ReplicaAssignment[1] = []int32{0, 1, 1}
			}
			admin := newTopicAdminFake(t, topicMetadataReply{topics: map[string]sarama.TopicDetail{spec.Name: detail}})
			if err := ensureTopics(admin, []TopicSpec{spec}, 1, admin.clock, admin.sleep); err == nil {
				t.Fatalf("strict topic accepted %s metadata: %+v", name, detail)
			}
			if len(admin.created) != 0 || admin.alterCalls != 0 {
				t.Fatalf("replica violation must not attempt topic changes: created=%+v calls=%d", admin.created, admin.alterCalls)
			}
		})
	}
}

func TestDurableTopicsCreateAndIncrementallyApplyQuorumSettings(t *testing.T) {
	spec := durableTopicSpec()
	spec.RetentionMs = 60000
	admin := newTopicAdminFake(t, topicMetadataReply{}, topicMetadataReply{
		topics: map[string]sarama.TopicDetail{spec.Name: durableTopicDetail(spec.Partitions, 3)},
	})
	if err := ensureTopics(admin, []TopicSpec{spec}, 1, admin.clock, admin.sleep); err != nil {
		t.Fatalf("strict topic creation failed: %v", err)
	}
	created := admin.created[spec.Name]
	if created.ReplicationFactor != 3 {
		t.Fatalf("strict creation must never fall back to single broker: %+v", created)
	}
	for key, want := range map[string]string{"min.insync.replicas": "2", "unclean.leader.election.enable": "false", "retention.ms": "60000"} {
		if value := created.ConfigEntries[key]; value == nil || *value != want {
			t.Errorf("create must set %s=%s, got %+v", key, want, created.ConfigEntries)
		}
		entry := admin.retention[key]
		if entry.Operation != sarama.IncrementalAlterConfigsOperationSet || entry.Value == nil || *entry.Value != want {
			t.Errorf("incremental update must SET %s=%s: %+v", key, want, admin.retention)
		}
	}
	if len(admin.retention) != 3 || admin.alterCalls != 1 {
		t.Fatalf("one narrow incremental update required, got calls=%d entries=%+v", admin.alterCalls, admin.retention)
	}
}

type failingDurableTopicAdmin struct {
	*topicAdminFake
}

func (a failingDurableTopicAdmin) IncrementalAlterConfig(resource sarama.ConfigResourceType, name string,
	entries map[string]sarama.IncrementalAlterConfigsEntry, validateOnly bool) error {
	if err := a.topicAdminFake.IncrementalAlterConfig(resource, name, entries, validateOnly); err != nil {
		return err
	}
	return errors.New("configuration denied")
}

func TestDurableTopicsDoNotStartWhenQuorumConfigCannotBeApplied(t *testing.T) {
	spec := durableTopicSpec()
	_, marker := partitionContractMarker(spec.Name, spec.Partitions)
	admin := failingDurableTopicAdmin{newTopicAdminFake(t, topicMetadataReply{topics: map[string]sarama.TopicDetail{
		spec.Name: durableTopicDetail(spec.Partitions, 3), marker: {NumPartitions: 1, ReplicationFactor: 3},
	}})}
	if err := ensureTopics(admin, []TopicSpec{spec}, 1, admin.clock, admin.sleep); err == nil || !strings.Contains(err.Error(), "configuration denied") {
		t.Fatalf("configuration refusal must prevent strict startup even without retention management: %v", err)
	}
}

func TestDurableTopicsPreserveHigherExplicitMinimumISR(t *testing.T) {
	spec := durableTopicSpec()
	detail := durableTopicDetail(spec.Partitions, 3)
	minimumISR := "3"
	detail.ConfigEntries = map[string]*string{"min.insync.replicas": &minimumISR}
	admin := newTopicAdminFake(t, topicMetadataReply{topics: map[string]sarama.TopicDetail{spec.Name: detail}})
	if err := ensureTopics(admin, []TopicSpec{spec}, 1, admin.clock, admin.sleep); err != nil {
		t.Fatal(err)
	}
	if entry := admin.retention["min.insync.replicas"]; entry.Value == nil || *entry.Value != "3" {
		t.Fatalf("strict startup must not weaken an existing stronger ISR setting: %+v", entry)
	}
}

func TestDurableTopicsLeaveLegacyReplicaAndElectionBehaviorUnmanaged(t *testing.T) {
	spec := durableTopicSpec()
	spec.RequireDurability = false
	spec.ReplicaFactor = 0
	admin := newTopicAdminFake(t, topicMetadataReply{topics: map[string]sarama.TopicDetail{
		spec.Name: {NumPartitions: spec.Partitions, ReplicationFactor: 1},
	}})
	if err := ensureTopics(admin, []TopicSpec{spec}, 1, admin.clock, admin.sleep); err != nil {
		t.Fatal(err)
	}
	if admin.alterCalls != 0 {
		t.Fatalf("legacy startup must not add strict topic configuration: %+v", admin.retention)
	}
}
