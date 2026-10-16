package kafkautil

import (
	"errors"
	"fmt"
	"time"

	"github.com/IBM/sarama"
)

// GroupLag 是一个消费组在一个 topic 上的积压(Lag / RetentionGap 是全部分区求和,MaxPartitionLag 是单分区最大值;
// 调用方不按分区出指标,基数恒定)。
type GroupLag struct {
	// Lag = Σ max(0, end − max(committed, start)):end = 下一条要写的 offset(OffsetNewest),start = 日志起点(OffsetOldest)。
	// 没提交过的分区按「从 start 开始」算(消费者都是 StartOffset=FirstOffset)。
	Lag int64
	// MaxPartitionLag 是上式里单个分区那一项的最大值(没提交过的分区同样计入)。
	// broker 的 retention.bytes 按**分区**执行:积压集中在少数分区时(持有它的实例卡住、毒丸行卡住所在分区),
	// 合计 Lag 还远没到线,那个分区已经开始被删 —— 「离删除线还有多远」只能拿它比,不能拿合计比。
	MaxPartitionLag int64
	// RetentionGap = Σ max(0, start − committed),只算提交过的分区:保留期(retention.ms 或 retention.bytes)
	// 已经删掉、而这个组还没提交到的消息条数 —— 通常意味着这些数据已经丢了。也含已被消费者读入、尚未提交的消息
	// 所在段被删的情形(消费者预取在前、处理完才提交):此时未必丢,消费者照常处理并提交越过 start 后它自行归零,
	// 提交之前停机 / 重建时其中没处理完的才真丢。丢没丢、丢了多少以 [kafka-go] 日志为准。
	RetentionGap int64
	// Uncommitted 是还没提交过 offset 的分区数(新组 / 新一代 topic 刚建)。
	Uncommitted int
}

// CommittedOffsetsReader 是读一个组已提交位点要的唯一调用;sarama.ClusterAdmin 满足它。
// 单独成接口是为了让 lag 探针(本文件)与 go/db 的消费组交接闸(go/db/internal/kafka/group_handoff.go)
// 共用同一份 fail-closed 的位点读取,不各写一份错误语义。
type CommittedOffsetsReader interface {
	ListConsumerGroupOffsets(group string, topicPartitions map[string][]int32) (*sarama.OffsetFetchResponse, error)
}

// groupOffsetReader 只暴露算 lag 要的两个调用,单测注入 fake,不连真实 Kafka(同 topicAdmin 的做法)。
type groupOffsetReader interface {
	CommittedOffsetsReader
	GetOffset(topic string, partition int32, at int64) (int64, error)
}

// CommittedOffsets 读 group 在 topic 的 partitions 上的已提交位点,返回 分区 → offset;-1 原样返回,表示该分区没提交过。
//
// fail-closed:调用本身出错、响应为 nil、响应顶层 Err 非 0(OffsetFetch v2+)、某分区 block 缺失或 block.Err 非 0,
// 都返回 error,不把读不到当成「没有位点」。sarama.KError 用 %w 包住,调用方可以
// errors.Is(err, sarama.ErrGroupIDNotFound) 区分「组不存在」(交接闸按全部 -1 处理)。
// 错误文本不带前缀,由调用方补(computeGroupLag 补 "kafka lag probe: ",与抽取之前逐字相同)。
func CommittedOffsets(r CommittedOffsetsReader, group, topic string, partitions []int32) (map[int32]int64, error) {
	resp, err := r.ListConsumerGroupOffsets(group, map[string][]int32{topic: partitions})
	if err != nil {
		return nil, fmt.Errorf("fetch offsets of group %s on %s: %w", group, topic, err)
	}
	if resp == nil {
		return nil, fmt.Errorf("fetch offsets of group %s on %s: empty response", group, topic)
	}
	if resp.Err != sarama.ErrNoError {
		return nil, fmt.Errorf("fetch offsets of group %s on %s: %w", group, topic, resp.Err)
	}
	out := make(map[int32]int64, len(partitions))
	for _, p := range partitions {
		block := resp.GetBlock(topic, p)
		if block == nil {
			return nil, fmt.Errorf("group %s has no offset block for %s[%d]", group, topic, p)
		}
		if block.Err != sarama.ErrNoError {
			return nil, fmt.Errorf("group %s offset of %s[%d]: %w", group, topic, p, block.Err)
		}
		out[p] = block.Offset
	}
	return out, nil
}

