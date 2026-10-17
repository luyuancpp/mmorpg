package kafkautil

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/IBM/sarama"
)

// 只操作本次随机topic及其计算出的分区契约marker，不消费或修改共享业务topic。
func TestDurableTopicContractWithThreeRealBrokers(t *testing.T) {
	raw := strings.TrimSpace(os.Getenv("KAFKA_DURABILITY_BROKERS"))
	if raw == "" {
		t.Skip("未设置KAFKA_DURABILITY_BROKERS，跳过真实三broker耐久契约；需显式提供逗号分隔的独立测试集群")
	}
	brokers := strings.Split(raw, ",")
	unique := make(map[string]bool)
	for i, broker := range brokers {
		brokers[i] = strings.TrimSpace(broker)
		if brokers[i] == "" || unique[brokers[i]] {
			t.Fatal("KAFKA_DURABILITY_BROKERS必须显式包含至少三个不同地址")
		}
		unique[brokers[i]] = true
	}
	if len(unique) < 3 {
		t.Fatal("KAFKA_DURABILITY_BROKERS不足三个不同地址，不得把单broker验收计为通过")
	}
	var random [12]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	prefix := fmt.Sprintf("mmorpg_durability_it_%d_%s", os.Getpid(), hex.EncodeToString(random[:]))
	strong, weak := prefix+"_rf3", prefix+"_rf1"
	_, strongMarker := partitionContractMarker(strong, 3)
	_, weakMarker := partitionContractMarker(weak, 3)
	owned := []string{strong, weak, strongMarker, weakMarker}
	t.Logf("本次独立topic=%s,%s；仅清理这两者及其分区marker", strong, weak)

	cfg := sarama.NewConfig()
	cfg.Version = sarama.V3_0_0_0
	cfg.Net.DialTimeout = 3 * time.Second
	cfg.Net.ReadTimeout = 5 * time.Second
	cfg.Net.WriteTimeout = 5 * time.Second
	cfg.Admin.Timeout = 5 * time.Second
	admin, err := sarama.NewClusterAdmin(brokers, cfg)
	if err != nil {
		t.Fatalf("连接显式三broker测试集群: %v", err)
	}
	t.Cleanup(func() {
		defer admin.Close()
		for _, name := range owned {
			if err := admin.DeleteTopic(name); err != nil && !errors.Is(err, sarama.ErrUnknownTopicOrPartition) {
				t.Errorf("清理本次topic %s: %v", name, err)
			}
		}
		deadline := time.Now().Add(30 * time.Second)
		for {
			topics, err := admin.ListTopics()
			if err != nil {
				t.Errorf("复核本次topic清理: %v", err)
				return
			}
			var remaining []string
			for _, name := range owned {
				if _, exists := topics[name]; exists {
					remaining = append(remaining, name)
				}
			}
			if len(remaining) == 0 {
				t.Log("本次随机topic与marker清理完成，残留0")
				return
			}
			if !time.Now().Before(deadline) {
				t.Errorf("本次topic删除未完成，需精确清理: %v", remaining)
				return
			}
			time.Sleep(100 * time.Millisecond)
		}
	})

	spec := TopicSpec{Name: strong, Partitions: 3, ReplicaFactor: 3, RequireDurability: true, RetentionMs: 600_000}
	var assignment map[int32][]int32
	if !t.Run("新建三分区逐分区三副本与严格配置", func(t *testing.T) {
		if err := EnsureTopics(brokers, []TopicSpec{spec}); err != nil {
			t.Fatalf("公开EnsureTopics严格创建: %v", err)
		}
		detail := realDurabilityTopicDetail(t, admin, strong, 3)
		if detail.ReplicationFactor != 3 || len(detail.ReplicaAssignment) != 3 {
			t.Fatalf("实际RF/分区元数据不符: %+v", detail)
		}
		assignment = detail.ReplicaAssignment
		for partition := int32(0); partition < 3; partition++ {
			distinct := make(map[int32]bool)
			for _, broker := range assignment[partition] {
				distinct[broker] = true
			}
			if len(assignment[partition]) != 3 || len(distinct) != 3 {
				t.Fatalf("分区%d不是三个不同broker副本: %v", partition, assignment[partition])
			}
			t.Logf("partition=%d replicas=%v", partition, assignment[partition])
		}
		assertRealDurabilityConfig(t, admin, strong, "2", "false")
	}) {
		return
	}
	t.Run("已有RF3幂等初始化保持原分区及副本分配", func(t *testing.T) {
		if err := EnsureTopics(brokers, []TopicSpec{spec}); err != nil {
			t.Fatalf("公开EnsureTopics幂等重入: %v", err)
		}
		after := realDurabilityTopicDetail(t, admin, strong, 3)
		if !reflect.DeepEqual(assignment, after.ReplicaAssignment) {
			t.Fatalf("幂等初始化改变了副本分配: before=%v after=%v", assignment, after.ReplicaAssignment)
		}
		assertRealDurabilityConfig(t, admin, strong, "2", "false")
	})
	t.Run("既有RF1明确拒绝且不自动重分配或改配置", func(t *testing.T) {
		minimumISR, unclean := "1", "true"
		if err := admin.CreateTopic(weak, &sarama.TopicDetail{NumPartitions: 3, ReplicationFactor: 1,
			ConfigEntries: map[string]*string{"min.insync.replicas": &minimumISR, "unclean.leader.election.enable": &unclean}}, false); err != nil {
			t.Fatalf("创建本次RF1负向夹具: %v", err)
		}
		before := realDurabilityTopicDetail(t, admin, weak, 3)
		weakSpec := spec
		weakSpec.Name = weak
		err := EnsureTopics(brokers, []TopicSpec{weakSpec})
		if err == nil || !strings.Contains(err.Error(), "replication contract mismatch") {
			t.Fatalf("严格初始化必须明确拒绝RF1，不能靠网络失败代替: %v", err)
		}
		t.Logf("预期拒绝: %v", err)
		after := realDurabilityTopicDetail(t, admin, weak, 3)
		if after.ReplicationFactor != 1 || !reflect.DeepEqual(before.ReplicaAssignment, after.ReplicaAssignment) {
			t.Fatalf("拒绝路径自动改变了RF1分配: before=%v after=%v", before.ReplicaAssignment, after.ReplicaAssignment)
		}
		assertRealDurabilityConfig(t, admin, weak, "1", "true")
	})
}

