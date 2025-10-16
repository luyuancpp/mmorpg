package kafka

import (
	"context"
	"crypto/sha1"
	"fmt"
	"sort"
	"sync"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	db_proto "login/proto/service/go/grpc/db"
)

// KeyOrderedKafkaProducer 基于一致性哈希的Kafka生产者，解决分区扩容问题
type KeyOrderedKafkaProducer struct {
	producer       sarama.AsyncProducer
	topic          string
	partitionCnt   int
	consistentHash *ConsistentHash // 一致性哈希实例
	mu             sync.Mutex
	successCh      chan *sarama.ProducerMessage
	errorCh        chan *sarama.ProducerError
	ctx            context.Context
	cancel         context.CancelFunc
}

// 一致性哈希核心结构
type ConsistentHash struct {
	ring         map[uint32]int32 // 哈希环：hash值 → 分区ID
	sortedHashes []uint32         // 排序后的哈希值，用于快速查找
	replicaCnt   int              // 每个分区的虚拟节点数量（增加分布均匀性）
	mu           sync.RWMutex
}

// 初始化一致性哈希
func NewConsistentHash(replicaCnt int) *ConsistentHash {
	return &ConsistentHash{
		ring:       make(map[uint32]int32),
		replicaCnt: replicaCnt,
	}
}

// 添加分区到哈希环（支持动态添加，如扩容时）
func (c *ConsistentHash) AddPartition(partition int32) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// 为每个分区创建多个虚拟节点，避免数据倾斜
	for i := 0; i < c.replicaCnt; i++ {
		// 虚拟节点ID：partition + i（保证唯一性）
		hashKey := fmt.Sprintf("%d-%d", partition, i)
		hash := c.hash(hashKey)
		c.ring[hash] = partition
		c.sortedHashes = append(c.sortedHashes, hash)
	}

	// 排序哈希值，便于后续二分查找
	sort.Slice(c.sortedHashes, func(i, j int) bool {
		return c.sortedHashes[i] < c.sortedHashes[j]
	})
}

// 计算key对应的分区
func (c *ConsistentHash) GetPartition(key string) (int32, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if len(c.ring) == 0 {
		return 0, false
	}

	hash := c.hash(key)
	// 二分查找哈希环上最近的虚拟节点
	idx := sort.Search(len(c.sortedHashes), func(i int) bool {
		return c.sortedHashes[i] >= hash
	})

	// 若超过最大索引，循环到环的起点
	if idx == len(c.sortedHashes) {
		idx = 0
	}

	return c.ring[c.sortedHashes[idx]], true
}

// 哈希计算（复用原SHA-1算法，保证一致性）
func (c *ConsistentHash) hash(key string) uint32 {
	h := sha1.New()
	h.Write([]byte(key))
	hashBytes := h.Sum(nil)
	return uint32(hashBytes[0])<<24 | uint32(hashBytes[1])<<16 | uint32(hashBytes[2])<<8 | uint32(hashBytes[3])
}

// 初始化生产者（新增一致性哈希初始化）
func NewKeyOrderedKafkaProducer(
	bootstrapServers, topic string,
	partitionCnt int,
) (*KeyOrderedKafkaProducer, error) {
	// ... 省略原有配置代码（与之前相同）...

	// 初始化一致性哈希：每个分区创建10个虚拟节点（平衡性能与均匀性）
	consistent := NewConsistentHash(10)
	for i := int32(0); i < int32(partitionCnt); i++ {
		consistent.AddPartition(i)
	}

	return &KeyOrderedKafkaProducer{
		// ... 省略其他字段赋值 ...
		consistentHash: consistent,
	}, nil
}

// 核心修改：使用一致性哈希计算分区
func (p *KeyOrderedKafkaProducer) getPartitionByKey(key string) int32 {
	// 从一致性哈希环中获取分区
	partition, ok := p.consistentHash.GetPartition(key)
	if !ok {
		// 异常情况：默认使用原取模方式（避免崩溃）
		logx.Errorf("一致性哈希获取分区失败，使用默认取模 | key: %s", key)
		return p.fallbackPartition(key)
	}
	return partition
}

// 降级方案：原取模算法（仅在一致性哈希失败时使用）
func (p *KeyOrderedKafkaProducer) fallbackPartition(key string) int32 {
	h := sha1.New()
	h.Write([]byte(key))
	hashBytes := h.Sum(nil)
	uintHash := uint32(hashBytes[0])<<24 | uint32(hashBytes[1])<<16 | uint32(hashBytes[2])<<8 | uint32(hashBytes[3])
	return int32(uintHash % uint32(p.partitionCnt))
}

// 新增：支持动态添加分区（扩容时调用）
func (p *KeyOrderedKafkaProducer) AddPartitions(newPartitions []int32) {
	p.mu.Lock()
	defer p.mu.Unlock()

	for _, part := range newPartitions {
		p.consistentHash.AddPartition(part)
	}
	p.partitionCnt = len(newPartitions) + p.partitionCnt // 更新总分区数
	logx.Infof("已新增分区，当前总分区数: %d", p.partitionCnt)
}

