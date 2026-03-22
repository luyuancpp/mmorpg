package kafka

import (
	"context"
	"fmt"
	"login/internal/config"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"login/internal/logic/pkg/consistent"
	db_proto "login/proto/db"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// ProducerMeta holds message metadata for payload recycling.
type ProducerMeta struct {
	producer *KeyOrderedKafkaProducer
	payload  []byte
}

// KeyOrderedKafkaProducer is a consistent-hash-based ordered Kafka producer using SyncProducer for idempotency.
type KeyOrderedKafkaProducer struct {
	producer     sarama.SyncProducer
	client       sarama.Client
	topic        string
	partitionCnt int
	mu           sync.Mutex
	ctx          context.Context
	cancel       context.CancelFunc
	consistent   *consistent.Consistent
	closed       bool
	payloadPool  sync.Pool // reuses []byte to reduce GC

	successCount int64
	errorCount   int64

	// fault-tolerance state
	unavailablePartitions map[int32]time.Time // partition → time marked unavailable
	retryInterval         time.Duration       // retry interval for unavailable partitions
}

// NewKeyOrderedKafkaProducer creates an idempotent producer using SyncProducer.
func NewKeyOrderedKafkaProducer(cfg config.KafkaConfig) (*KeyOrderedKafkaProducer, error) {
	config := sarama.NewConfig()
	config.Version = sarama.V3_6_0_0 // requires Kafka >= 0.11.0.0
	config.Net.DialTimeout = cfg.DialTimeout
	config.Net.ReadTimeout = cfg.ReadTimeout
	config.Net.WriteTimeout = cfg.WriteTimeout
	config.Producer.Return.Successes = true
	config.Producer.Return.Errors = true
	config.Producer.Retry.Max = cfg.RetryMax
	config.Producer.Retry.Backoff = cfg.RetryBackoff
	config.Producer.RequiredAcks = sarama.WaitForAll
	config.ChannelBufferSize = cfg.ChannelBuffer
	config.Producer.Compression = cfg.CompressionType
	config.Producer.Idempotent = cfg.Idempotent
	config.Net.MaxOpenRequests = 1 // required by idempotency

	if cfg.Idempotent {
		config.Producer.Transaction.ID = fmt.Sprintf("kafka-producer-%d", time.Now().UnixNano())
	}

	if err := config.Validate(); err != nil {
		logx.Errorf("invalid Kafka config: %v", err)
		return nil, err
	}

	client, err := sarama.NewClient([]string{cfg.BootstrapServers}, config)
	if err != nil {
		logx.Errorf("failed to create Kafka client: %v", err)
		return nil, fmt.Errorf("failed to create client: %w", err)
	}

	producer, err := sarama.NewSyncProducerFromClient(client)
	if err != nil {
		client.Close()
		logx.Errorf("failed to create Kafka sync producer: %v", err)
		return nil, fmt.Errorf("failed to create producer: %w", err)
	}

	if cfg.Idempotent {
		if !producer.IsTransactional() {
			producer.Close()
			client.Close()
			return nil, fmt.Errorf("idempotent producer requires transaction support (Kafka >= 0.11.0.0)")
		}
		if err := producer.BeginTxn(); err != nil {
			producer.Close()
			client.Close()
			logx.Errorf("failed to begin transaction: %v", err)
			return nil, fmt.Errorf("failed to begin transaction: %w", err)
		}
	}

	consistentHash := consistent.NewConsistent(20)
	for i := int32(0); i < int32(cfg.InitialPartition); i++ {
		consistentHash.AddPartition(i)
	}

	ctx, cancel := context.WithCancel(context.Background())
	payloadPool := sync.Pool{
		New: func() interface{} {
			return make([]byte, 0, 1024)
		},
	}

	unavailableParts := make(map[int32]time.Time)
	retryInterval := 10 * time.Second

	kp := &KeyOrderedKafkaProducer{
		producer:              producer,
		client:                client,
		topic:                 cfg.Topic,
		partitionCnt:          cfg.InitialPartition,
		ctx:                   ctx,
		cancel:                cancel,
		consistent:            consistentHash,
		closed:                false,
		payloadPool:           payloadPool,
		unavailablePartitions: unavailableParts,
		retryInterval:         retryInterval,
	}

	go kp.syncPartitions(cfg.SyncInterval)
	go kp.monitorStats(cfg.StatsInterval)
	go kp.checkUnavailablePartitions()
	go kp.sendTestMessage()

	return kp, nil
}

// SendTasks sends a batch of tasks using SyncProducer.SendMessages.
func (p *KeyOrderedKafkaProducer) SendTasks(ctx context.Context, tasks []*db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("producer closed: batch send failed")
	}
	if len(tasks) == 0 || key == "" {
		return fmt.Errorf("invalid params: taskCount=%d, key=%s", len(tasks), key)
	}

	partition, ok := p.getAvailablePartition(key)
	if !ok {
		return fmt.Errorf("no available partition: key=%s", key)
	}

	var msgs []*sarama.ProducerMessage
	for _, task := range tasks {
		if task.TaskId == "" {
			p.recycleMessages(msgs)
			return fmt.Errorf("empty task ID: %+v", task)
		}

		payload := p.payloadPool.Get().([]byte)
		payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
		if err != nil {
			p.payloadPool.Put(payload)
			p.recycleMessages(msgs)
			return fmt.Errorf("failed to marshal task: %s, %w", task.TaskId, err)
		}

		msgs = append(msgs, &sarama.ProducerMessage{
			Topic:     p.topic,
			Key:       sarama.StringEncoder(key),
			Value:     sarama.ByteEncoder(payload),
			Partition: partition,
			Timestamp: time.Now(),
			Metadata: &ProducerMeta{
				producer: p,
				payload:  payload,
			},
		})
	}

	select {
	case <-ctx.Done():
		p.recycleMessages(msgs)
		p.markPartitionUnavailable(partition)
		return fmt.Errorf("batch send timeout: %v", ctx.Err())
	default:
		if err := p.producer.SendMessages(msgs); err != nil {
			p.recycleMessages(msgs)
			atomic.AddInt64(&p.errorCount, int64(len(msgs)))
			logx.Errorf("batch send failed: partition=%d, err=%v", partition, err)

			if isPartitionUnavailableErr(err) {
				p.markPartitionUnavailable(partition)
			}

			if p.producer.IsTransactional() {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("failed to restart transaction: %v", err)
				}
			}
			return err
		}

		if p.producer.IsTransactional() {
			if err := p.producer.CommitTxn(); err != nil {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("failed to restart transaction after commit failure: %v", err)
				}
				return fmt.Errorf("failed to commit transaction: %w", err)
			}
			if err := p.producer.BeginTxn(); err != nil {
				return fmt.Errorf("failed to begin new transaction: %w", err)
			}
		}

		atomic.AddInt64(&p.successCount, int64(len(msgs)))
		return nil
	}
}

