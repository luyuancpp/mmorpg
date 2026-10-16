package kafka

import (
	"login/internal/config"
	"testing"

	"github.com/IBM/sarama"

	"shared/kafkacmd"
	"shared/kafkautil"
)

// 分区器的作用域是这轮改造里最容易搞砸的一点:命令 topic 必须尊重显式分区;
// db_task topic 必须落在分区契约(kafkautil.DBTaskPartition,与 C++ librdkafka consistent_random 同一个函数)上;
// 其它 topic 保持原来的哈希分区行为。sarama 是按 topic 各建一个 Partitioner 的,所以这里直接盯构造函数的选型。
func TestCommandAwarePartitionerScope(t *testing.T) {
	cmdTopic := kafkacmd.GateCommandTopic()
	if _, ok := NewCommandAwarePartitioner(cmdTopic).(*commandPartitioner); !ok {
		t.Fatalf("command topic %q must get the explicit-partition partitioner", cmdTopic)
	}
	if _, ok := NewCommandAwarePartitioner(kafkacmd.SceneCommandTopic()).(*commandPartitioner); !ok {
		t.Fatalf("scene command topic must get the explicit-partition partitioner")
	}

	// db_task topic:落点必须等于分区契约。
	for _, topic := range []string{"db_task_zone_1", "db_task_zone_1_g2"} {
		assertDBTaskContractPartitioner(t, topic, NewCommandAwarePartitioner(topic))
	}

	// 其它非命令 topic:必须拿到与 sarama.NewConfig() 默认值同一个分区器类型。
	// "db_task" 刻意留在这一组:全仓不存在叫 db_task 的 topic(历史名是 db_task_topic,现行命名见
	// config.DbTaskTopicForGeneration),把前缀放宽到 "db_task" 会误伤无关 topic。
	for _, topic := range []string{"db_task", "gate-5", "scene-77", "player_snapshot_topic_g1"} {
		got := NewCommandAwarePartitioner(topic)
		if _, isCmd := got.(*commandPartitioner); isCmd {
			t.Errorf("topic %q must NOT be treated as a control-plane command topic", topic)
		}
		want := sarama.NewHashPartitioner(topic)
		if _, ok := got.(sarama.Partitioner); !ok {
			t.Errorf("topic %q: partitioner does not satisfy sarama.Partitioner", topic)
		}
		// 同一个 key 必须落到与默认哈希分区器完全相同的分区上。
		msg := &sarama.ProducerMessage{Topic: topic, Key: sarama.StringEncoder("player-1001")}
		gotPart, err := got.Partition(msg, 8)
		if err != nil {
			t.Fatalf("topic %q: %v", topic, err)
		}
		wantPart, err := want.Partition(msg, 8)
		if err != nil {
			t.Fatalf("topic %q reference partitioner: %v", topic, err)
		}
		if gotPart != wantPart {
			t.Errorf("topic %q: partition %d, want %d (non-command topics must keep the previous behaviour)",
				topic, gotPart, wantPart)
		}
	}
}

// assertDBTaskContractPartitioner 断言 got 不是命令分区器,且对玩家 key 的落点等于 kafkautil.DBTaskPartition。
func assertDBTaskContractPartitioner(t *testing.T, topic string, got sarama.Partitioner) {
	t.Helper()
	if _, isCmd := got.(*commandPartitioner); isCmd {
		t.Errorf("db_task topic %q must NOT be treated as a control-plane command topic", topic)
		return
	}
	for _, key := range []string{"1001", "7311864093319258112"} {
		for _, n := range []int32{8, 10} {
			msg := &sarama.ProducerMessage{Topic: topic, Key: sarama.StringEncoder(key)}
			gotPart, err := got.Partition(msg, n)
			if err != nil {
				t.Fatalf("topic %q key %q n=%d: %v", topic, key, n, err)
			}
			wantPart, err := kafkautil.DBTaskPartition([]byte(key), n)
			if err != nil {
				t.Fatalf("DBTaskPartition(%q, %d): %v", key, n, err)
			}
			if gotPart != wantPart {
				t.Errorf("topic %q key %q n=%d: partition %d, want contract partition %d", topic, key, n, gotPart, wantPart)
			}
		}
	}
}