// computeGroupLag 算一个组在一个 topic 上的 GroupLag。
//
// **读取顺序是正确性要求**:先读组的已提交 offset,再逐分区读 end / start。反过来(先 end 后 committed)时,
// 消费者在两次读之间提交一次就会出现 committed > end,繁忙 topic 上探针会周期性误报;先读 committed,
// end 单调不减,committed <= end 恒成立(除非 topic 真被删了重建)。代价:start 也是后读的,保留期恰在两次读之间推进、
// 而消费者其实已越过它时,会算出一个**瞬时**的 RetentionGap > 0 —— 对应告警 DataServiceAuditRetentionGap 自 2026-10-15
// 起不带 for(max_over_time[15m]、for: 0m),这种正值也会呼叫,按丢数据处置后以 `[kafka-go]` 日志确认条数(可能为 0),
// 见 deploy/k8s/data-service-audit-alerts.yaml 与 docs/ops/audit-topic-generation-bump-runbook.md §7。
//
// 任何一处读失败、响应顶层 Err(OffsetFetch v2+)或某分区 block 缺失 / 带错,都返回 error;
// committed > end(topic 被删了重建过)时该分区 lag 记 0,并返回 error 让调用方看见,不静默当 0。
func computeGroupLag(r groupOffsetReader, group, topic string, partitions int32) (GroupLag, error) {
	if partitions <= 0 {
		return GroupLag{}, fmt.Errorf("kafka lag probe: topic %s has invalid partition count %d", topic, partitions)
	}
	ids := make([]int32, partitions)
	for i := range ids {
		ids[i] = int32(i)
	}

	// 先读 committed(见上面的读取顺序说明);错误语义与文本由 CommittedOffsets 统一给出,这里只补前缀。
	committedByPartition, err := CommittedOffsets(r, group, topic, ids)
	if err != nil {
		return GroupLag{}, fmt.Errorf("kafka lag probe: %w", err)
	}
	committed := make([]int64, partitions)
	for i, p := range ids {
		committed[i] = committedByPartition[p]
	}

	var (
		out      GroupLag
		mismatch error
	)
	for i, p := range ids {
		end, err := r.GetOffset(topic, p, sarama.OffsetNewest)
		if err != nil {
			return GroupLag{}, fmt.Errorf("kafka lag probe: end offset of %s[%d]: %w", topic, p, err)
		}
		start, err := r.GetOffset(topic, p, sarama.OffsetOldest)
		if err != nil {
			return GroupLag{}, fmt.Errorf("kafka lag probe: start offset of %s[%d]: %w", topic, p, err)
		}
		c := committed[i]
		if c < 0 {
			// 没提交过(Kafka 回 -1):消费者从日志起点开始读。
			out.Uncommitted++
			if end > start {
				out.Lag += end - start
				out.MaxPartitionLag = max(out.MaxPartitionLag, end-start)
			}
			continue
		}
		if c > end {
			// 先读 committed 后读 end,正常情况下不可能:topic 被删了重建过(或换了集群)。lag 记 0,报出来。
			mismatch = errors.Join(mismatch, fmt.Errorf("kafka lag probe: group %s committed offset %d of %s[%d] is beyond the log end %d (topic recreated?)",
				group, c, topic, p, end))
			continue
		}
		if c < start {
			out.RetentionGap += start - c
		}
		if from := max(c, start); end > from {
			out.Lag += end - from
			out.MaxPartitionLag = max(out.MaxPartitionLag, end-from)
		}
	}
	return out, mismatch
}

// groupLagProbeNetTimeout:探针的拨号 / 读 / 写超时。sarama 调用不收 ctx,默认 30s 的超时会让一轮探测在 broker
// 半死时挂半分钟以上、并拖慢关停;探针只是观测,宁可快速失败、下一轮再来。
const groupLagProbeNetTimeout = 5 * time.Second

// GroupLagProber 持有一对长连接的 sarama Client + ClusterAdmin(admin 由 client 派生,admin.Close 会一并关 client),
// 自己实现 groupOffsetReader:GetOffset 转 client、ListConsumerGroupOffsets 转 admin。
// 连接坏了不就地自愈:调用方(data_service 的 lag 探针)探失败就 Close、下一轮重建。
type GroupLagProber struct {
	client sarama.Client
	admin  sarama.ClusterAdmin
}