// SendTask 保持不变（复用修改后的getPartitionByKey）
// SendTask 异步发送任务消息（相同key路由到固定分区，保证有序性）
// 参数说明：
// - task: 待发送的数据库任务Proto结构体（包含TaskID、操作类型、消息体等）
// - key: 业务key（如playerId字符串，用于一致性哈希计算固定分区）
func (p *KeyOrderedKafkaProducer) SendTask(task *db_proto.DBTask, key string) error {
	// 加锁保证并发安全：避免多协程同时操作producer或修改分区计数
	p.mu.Lock()
	defer p.mu.Unlock()

	// 1. 校验生产者状态：避免已关闭/异常的生产者继续发送消息
	if p.producer == nil || p.ctx.Err() != nil {
		return fmt.Errorf("生产者已关闭或上下文已取消，无法发送消息 | TaskID: %s | Key: %s", task.TaskId, key)
	}

	// 2. 校验入参合法性：避免空任务或空key导致后续逻辑异常
	if task == nil {
		return fmt.Errorf("待发送的DBTask为空 | Key: %s", key)
	}
	if task.TaskId == "" {
		return fmt.Errorf("DBTask.TaskId为空，无法追踪任务 | Key: %s", key)
	}
	if key == "" {
		return fmt.Errorf("业务key为空，无法计算固定分区 | TaskID: %s", task.TaskId)
	}

	// 3. 序列化Proto消息：将DBTask转为字节流，用于Kafka消息体
	payload, err := proto.Marshal(task)
	if err != nil {
		return fmt.Errorf("DBTask序列化失败 | TaskID: %s | Key: %s | 原因: %w", task.TaskId, key, err)
	}

	// 4. 计算目标分区：通过一致性哈希获取固定分区（核心逻辑，保证相同key路由到固定分区）
	partition, ok := p.consistentHash.GetPartition(key)
	if !ok {
		// 降级逻辑：若一致性哈希执行失败（如无分区配置），使用原取模算法避免业务中断
		logx.Error("一致性哈希获取分区失败，触发降级逻辑 | TaskID: %s | Key: %s", task.TaskId, key)
		partition = p.fallbackGetPartition(key)
	}
	logx.Debugf("任务计算目标分区完成 | TaskID: %s | Key: %s | 目标分区: %d", task.TaskId, key, partition)

	// 5. 构造Kafka消息体：显式指定分区、时间戳等元数据，避免依赖Sarama默认路由
	msg := &sarama.ProducerMessage{
		Topic:     p.topic,                     // 目标Kafka主题（初始化时指定，固定不变）
		Key:       sarama.StringEncoder(key),   // 业务key（保留用于Kafka审计、追踪和调试）
		Value:     sarama.ByteEncoder(payload), // 消息体（序列化后的DBTask字节流）
		Partition: partition,                   // 显式指定分区（关键：保障相同key的有序性）
		Timestamp: time.Now(),                  // 消息发送时间戳（用于日志分析、时序排序）
	}

	// 6. 异步投递消息到Kafka：通过Sarama的Input通道非阻塞投递，避免阻塞上游业务
	select {
	case p.producer.Input() <- msg:
		// 消息成功投递到Input通道（此时仅代表进入Sarama内部缓存，不代表已发送到Kafka）
		logx.Debugf("消息已投递到生产者Input通道 | TaskID: %s | Key: %s | 目标分区: %d", task.TaskId, key, partition)
		return nil

	case <-p.ctx.Done():
		// 上下文已取消（如调用Close()方法），无法继续投递
		return fmt.Errorf("生产者上下文已取消，消息投递失败 | TaskID: %s | Key: %s | 目标分区: %d", task.TaskId, key, partition)

	case <-time.After(500 * time.Millisecond):
		// 超时保护：避免Input通道满导致长时间阻塞（根据业务并发量调整超时时间）
		return fmt.Errorf("生产者Input通道繁忙，消息投递超时（500ms） | TaskID: %s | Key: %s | 目标分区: %d", task.TaskId, key, partition)
	}
}

// fallbackGetPartition 降级分区计算逻辑：当一致性哈希失败时，使用原SHA-1取模算法
func (p *KeyOrderedKafkaProducer) fallbackGetPartition(key string) int32 {
	// 复用原哈希算法，保证降级逻辑与历史逻辑一致
	h := sha1.New()
	h.Write([]byte(key))
	hashBytes := h.Sum(nil)

	// 将哈希结果转为uint32（取前4字节，避免溢出）
	uintHash := uint32(hashBytes[0])<<24 |
		uint32(hashBytes[1])<<16 |
		uint32(hashBytes[2])<<8 |
		uint32(hashBytes[3])

	// 对当前总分区数取模，得到0~partitionCount-1的分区号
	return int32(uintHash % uint32(p.partitionCnt))
}