// SendTask sends a single task using SyncProducer.SendMessage.
func (p *KeyOrderedKafkaProducer) SendTask(ctx context.Context, task *db_proto.DBTask, key string) error {
	if p.closed {
		return fmt.Errorf("producer closed: task=%s", task.TaskId)
	}
	if task.TaskId == "" || key == "" {
		return fmt.Errorf("invalid params: taskID=%s, key=%s", task.TaskId, key)
	}

	payload := p.payloadPool.Get().([]byte)
	payload, err := proto.MarshalOptions{}.MarshalAppend(payload[:0], task)
	if err != nil {
		p.payloadPool.Put(payload)
		return fmt.Errorf("failed to marshal task: %s, %w", task.TaskId, err)
	}

	partition, ok := p.getAvailablePartition(key)
	if !ok {
		p.payloadPool.Put(payload)
		return fmt.Errorf("no available partition: key=%s, task=%s", key, task.TaskId)
	}

	msg := &sarama.ProducerMessage{
		Topic:     p.topic,
		Key:       sarama.StringEncoder(key),
		Value:     sarama.ByteEncoder(payload),
		Partition: partition,
		Timestamp: time.Now(),
		Metadata: &ProducerMeta{
			producer: p,
			payload:  payload,
		},
	}

	select {
	case <-ctx.Done():
		p.payloadPool.Put(payload)
		p.markPartitionUnavailable(partition)
		return fmt.Errorf("send timeout: task=%s, %v", task.TaskId, ctx.Err())
	default:
		_, _, err := p.producer.SendMessage(msg)
		if err != nil {
			p.payloadPool.Put(payload)
			atomic.AddInt64(&p.errorCount, 1)
			logx.Errorf("send failed: task=%s, partition=%d, err=%v", task.TaskId, partition, err)

			if isPartitionUnavailableErr(err) {
				p.markPartitionUnavailable(partition)
			}

			if p.producer.IsTransactional() {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("failed to restart transaction: %v", err)
				}
			}
			return err
		}

		if p.producer.IsTransactional() {
			if err := p.producer.CommitTxn(); err != nil {
				_ = p.producer.AbortTxn()
				if err := p.producer.BeginTxn(); err != nil {
					logx.Errorf("failed to restart transaction after commit failure: %v", err)
				}
				return fmt.Errorf("failed to commit transaction: %w", err)
			}
			if err := p.producer.BeginTxn(); err != nil {
				return fmt.Errorf("failed to begin new transaction: %w", err)
			}
		}

		p.payloadPool.Put(payload)
		atomic.AddInt64(&p.successCount, 1)
		logx.Debugf("message sent: task=%s, partition=%d", task.TaskId, partition)
		return nil
	}
}