// newGroupLagProberConfig 是探针连 broker 用的 sarama 配置;单独成函数是为了让单测钉住各项取值(不连 Kafka)。
//
// Metadata.AllowAutoTopicCreation = false(负责人 2026-10-15 按「最标准」定:只读的监控探针不得对被监控系统产生副作用)。
// 下列 sarama 行为按本机模块缓存里的 v1.43.1 源码核对:
//   - 默认值是 true(config.go:524)。探针里唯一按 topic 拉元数据的是 GetOffset:元数据缓存里没有该 topic / 分区时先按 topic
//     刷新(client.go:496-510),出错后再按 topic 刷新一次重试(:563-577);按 topic 的 Metadata 请求带上这个标志(:1022-1031,
//     拉全量元数据时 sarama 自己强制为 false,:1025-1026;V3_0_0_0 下是 Metadata v10,标志在 v>3 时编码,metadata_request.go:95-96)。
//     默认 true 时,broker 开着 auto.create.topics.enable、num.partitions=1(本仓 compose / K8s 均如此),探针打到尚未预建或已被删的
//     topic 就可能把它建成 1 分区,data_service 的分区契约随即永久失败。
//   - ListConsumerGroupOffsets 只向 coordinator 发 OffsetFetch,不拉 topic 元数据(admin.go:1009-1018),与这个标志无关。
//   - 关掉之后 topic 不存在时:元数据响应里该 topic 的错误若是 UNKNOWN_TOPIC_OR_PARTITION,sarama 记为 ErrUnknownTopicOrPartition
//     并按 Metadata.Retry.Max 重试(client.go:1130-1133、:1048-1051),仍失败即由 GetOffset 返回 (-1, ErrUnknownTopicOrPartition)。
//     computeGroupLag 把它包成 "kafka lag probe: end offset of <topic>[0]: ..." 返回 error(读 committed 那一步对不存在的 topic
//     回 -1 还是分区错,属 broker 语义、未核;两种情形最终都是 error),调用方记 lag_probe_total{result="error"}、关连接下一轮重建,
//     持续 15 分钟即 DataServiceAuditLagProbeBlind —— 不会当成 0 lag。broker 收到 false 时对不存在的 topic 回哪个错误码属 Kafka
//     语义,本注未实测。
func newGroupLagProberConfig() *sarama.Config {
	cfg := sarama.NewConfig()
	cfg.Version = sarama.V3_0_0_0 // 与 EnsureTopics 一致
	cfg.Net.DialTimeout = groupLagProbeNetTimeout
	cfg.Net.ReadTimeout = groupLagProbeNetTimeout
	cfg.Net.WriteTimeout = groupLagProbeNetTimeout
	cfg.Metadata.Retry.Max = 1
	cfg.Metadata.AllowAutoTopicCreation = false
	return cfg
}

// NewGroupLagProber 连 broker(会拨号并拉一次元数据,失败即返回 error)。配置见 newGroupLagProberConfig。
func NewGroupLagProber(brokers []string) (*GroupLagProber, error) {
	cfg := newGroupLagProberConfig()

	client, err := sarama.NewClient(brokers, cfg)
	if err != nil {
		return nil, fmt.Errorf("kafka lag probe connect: %w", err)
	}
	admin, err := sarama.NewClusterAdminFromClient(client)
	if err != nil {
		_ = client.Close()
		return nil, fmt.Errorf("kafka lag probe admin: %w", err)
	}
	return &GroupLagProber{client: client, admin: admin}, nil
}

// GetOffset 实现 groupOffsetReader(转 sarama Client)。
func (p *GroupLagProber) GetOffset(topic string, partition int32, at int64) (int64, error) {
	return p.client.GetOffset(topic, partition, at)
}

// ListConsumerGroupOffsets 实现 groupOffsetReader(转 sarama ClusterAdmin)。
func (p *GroupLagProber) ListConsumerGroupOffsets(group string, topicPartitions map[string][]int32) (*sarama.OffsetFetchResponse, error) {
	return p.admin.ListConsumerGroupOffsets(group, topicPartitions)
}

// Probe 算一个组在一个 topic 上的积压,见 computeGroupLag。
func (p *GroupLagProber) Probe(group, topic string, partitions int32) (GroupLag, error) {
	return computeGroupLag(p, group, topic, partitions)
}

// Close 关 admin(连同它底下的 client)。
func (p *GroupLagProber) Close() error {
	return p.admin.Close()
}
