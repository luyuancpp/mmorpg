package kafkautil

// db_task 分区函数契约在 Go 侧的唯一实现(docs/design/db-task-kafka-partition-contract.md「分区函数契约」)。
//
// 契约:
//   - key = 玩家 id 的无符号十进制 ASCII,无前导零。Go 侧是 strconv.FormatUint(id, 10),
//     C++ 侧是 std::to_string(uint64_t)(cpp/libs/services/scene/player/constants/player.h 的 DbTaskKafkaKey),
//     两者逐字节相同。
//   - partition = CRC-32/IEEE(key) mod N,按 uint32 无符号取模。这就是 librdkafka 默认分区器
//     consistent_random 对非空 key 的算法 rd_crc32(key) % partition_cnt;
//     sarama.NewConsistentCRCHashPartitioner 是它的 Go 对应物(hashUnsigned 分支)。
//   - 空 key 不在契约内:librdkafka 对空 key 随机分区,sarama 对非 nil 的空 key 落 0 号分区,两边不一致。
//     所以两侧生产者都拒绝空 key,DBTaskPartition 对空 key 也报错。
//
// 为什么必须只有一处实现:go/db 的读后写屏障与写游标(applied cursor)都按分区成立。
// C++ scene 的写与 login 的读必须用同一个函数,同一玩家的读写才落在同一分区、按 offset 串行。
// 换函数 = 换 TopicGeneration(离线排空):在线换函数会让同一玩家的写在新旧生产者之间换分区,
// go/db 比较游标里记录的分区时判 orderingIncomparable,写被隔离进死信。

import (
	"errors"
	"fmt"
	"hash/crc32"
	"strconv"
	"strings"

	"github.com/IBM/sarama"
)

// DBTaskTopicPrefix 是所有 db_task topic 的前缀。命名规则本身在
// login / db 的 config.DbTaskTopicForGeneration(db_task_zone_{zone}[_g{gen}]);
// 这里只用来识别 topic 类别,login 的 command_partitioner_test 用真实命名函数把两者绑在一起。
const DBTaskTopicPrefix = "db_task_zone_"

var (
	// ErrEmptyDBTaskKey 表示 key 为空:空 key 不在分区契约内(librdkafka 与 sarama 的处理不一致)。
	ErrEmptyDBTaskKey = errors.New("kafkautil: empty db_task key is outside the partition contract")
	// ErrInvalidDBTaskPartitionCount 表示分区数不是正数。
	ErrInvalidDBTaskPartitionCount = errors.New("kafkautil: db_task partition count must be positive")
)

// 编译期断言:NewDBTaskPartitioner 可以直接赋给 sarama.Config.Producer.Partitioner。
var _ sarama.PartitionerConstructor = NewDBTaskPartitioner

// IsDBTaskTopic 判断 topic 是否属于 db_task 类别(含各代 _g{gen} 后缀)。
func IsDBTaskTopic(topic string) bool {
	return strings.HasPrefix(topic, DBTaskTopicPrefix)
}

// DBTaskKey 返回玩家 id 的分区 key:无符号十进制 ASCII,与 C++ std::to_string(uint64_t) 逐字节相同。
func DBTaskKey(playerID uint64) string {
	return strconv.FormatUint(playerID, 10)
}

// AppendDBTaskKey 把 DBTaskKey(playerID) 追加到 dst 后返回,供热路径(go/db 消费端观测)免分配使用。
func AppendDBTaskKey(dst []byte, playerID uint64) []byte {
	return strconv.AppendUint(dst, playerID, 10)
}

// DBTaskPartition 按契约算 key 应落的分区:CRC-32/IEEE(key) mod partitionCount,按 uint32 无符号取模。
//
// partitionCount <= 0 返回 (-1, ErrInvalidDBTaskPartitionCount);key 为空返回 (-1, ErrEmptyDBTaskKey)。
// 纯函数,无副作用,可并发调用。
func DBTaskPartition(key []byte, partitionCount int32) (int32, error) {
	if partitionCount <= 0 {
		return -1, fmt.Errorf("%w: %d", ErrInvalidDBTaskPartitionCount, partitionCount)
	}
	if len(key) == 0 {
		return -1, ErrEmptyDBTaskKey
	}
	return int32(crc32.ChecksumIEEE(key) % uint32(partitionCount)), nil
}

// NewDBTaskPartitioner 是 db_task 生产者唯一允许使用的 sarama 分区器构造函数,
// 结果与 DBTaskPartition 逐个相同(dbtask_partition_test.go 机械断言)。
//
// 写成函数而不是可变的包变量:包变量可以被任何人在运行期改掉,契约就不再只有一处来源。
func NewDBTaskPartitioner(topic string) sarama.Partitioner {
	return sarama.NewConsistentCRCHashPartitioner(topic)
}