// AddPartitions adds new partitions to the consistent hash ring.
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		return fmt.Errorf("producer closed: cannot add partitions")
	}
	if len(newPartitions) == 0 {
		return fmt.Errorf("empty partition list")
	}

	added := 0
	existing := make(map[int32]bool)
	for _, part := range p.consistent.GetPartitions() {
		existing[part] = true
	}
	for _, part := range newPartitions {
		if !existing[part] {
			p.consistent.AddPartition(part)
			existing[part] = true
			added++
		}
	}
	p.partitionCnt += added

	logx.Infof("partitions added: topic=%s, previous=%d, added=%d, total=%d",
		p.topic, p.partitionCnt-added, added, p.partitionCnt)
	return nil
}

// SendToTopic sends raw bytes to an arbitrary Kafka topic (not the configured topic).
func (p *KeyOrderedKafkaProducer) SendToTopic(topic string, data []byte) error {
	_, _, err := p.producer.SendMessage(&sarama.ProducerMessage{
		Topic: topic,
		Value: sarama.ByteEncoder(data),
	})
	return err
}

// Close gracefully shuts down the producer with transaction commit.
func (p *KeyOrderedKafkaProducer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		logx.Errorf("producer already closed: topic=%s", p.topic)
		return nil
	}

	p.closed = true
	p.cancel()

	var txnErr error
	if p.producer.IsTransactional() {
		txnErr = p.producer.CommitTxn()
	}

	producerErr := p.producer.Close()
	clientErr := p.client.Close()

	var finalErr error
	if txnErr != nil {
		finalErr = fmt.Errorf("failed to commit transaction: %w", txnErr)
	}
	if producerErr != nil {
		if finalErr == nil {
			finalErr = fmt.Errorf("failed to close producer: %w", producerErr)
		} else {
			finalErr = fmt.Errorf("%v; failed to close producer: %w", finalErr, producerErr)
		}
	}
	if clientErr != nil {
		if finalErr == nil {
			finalErr = fmt.Errorf("failed to close client: %w", clientErr)
		} else {
			finalErr = fmt.Errorf("%v; failed to close client: %w", finalErr, clientErr)
		}
	}

	if finalErr != nil {
		logx.Errorf("failed to close resources: topic=%s, err=%v", p.topic, finalErr)
		return finalErr
	}
	logx.Infof("producer closed: topic=%s", p.topic)
	return nil
}

// syncPartitions periodically syncs actual Kafka partitions.
func (p *KeyOrderedKafkaProducer) syncPartitions(interval time.Duration) {
	if interval <= 0 {
		interval = 30 * time.Second
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			partitions, err := p.client.Partitions(p.topic)
			if err != nil {
				logx.Errorf("failed to get Kafka partitions: topic=%s, err=%v", p.topic, err)
				continue
			}
			if err := p.AddPartitions(partitions); err != nil {
				logx.Errorf("failed to sync partitions: %v", err)
			}
		case <-p.ctx.Done():
			logx.Debug("context done: exiting partition sync")
			return
		}
	}
}

// monitorStats periodically logs message statistics.
func (p *KeyOrderedKafkaProducer) monitorStats(interval time.Duration) {
	if interval <= 0 {
		interval = 5 * time.Second
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			success := atomic.SwapInt64(&p.successCount, 0)
			errs := atomic.SwapInt64(&p.errorCount, 0)
			unavailableCnt := p.getUnavailablePartitionCount()
			logx.Infof("message stats: topic=%s, interval=%v, success=%d, errors=%d, unavailablePartitions=%d",
				p.topic, interval, success, errs, unavailableCnt)
		case <-p.ctx.Done():
			logx.Debug("context done: exiting stats monitor")
			return
		}
	}
}