func realDurabilityTopicDetail(t *testing.T, admin sarama.ClusterAdmin, name string, partitions int32) sarama.TopicDetail {
	t.Helper()
	topics, err := waitForCreatedTopicMetadata(admin, name, time.Now, time.Sleep)
	if err != nil {
		t.Fatal(err)
	}
	detail := topics[name]
	if detail.NumPartitions != partitions {
		t.Fatalf("topic %s分区数=%d，期望%d", name, detail.NumPartitions, partitions)
	}
	return detail
}

func assertRealDurabilityConfig(t *testing.T, admin sarama.ClusterAdmin, topic, minimumISR, unclean string) {
	t.Helper()
	entries, err := admin.DescribeConfig(sarama.ConfigResource{Type: sarama.TopicResource, Name: topic,
		ConfigNames: []string{"min.insync.replicas", "unclean.leader.election.enable"}})
	if err != nil {
		t.Fatal(err)
	}
	values := make(map[string]string)
	for _, entry := range entries {
		values[entry.Name] = entry.Value
	}
	if values["min.insync.replicas"] != minimumISR || values["unclean.leader.election.enable"] != unclean {
		t.Fatalf("topic %s真实配置不符: %v", topic, values)
	}
	t.Logf("topic=%s min.insync.replicas=%s unclean.leader.election.enable=%s", topic, minimumISR, unclean)
}
