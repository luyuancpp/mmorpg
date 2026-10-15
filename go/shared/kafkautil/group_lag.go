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

// groupOffsetReader 只暴露算 lag 要的两个调用,单测注入 fake,不连真实 Kafka(同 topicAdmin 的做法)。
type groupOffsetReader interface {
	GetOffset(topic string, partition int32, at int64) (int64, error)
	ListConsumerGroupOffsets(group string, topicPartitions map[string][]int32) (*sarama.OffsetFetchResponse, error)
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

	resp, err := r.ListConsumerGroupOffsets(group, map[string][]int32{topic: ids})
	if err != nil {
		return GroupLag{}, fmt.Errorf("kafka lag probe: fetch offsets of group %s on %s: %w", group, topic, err)
	}
	if resp == nil {
		return GroupLag{}, fmt.Errorf("kafka lag probe: fetch offsets of group %s on %s: empty response", group, topic)
	}
	if resp.Err != sarama.ErrNoError {
		return GroupLag{}, fmt.Errorf("kafka lag probe: fetch offsets of group %s on %s: %w", group, topic, resp.Err)
	}
	committed := make([]int64, partitions)
	for i, p := range ids {
		block := resp.GetBlock(topic, p)
		if block == nil {
			return GroupLag{}, fmt.Errorf("kafka lag probe: group %s has no offset block for %s[%d]", group, topic, p)
		}
		if block.Err != sarama.ErrNoError {
			return GroupLag{}, fmt.Errorf("kafka lag probe: group %s offset of %s[%d]: %w", group, topic, p, block.Err)
		}
		committed[i] = block.Offset
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

// NewGroupLagProber 连 broker(会拨号并拉一次元数据,失败即返回 error)。
func NewGroupLagProber(brokers []string) (*GroupLagProber, error) {
	cfg := sarama.NewConfig()
	cfg.Version = sarama.V3_0_0_0 // 与 EnsureTopics 一致
	cfg.Net.DialTimeout = groupLagProbeNetTimeout
	cfg.Net.ReadTimeout = groupLagProbeNetTimeout
	cfg.Net.WriteTimeout = groupLagProbeNetTimeout
	cfg.Metadata.Retry.Max = 1

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