// sendTestMessage sends a test message to verify the pipeline.
func (p *KeyOrderedKafkaProducer) sendTestMessage() {
	time.Sleep(3 * time.Second)
	testTask := &db_proto.DBTask{
		TaskId: fmt.Sprintf("test-%d", time.Now().Unix()),
	}

	if err := p.SendTask(context.Background(), testTask, "test-key"); err != nil {
		logx.Errorf("failed to send test message: %v", err)
	} else {
		logx.Infof("test message sent: taskID=%s", testTask.TaskId)
	}
}

// recycleMessages returns message payloads to the pool.
func (p *KeyOrderedKafkaProducer) recycleMessages(msgs []*sarama.ProducerMessage) {
	for _, msg := range msgs {
		if meta, ok := msg.Metadata.(*ProducerMeta); ok {
			meta.producer.payloadPool.Put(meta.payload)
		}
	}
}

// getTaskID extracts the task ID from a message value.
func getTaskID(value sarama.Encoder) string {
	if byteVal, ok := value.(sarama.ByteEncoder); ok {
		var task db_proto.DBTask
		err := proto.Unmarshal(byteVal, &task)
		if err == nil {
			return task.TaskId
		}
		return fmt.Sprintf("unmarshal failed: %v", err)
	}
	b, err := value.Encode()
	if err != nil {
		return fmt.Sprintf("encode failed: %v", err)
	}
	var task db_proto.DBTask
	if err := proto.Unmarshal(b, &task); err != nil {
		return fmt.Sprintf("unmarshal failed: %v", err)
	}
	return task.TaskId
}

// markPartitionUnavailable marks a partition as unavailable.
func (p *KeyOrderedKafkaProducer) markPartitionUnavailable(partition int32) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.unavailablePartitions[partition] = time.Now()
	logx.Errorf("partition marked unavailable: topic=%s, partition=%d, retryInterval=%v",
		p.topic, partition, p.retryInterval)
}

// getAvailablePartition returns an available partition for the given key.
func (p *KeyOrderedKafkaProducer) getAvailablePartition(key string) (int32, bool) {
	p.mu.Lock()
	defer p.mu.Unlock()

	partition, ok := p.consistent.GetPartition(key)
	if !ok {
		return 0, false
	}

	if _, isUnavailable := p.unavailablePartitions[partition]; !isUnavailable {
		return partition, true
	}

	allPartitions := p.consistent.GetPartitions()
	for _, part := range allPartitions {
		if _, isUnavailable := p.unavailablePartitions[part]; !isUnavailable {
			logx.Errorf("original partition unavailable, remapped: key=%s, from=%d, to=%d",
				key, partition, part)
			return part, true
		}
	}

	return 0, false
}

// checkUnavailablePartitions periodically re-enables recovered partitions.
func (p *KeyOrderedKafkaProducer) checkUnavailablePartitions() {
	ticker := time.NewTicker(p.retryInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			p.mu.Lock()
			now := time.Now()
			for part, unavailableTime := range p.unavailablePartitions {
				if now.Sub(unavailableTime) >= p.retryInterval {
					delete(p.unavailablePartitions, part)
					logx.Infof("partition recovered: topic=%s, partition=%d, downtime=%v",
						p.topic, part, now.Sub(unavailableTime))
				}
			}
			p.mu.Unlock()
		case <-p.ctx.Done():
			logx.Debug("context done: exiting unavailable partition check")
			return
		}
	}
}

// isPartitionUnavailableErr checks if the error indicates a partition is unavailable.
func isPartitionUnavailableErr(err error) bool {
	if prodErr, ok := err.(*sarama.ProducerError); ok {
		errMsg := prodErr.Error()
		if strings.Contains(errMsg, "partition") {
			switch prodErr.Err {
			case sarama.ErrUnknownTopicOrPartition:
			case sarama.ErrLeaderNotAvailable:
			case sarama.ErrOffsetNotAvailable:
			case sarama.ErrReplicaNotAvailable:
				return true
			}
			return strings.Contains(errMsg, "unavailable")
		}
	}

	errMsg := err.Error()
	return strings.Contains(errMsg, "partition") && strings.Contains(errMsg, "unavailable")
}

// getUnavailablePartitionCount returns the number of unavailable partitions.
func (p *KeyOrderedKafkaProducer) getUnavailablePartitionCount() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return len(p.unavailablePartitions)
}
