package saveoutbox

import (
	"context"
	"errors"
	"time"

	kafkago "github.com/segmentio/kafka-go"
	"shared/kafkautil"
)

// dbTaskBalancer 仅适配Writer接口，分区知识仍由shared/kafkautil独占。
type dbTaskBalancer struct{}

func (dbTaskBalancer) Balance(msg kafkago.Message, partitions ...int) int {
	p, err := kafkautil.DBTaskPartition(msg.Key, int32(len(partitions)))
	if err != nil {
		// Publisher在入队前拒绝空key；Writer只在取到非空分区元数据后调用。
		return -1 // 非法分区让broker拒绝，不能误投0号分区。
	}
	return partitions[int(p)]
}

type kafkaPublisher struct {
	writer    *kafkago.Writer
	transport *kafkago.Transport
	topic     string
}

func newKafkaPublisher(brokers []string, topic string) (*kafkaPublisher, error) {
	if len(brokers) == 0 || topic == "" {
		return nil, errors.New("save outbox Kafka配置缺失")
	}
	transport := &kafkago.Transport{DialTimeout: time.Second, IdleTimeout: 30 * time.Second,
		MetadataTopics: []string{topic}, ClientID: "db-save-outbox"}
	return &kafkaPublisher{topic: topic, transport: transport, writer: &kafkago.Writer{
		Addr: kafkago.TCP(brokers...), Balancer: dbTaskBalancer{}, RequiredAcks: kafkago.RequireAll,
		MaxAttempts: 1, BatchSize: 1, BatchTimeout: time.Millisecond,
		ReadTimeout: time.Second, WriteTimeout: time.Second,
		Transport: transport, AllowAutoTopicCreation: false,
	}}, nil
}

func (p *kafkaPublisher) Publish(ctx context.Context, topic, key string, payload []byte) error {
	if topic != p.topic || key == "" || len(payload) == 0 {
		return errors.New("save outbox Kafka投递超出当前topic或缺身份")
	}
	// WriteMessages取消后仍可能晚到；payload不再修改，原意图等SQL双ACK后才清理。
	return p.writer.WriteMessages(ctx, kafkago.Message{Topic: topic, Key: []byte(key), Value: payload})
}

func (p *kafkaPublisher) close() error {
	err := p.writer.Close() // 单次尝试、网络超时有界；先由服务取消Run，阻止新入队。
	p.transport.CloseIdleConnections()
	return err
}