// kafkautil.DBTaskTopicPrefix 是 topic 命名规则之外的第三份写法,这里用真实的命名函数把它们绑在一起:
// 命名规则一改、前缀没跟着改,plainProducer 的拒绝与契约分区器就会对新 topic 静默失效。
func TestDBTaskTopicNamingMatchesContractPrefix(t *testing.T) {
	for _, gen := range []uint32{0, 1, 2} {
		topic := config.DbTaskTopicForGeneration(1, gen)
		if !kafkautil.IsDBTaskTopic(topic) {
			t.Errorf("config.DbTaskTopicForGeneration(1, %d) = %q is not recognised by kafkautil.IsDBTaskTopic", gen, topic)
			continue
		}
		assertDBTaskContractPartitioner(t, topic, NewCommandAwarePartitioner(topic))
	}
}

// 命令 topic 上,分区号必须原样透传 —— 这正是 sarama 默认哈希分区器会毁掉的东西。
func TestCommandPartitionerHonoursExplicitPartition(t *testing.T) {
	p := &commandPartitioner{topic: kafkacmd.GateCommandTopic()}
	for _, want := range []int32{0, 1, 44, 255} {
		got, err := p.Partition(&sarama.ProducerMessage{Partition: want}, 256)
		if err != nil {
			t.Fatalf("partition %d: %v", want, err)
		}
		if got != want {
			t.Errorf("partition = %d, want %d", got, want)
		}
	}
}

// broker 的分区数与契约不符时必须报错,不许降级到别的分区。
// 最常见的成因是 topic 没预建、被抢先发消息的生产者 auto-create 成了 1 分区;
// 那时候把命令投到 0 号分区等于静默丢弃(目标节点根本没 assign 这个分区)。
func TestCommandPartitionerRejectsOutOfRangePartition(t *testing.T) {
	p := &commandPartitioner{topic: kafkacmd.GateCommandTopic()}
	if _, err := p.Partition(&sarama.ProducerMessage{Partition: 44}, 1); err == nil {
		t.Fatal("partition 44 on a 1-partition broker topic must fail closed, not silently fall back")
	}
	if _, err := p.Partition(&sarama.ProducerMessage{Partition: -1}, 256); err == nil {
		t.Fatal("negative partition must fail closed")
	}
}

// SendToTopic 是"随便发个 topic"的通用口子,不带分区。命令 topic 从这里出去会
// 落到 0 号分区,所以必须被挡住(否则新增调用方很容易无意中绕过寻址收口)。
func TestSendToTopicRefusesCommandTopics(t *testing.T) {
	p := &KeyOrderedKafkaProducer{}
	if err := p.SendToTopic(kafkacmd.GateCommandTopic(), []byte("x"), "k"); err == nil {
		t.Fatal("SendToTopic must refuse control-plane command topics")
	}
	if err := p.SendToTopic(kafkacmd.SceneCommandTopic(), []byte("x"), "k"); err == nil {
		t.Fatal("SendToTopic must refuse control-plane command topics")
	}
}

// 负分区在到达 sarama 之前就要被拦下:sarama 那边只会给一个 ErrInvalidPartition,
// 排障时看不出是寻址算错了还是别的。
func TestSendToTopicPartitionRejectsNegativePartition(t *testing.T) {
	p := &KeyOrderedKafkaProducer{}
	if err := p.SendToTopicPartition(kafkacmd.GateCommandTopic(), -1, []byte("x"), "k"); err == nil {
		t.Fatal("negative partition must be rejected before reaching the broker")
	}
}

// db_task 只能经 SendTask / SendTasks 发(那里校验 key 契约)。plainProducer 为 nil,
// 所以能返回 error 而不 panic 也证明了拒绝发生在触碰生产者之前。
func TestSendToTopicRefusesDBTaskTopics(t *testing.T) {
	p := &KeyOrderedKafkaProducer{}
	for _, topic := range []string{"db_task_zone_1", "db_task_zone_1_g2"} {
		if err := p.SendToTopic(topic, []byte("x"), "1001"); err == nil {
			t.Fatalf("SendToTopic must refuse db_task topic %q", topic)
		}
	}
}

// 显式分区在 db_task topic 上不成立(plainProducer 的契约分区器会按 key 覆盖它),同样拒绝。
func TestSendToTopicPartitionRefusesDBTaskTopics(t *testing.T) {
	p := &KeyOrderedKafkaProducer{}
	for _, topic := range []string{"db_task_zone_1", "db_task_zone_1_g2"} {
		if err := p.SendToTopicPartition(topic, 3, []byte("x"), "1001"); err == nil {
			t.Fatalf("SendToTopicPartition must refuse db_task topic %q", topic)
		}
	}
}
