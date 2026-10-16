package kafka

// group_handoff.go —— go/db 存盘消费组按 zone 拆分时的不停服交接闸(docs/design/db-consumer-group-per-zone.md)。
//
// 背景:K8s 上全部 zone 的 go/db 原来共用消费组 db_rpc_consumer_group(下称 L),现在逐 zone 切到
// db_rpc_consumer_group_z<ZoneId>(下称 Z),也允许单 zone 反向切回 L。直接改组名不行:新组没有已提交位点,
// sarama 按 Offsets.Initial=OffsetOldest 从保留期起点全量重放;游标缺键(FLUSHDB 之后)时旧写会被放行。
// 本文件只在配置了 Kafka.GroupHandoffFrom(交接形状 handoff(X←Y))时生效;最终形状(只写 GroupID)不建任何对象,
// 行为与引入之前完全相同。
//
// 职责与不变量:
//  1. 入组前判定一次(await):本进程第一次 Consume 之前,在消费循环里轮询判定,被闸住的一方根本不入组、
//     不持有分配(修 B1 的互等)。放行之后置 released,之后的再平衡不再看对方(只闸一次,收窄对 L coordinator
//     的跨 zone 依赖,B3);只有守望让出时才复位、重新过闸。
//  2. 判定只凭正面证据放行(evaluate):本组 / 对方查询失败、对方状态未知、对方正在再平衡、对方持有本 topic 分区,
//     一律等待。R7:两组在同一分区都有位点且对方更大时拒绝放行(本组位点陈旧,续消费会大段重放),
//     处置是删除陈旧一方在本 topic 上的位点(tools/scripts/kafka_offset_reset.ps1 -FinalizeHandoff / -RetireGroup)。
//  3. Setup 播种(seed):本组没有位点、对方有的分区,MarkOffset(对方已提交位点原值,**不加 1**)——已提交位点
//     本来就是「下一条要读的 offset」;本组已有位点的分区不动;两组都没有的分区从 Offsets.Initial 起步。
//     进程内缓存「已落定(settled)」分区,之后的 Setup 不再查询它们;刚播种的分区不进缓存,下次 Setup 再确认一次
//     本组位点(MarkOffset 之后、1s 自动提交之前崩溃时,提交会丢)。
//  4. 守望(watch / watchOnce)只凭正面证据让出:对方 Stable、对方成员持有本 topic 的分区、对方在本 topic 上的
//     已提交位点在相邻两次观测之间前进过,三者同时成立才取消本 session 并复位 released。被闸住的对方不入组、
//     不持有分配,所以不会触发让出。
//  5. inspector(admin 连接)任何一次查询出错就丢弃,下次查询用工厂重建:sarama 的 ClusterAdmin 只读缓存的
//     coordinator,遇到 NOT_COORDINATOR 不刷新;新 client 会重新 FindCoordinator(修订稿 R2)。
//
// 不是互斥:守望周期 5s 加一轮再平衡内两组可能同时消费同一分区,兜底仍靠按 topic 拼键的游标、排序锁与
// owner_epoch(设计文档 §3)。本文件不改游标、排序锁、重试键,也不改 Offsets.Initial。

import (
	"context"
	db_config "db/internal/config"
	"db/internal/metrics"
	"errors"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"

	"shared/kafkautil"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	// handoffPollInterval 是交接闸前的轮询周期;gate_total 因此 ≤1/s/Pod。
	handoffPollInterval = 1 * time.Second
	// handoffWatchInterval 是放行之后守望对方组的周期,也是两组同时消费的重叠窗口上限的主要部分(设计文档 §3)。
	handoffWatchInterval = 5 * time.Second
	// handoffAdminTimeout:sarama 的 admin 调用不收 ctx,靠 Net.Dial/Read/WriteTimeout 限时(同 kafkautil.GroupLagProber)。
	handoffAdminTimeout = 5 * time.Second
	// handoffWaitLogInterval:等待原因不变时最多每 10s 打一行日志。
	handoffWaitLogInterval = 10 * time.Second
	// consumerClientIDMaxLen 是 sarama ClientID 的长度上限(本仓库约定,Kafka 本身不强制)。
	consumerClientIDMaxLen = 200
)

// consumerKafkaVersion 是消费者与交接闸 inspector 共用的协议版本。sarama.KafkaVersion 是结构体,只能用 var。
var consumerKafkaVersion = sarama.V3_5_0_0

// Kafka 消费组状态字符串(DescribeGroups 的 group_state)。
const (
	groupStateEmpty               = "Empty"
	groupStateDead                = "Dead"
	groupStateStable              = "Stable"
	groupStatePreparingRebalance  = "PreparingRebalance"
	groupStateCompletingRebalance = "CompletingRebalance"
)

// groupInspector 是交接闸读 Kafka 事实的接口;单测注入 fake,不连真实 Kafka。
// 方法不收 ctx:sarama admin 调用靠网络超时限时(handoffAdminTimeout)。
type groupInspector interface {
	// DescribeGroup 返回 group 的状态,以及其成员当前分配里属于 topic 的分区。
	DescribeGroup(group, topic string) (peerGroupView, error)
	// CommittedOffsets 返回 group 在 topic 的 partitions 上的已提交位点;-1 表示没有位点。组不存在时全部为 -1。
	CommittedOffsets(group, topic string, partitions []int32) (map[int32]int64, error)
	Close() error
}

// peerGroupView 是对方组的一次观测。Held 只在 State==Stable 时可信(再平衡中成员分配为空或过时)。
type peerGroupView struct {
	State string
	Held  map[int32]struct{}
}

// saramaGroupInspector 持有一条 sarama Client 派生的 ClusterAdmin(admin.Close 会一并关 client)。
// 连接坏了不就地自愈:groupHandoff.withInspector 在任何查询出错后 Close 它,下次查询重建。
type saramaGroupInspector struct {
	admin sarama.ClusterAdmin
}

// newSaramaGroupInspector 连 broker(会拨号并拉一次元数据,失败即返回 error)。口径与 kafkautil.GroupLagProber 相同:
// 网络超时 5s、Metadata.Retry.Max=1;版本用 consumerKafkaVersion;ClientID 在消费者的 ClientID 后加 -handoff,
// 便于在 broker 侧区分。
func newSaramaGroupInspector(brokers []string, version sarama.KafkaVersion, clientID string) (groupInspector, error) {
	cfg := sarama.NewConfig()
	cfg.Version = version
	cfg.ClientID = clientID + "-handoff"
	cfg.Net.DialTimeout = handoffAdminTimeout
	cfg.Net.ReadTimeout = handoffAdminTimeout
	cfg.Net.WriteTimeout = handoffAdminTimeout
	cfg.Metadata.Retry.Max = 1

	client, err := sarama.NewClient(brokers, cfg)
	if err != nil {
		return nil, fmt.Errorf("group handoff inspector connect: %w", err)
	}
	admin, err := sarama.NewClusterAdminFromClient(client)
	if err != nil {
		_ = client.Close()
		return nil, fmt.Errorf("group handoff inspector admin: %w", err)
	}
	return &saramaGroupInspector{admin: admin}, nil
}

// DescribeGroup 实现 groupInspector。GROUP_ID_NOT_FOUND 映射为 Dead;其它非零错误返回 error。
func (s *saramaGroupInspector) DescribeGroup(group, topic string) (peerGroupView, error) {
	descs, err := s.admin.DescribeConsumerGroups([]string{group})
	if err != nil {
		return peerGroupView{}, fmt.Errorf("describe group %s: %w", group, err)
	}
	if len(descs) != 1 || descs[0] == nil {
		return peerGroupView{}, fmt.Errorf("describe group %s: got %d descriptions, want 1", group, len(descs))
	}
	return peerViewFromDescription(descs[0], topic)
}

// CommittedOffsets 实现 groupInspector,委托 kafkautil.CommittedOffsets(与 lag 探针同一份 fail-closed 语义);
// 组不存在(ErrGroupIDNotFound)按全部 -1 处理。
func (s *saramaGroupInspector) CommittedOffsets(group, topic string, partitions []int32) (map[int32]int64, error) {
	offsets, err := kafkautil.CommittedOffsets(s.admin, group, topic, partitions)
	if errors.Is(err, sarama.ErrGroupIDNotFound) {
		return noOffsets(partitions), nil
	}
	return offsets, err
}

// Close 关 admin(连同它底下的 client)。
func (s *saramaGroupInspector) Close() error {
	return s.admin.Close()
}

// peerViewFromDescription 把一条 DescribeGroups 结果转成 peerGroupView。desc.Err==ErrGroupIDNotFound → Dead、不持有;
// 其它非零 Err → error;成员分配解码失败 → error(fail-closed,不当成「不持有」)。
func peerViewFromDescription(desc *sarama.GroupDescription, topic string) (peerGroupView, error) {
	switch desc.Err {
	case sarama.ErrNoError:
	case sarama.ErrGroupIDNotFound:
		return peerGroupView{State: groupStateDead, Held: map[int32]struct{}{}}, nil
	default:
		return peerGroupView{}, fmt.Errorf("describe group %s: %w", desc.GroupId, desc.Err)
	}
	held, err := heldPartitions(desc, topic)
	if err != nil {
		return peerGroupView{}, err
	}
	return peerGroupView{State: desc.State, Held: held}, nil
}

// heldPartitions 收集组内各成员当前分配里属于 topic 的分区。没有分配(nil)的成员跳过;分配字节解码出错返回 error。
func heldPartitions(desc *sarama.GroupDescription, topic string) (map[int32]struct{}, error) {
	held := make(map[int32]struct{})
	if desc == nil {
		return held, nil
	}
	for memberID, member := range desc.Members {
		if member == nil {
			continue
		}
		assignment, err := member.GetMemberAssignment()
		if err != nil {
			return nil, fmt.Errorf("decode assignment of member %s in group %s: %w", memberID, desc.GroupId, err)
		}
		if assignment == nil {
			continue
		}
		for _, p := range assignment.Topics[topic] {
			held[p] = struct{}{}
		}
	}
	return held, nil
}

// gateDecision 是交接闸的一次判定。own 是这次读到的本组位点,放行时用来初始化 settled。
type gateDecision struct {
	pass    bool
	outcome string
	detail  string
	own     map[int32]int64
}

// groupHandoff 是一个进程内、一个(本组, 对方组, topic)的交接闸。
type groupHandoff struct {
	group      string
	peer       string
	topic      string
	partitions []int32

	// newInspector 建 admin 连接;insp 懒建,任何查询出错后丢弃(见 withInspector)。inspMu 串行化所有 admin 调用。
	newInspector func() (groupInspector, error)
	inspMu       sync.Mutex
	insp         groupInspector

	now   func() time.Time
	sleep func(ctx context.Context, d time.Duration) bool

	// mu 保护 released、sessionYield、settled。
	mu           sync.Mutex
	released     bool
	sessionYield context.CancelFunc
	settled      map[int32]struct{}

	// watchPrev 是守望上一次观测到的对方已提交位点;只由守望 goroutine(watch / watchOnce)读写。
	watchPrev map[int32]int64
	// watchFailing 只由守望 goroutine 读写:查询从成功转为失败时打一行日志,持续失败不刷屏。
	watchFailing bool
}

// newGroupHandoff 建交接闸。group / peer 不能为空、不能相等;partitionCnt 必须为正(分区取 [0, partitionCnt))。
// 只保存 inspector 工厂,第一次查询时才连 broker。
func newGroupHandoff(newInspector func() (groupInspector, error), group, peer, topic string, partitionCnt int32) (*groupHandoff, error) {
	switch {
	case newInspector == nil:
		return nil, errors.New("group handoff needs an inspector factory")
	case group == "" || peer == "":
		return nil, fmt.Errorf("group handoff needs both groups, got group=%q peer=%q", group, peer)
	case group == peer:
		return nil, fmt.Errorf("group handoff peer must differ from the consumer group %q", group)
	case topic == "":
		return nil, errors.New("group handoff needs a topic")
	case partitionCnt <= 0:
		return nil, fmt.Errorf("group handoff needs a positive partition count, got %d", partitionCnt)
	}
	partitions := make([]int32, partitionCnt)
	for i := range partitions {
		partitions[i] = int32(i)
	}
	return &groupHandoff{
		group:        group,
		peer:         peer,
		topic:        topic,
		partitions:   partitions,
		newInspector: newInspector,
		now:          time.Now,
		sleep:        sleepWithContext,
	}, nil
}

// newGroupHandoffFromConfig 按配置建交接闸。GroupHandoffFrom 为空(最终形状)时返回 (nil, nil),且不调用工厂:
// 不在交接中的进程不连 admin、不依赖对方组(T8)。调用方须先 ValidateConsumerGroup,并已拼好 kcfg.Topic。
func newGroupHandoffFromConfig(kcfg db_config.KafkaConfig, group string, newInspector func() (groupInspector, error)) (*groupHandoff, error) {
	if kcfg.GroupHandoffFrom == "" {
		return nil, nil
	}
	return newGroupHandoff(newInspector, group, kcfg.GroupHandoffFrom, kcfg.Topic, kcfg.PartitionCnt)
}

// sleepWithContext 睡 d,ctx 先结束时返回 false。
func sleepWithContext(ctx context.Context, d time.Duration) bool {
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return false
	case <-timer.C:
		return true
	}
}

// withInspector 在 inspMu 内用当前 inspector 执行 fn:inspector 为 nil 时先用工厂建,建不出来返回错误;
// fn 出错时 Close 掉并置 nil,下次调用重建(新 client 重新 FindCoordinator,覆盖 coordinator 迁移、broker 重启)。
func (h *groupHandoff) withInspector(fn func(groupInspector) error) error {
	h.inspMu.Lock()
	defer h.inspMu.Unlock()
	if h.insp == nil {
		insp, err := h.newInspector()
		if err != nil {
			return fmt.Errorf("group handoff inspector: %w", err)
		}
		if insp == nil {
			return errors.New("group handoff inspector: factory returned nil")
		}
		h.insp = insp
	}
	if err := fn(h.insp); err != nil {
		if cerr := h.insp.Close(); cerr != nil {
			logx.Errorf("group handoff inspector close after a failed query failed: group=%s peer=%s err=%v", h.group, h.peer, cerr)
		}
		h.insp = nil
		return err
	}
	return nil
}

// evaluate 做一次闸前判定:在一次 withInspector 里依次查本组位点、对方位点、对方 DescribeGroup。
func (h *groupHandoff) evaluate() gateDecision {
	var (
		own, peer       map[int32]int64
		view            peerGroupView
		ownErr, peerErr error
	)
	err := h.withInspector(func(insp groupInspector) error {
		var qerr error
		if own, qerr = insp.CommittedOffsets(h.group, h.topic, h.partitions); qerr != nil {
			ownErr = fmt.Errorf("committed offsets of group %s: %w", h.group, qerr)
			return qerr
		}
		if peer, qerr = insp.CommittedOffsets(h.peer, h.topic, h.partitions); qerr != nil {
			peerErr = fmt.Errorf("committed offsets of peer group %s: %w", h.peer, qerr)
			return qerr
		}
		if view, qerr = insp.DescribeGroup(h.peer, h.topic); qerr != nil {
			peerErr = fmt.Errorf("describe peer group %s: %w", h.peer, qerr)
			return qerr
		}
		return nil
	})
	if err != nil && ownErr == nil && peerErr == nil {
		// fn 没跑到:inspector 建不出来,连本组位点都读不到。
		ownErr = err
	}
	return decideGate(h.partitions, own, peer, view, ownErr, peerErr)
}

// decideGate 是闸前判定的纯函数,判定顺序即优先级(修订稿 §4.3):
//
//	① 本组查询失败(含 inspector 建不出来)                      → 等待 wait_own_unavailable
//	② 对方位点或 DescribeGroup 查询失败(不管本组是否完整)       → 等待 wait_peer_unavailable
//	③ 对方 Stable 且持有本 topic 分区                           → 等待 wait_peer_holding
//	④ R7:某分区两组都有位点且对方更大                          → 等待 wait_own_behind_peer
//	⑤ 对方在全部分区都没有位点、且不持有分区(不管什么状态)     → 放行 no_peer_offsets
//	⑥ 对方 Empty / Dead,或 Stable 且不持有                      → 放行 peer_released
//	⑦ 对方 PreparingRebalance / CompletingRebalance             → 等待 wait_peer_rebalancing
//	⑧ 空串或其它未知状态                                       → 等待 wait_peer_state_unknown
func decideGate(partitions []int32, own, peer map[int32]int64, view peerGroupView, ownErr, peerErr error) gateDecision {
	if ownErr != nil {
		return gateDecision{outcome: metrics.HandoffGateWaitOwnUnavailable, detail: ownErr.Error()}
	}
	if peerErr != nil {
		return gateDecision{outcome: metrics.HandoffGateWaitPeerUnavailable, detail: peerErr.Error()}
	}
	if view.State == groupStateStable && len(view.Held) > 0 {
		return gateDecision{outcome: metrics.HandoffGateWaitPeerHolding, detail: fmt.Sprintf("held=%v", sortedPartitions(view.Held))}
	}
	for _, p := range partitions {
		o, q := offsetOf(own, p), offsetOf(peer, p)
		if o >= 0 && q >= 0 && q > o {
			return gateDecision{outcome: metrics.HandoffGateWaitOwnBehindPeer,
				detail: fmt.Sprintf("partition=%d own=%d peer=%d", p, o, q)}
		}
	}
	peerHasOffsets := false
	for _, p := range partitions {
		if offsetOf(peer, p) >= 0 {
			peerHasOffsets = true
			break
		}
	}
	if !peerHasOffsets && len(view.Held) == 0 {
		return gateDecision{pass: true, outcome: metrics.HandoffGateNoPeerOffsets, detail: "state=" + view.State, own: own}
	}
	switch view.State {
	case groupStateEmpty, groupStateDead, groupStateStable:
		// Stable 走到这里说明 Held 为空(③ 已排除持有):对方成员只在消费别的 topic(legacy 组被多个 zone 共用)。
		return gateDecision{pass: true, outcome: metrics.HandoffGatePeerReleased, detail: "state=" + view.State, own: own}
	case groupStatePreparingRebalance, groupStateCompletingRebalance:
		return gateDecision{outcome: metrics.HandoffGateWaitPeerRebalancing, detail: "state=" + view.State}
	default:
		return gateDecision{outcome: metrics.HandoffGateWaitPeerStateUnknown, detail: fmt.Sprintf("state=%q", view.State)}
	}
}

// await 在本进程入组前阻塞,直到交接闸放行或 ctx 结束。已放行时立即返回 nil(之后的再平衡不再看对方)。
// 只由消费循环 goroutine 调用。
func (h *groupHandoff) await(ctx context.Context) error {
	h.mu.Lock()
	released := h.released
	h.mu.Unlock()
	if released {
		return nil
	}

	start := h.now()
	var (
		lastOutcome string
		lastLog     time.Time
	)
	for {
		if err := ctx.Err(); err != nil {
			metrics.SetHandoffWaiting(0)
			return err
		}
		d := h.evaluate()
		metrics.CountHandoffGate(d.outcome)
		now := h.now()
		waited := now.Sub(start)
		if d.pass {
			h.mu.Lock()
			h.released = true
			h.settled = settledFrom(d.own)
			h.mu.Unlock()
			metrics.SetHandoffWaiting(0)
			metrics.ObserveHandoffWait(waited)
			logx.Infof("group handoff released: group=%s peer=%s topic=%s reason=%s waited=%s detail=%s",
				h.group, h.peer, h.topic, d.outcome, waited, d.detail)
			return nil
		}
		metrics.SetHandoffWaiting(waited)
		if d.outcome != lastOutcome || now.Sub(lastLog) >= handoffWaitLogInterval {
			h.logWait(d, waited)
			lastOutcome = d.outcome
			lastLog = now
		}
		if !h.sleep(ctx, handoffPollInterval) {
			metrics.SetHandoffWaiting(0)
			return ctx.Err()
		}
	}
}

// logWait 打一行等待日志:需要人处置的原因(R7、查询失败、对方状态未知)用 Errorf,其余用 Infof。
func (h *groupHandoff) logWait(d gateDecision, waited time.Duration) {
	switch d.outcome {
	case metrics.HandoffGateWaitOwnBehindPeer:
		logx.Errorf("group handoff refused: group=%s has committed offsets behind peer=%s on topic=%s (%s); resuming would replay already-applied writes from the stale offsets. "+
			"Delete the stale group's offsets on this topic: if this group is stale (an earlier handoff was never finalized), retire it with "+
			"tools/scripts/kafka_offset_reset.ps1 -Group %s -RetireGroup %s -FinalizeHandoff; if the peer is stale (this group was rewound on purpose, e.g. a rollback), "+
			"delete the peer's offsets with the reset mode plus -RetireGroup %s, as k8s_zone_rollback.ps1 Step 5 does. "+
			"Do not edit ordering cursors by hand. See docs/design/db-consumer-group-per-zone.md. waited=%s",
			h.group, h.peer, h.topic, d.detail, h.peer, h.group, h.peer, waited)
	case metrics.HandoffGateWaitOwnUnavailable, metrics.HandoffGateWaitPeerUnavailable, metrics.HandoffGateWaitPeerStateUnknown:
		logx.Errorf("group handoff waiting: group=%s peer=%s topic=%s reason=%s detail=%s waited=%s",
			h.group, h.peer, h.topic, d.outcome, d.detail, waited)
	default:
		logx.Infof("group handoff waiting: group=%s peer=%s topic=%s reason=%s detail=%s waited=%s",
			h.group, h.peer, h.topic, d.outcome, d.detail, waited)
	}
}

// seed 在 Setup 里给本 session 分到的、尚未落定的分区播种。查询失败返回 error(Setup 随之失败,成员 1s 后重新入组,
// 用重建的 inspector 再查);本方法不提交,MarkOffset 由 sarama 的 offset manager 按 CommitInterval 提交。
func (h *groupHandoff) seed(session sarama.ConsumerGroupSession, claimed []int32) error {
	need := h.unsettled(claimed)
	if len(need) == 0 {
		return nil
	}
	reached := false
	err := h.withInspector(func(insp groupInspector) error {
		reached = true
		own, qerr := insp.CommittedOffsets(h.group, h.topic, need)
		if qerr != nil {
			countSeed(metrics.HandoffSeedAborted, len(need))
			return fmt.Errorf("committed offsets of group %s: %w", h.group, qerr)
		}
		rest := make([]int32, 0, len(need))
		for _, p := range need {
			if o := offsetOf(own, p); o >= 0 {
				h.markSettled(p)
				metrics.CountHandoffSeed(metrics.HandoffSeedOwnPresent)
				logx.Debugf("group handoff partition already has own offset: group=%s topic=%s partition=%d offset=%d", h.group, h.topic, p, o)
				continue
			}
			rest = append(rest, p)
		}
		if len(rest) == 0 {
			return nil
		}
		peer, qerr := insp.CommittedOffsets(h.peer, h.topic, rest)
		if qerr != nil {
			countSeed(metrics.HandoffSeedAborted, len(rest))
			return fmt.Errorf("committed offsets of peer group %s: %w", h.peer, qerr)
		}
		for _, p := range rest {
			if q := offsetOf(peer, p); q >= 0 {
				// 原值,不加 1:已提交位点就是「下一条要读的 offset」。不进 settled:提交可能丢,下次 Setup 再确认。
				session.MarkOffset(h.topic, p, q, "")
				metrics.CountHandoffSeed(metrics.HandoffSeedSeeded)
				logx.Infof("group handoff seeded partition: group=%s topic=%s partition=%d offset=%d peer=%s", h.group, h.topic, p, q, h.peer)
				continue
			}
			h.markSettled(p)
			metrics.CountHandoffSeed(metrics.HandoffSeedNoOffsets)
			// logx 没有 WARN 级别;这不是故障(从未写入的分区本来就没有位点),用 Infof 并在文本里标 WARN。
			logx.Infof("WARN group handoff partition has no committed offset in either group; it starts from Offsets.Initial (oldest), which is normal for a partition that was never written: group=%s peer=%s topic=%s partition=%d",
				h.group, h.peer, h.topic, p)
		}
		return nil
	})
	if err != nil && !reached {
		// inspector 建不出来,一个分区都没查到。
		countSeed(metrics.HandoffSeedAborted, len(need))
	}
	return err
}

// watch 每 handoffWatchInterval 调一次 watchOnce,ctx 结束后关闭 done。
func (h *groupHandoff) watch(ctx context.Context, done chan<- struct{}) {
	defer close(done)
	ticker := time.NewTicker(handoffWatchInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			h.watchOnce()
		}
	}
}

// watchOnce 观测一次对方组。未放行时清空基线直接返回;任一查询出错直接返回(只凭正面证据让出)。
// 对方 Stable、持有本 topic 分区、且相邻两次观测之间某分区已提交位点前进过(prev<0 且 cur≥0 也算),才让出。
func (h *groupHandoff) watchOnce() {
	h.mu.Lock()
	released := h.released
	h.mu.Unlock()
	if !released {
		h.watchPrev = nil
		return
	}

	var (
		view peerGroupView
		cur  map[int32]int64
	)
	err := h.withInspector(func(insp groupInspector) error {
		var qerr error
		if view, qerr = insp.DescribeGroup(h.peer, h.topic); qerr != nil {
			return qerr
		}
		cur, qerr = insp.CommittedOffsets(h.peer, h.topic, h.partitions)
		return qerr
	})
	if err != nil {
		if !h.watchFailing {
			logx.Infof("group handoff watcher cannot observe peer (will not yield without positive evidence): group=%s peer=%s topic=%s err=%v",
				h.group, h.peer, h.topic, err)
		}
		h.watchFailing = true
		return
	}
	h.watchFailing = false

	prev := h.watchPrev
	h.watchPrev = cur
	if prev == nil {
		return
	}
	if view.State != groupStateStable || len(view.Held) == 0 {
		return
	}
	advanced := false
	for _, p := range h.partitions {
		// offsetOf 对没有位点的分区返回 -1,所以「prev<0 且 cur≥0」也落在 cur > prev 里。
		if offsetOf(cur, p) > offsetOf(prev, p) {
			advanced = true
			break
		}
	}
	if !advanced {
		return
	}
	metrics.CountHandoffConflict()
	logx.Errorf("group handoff conflict: peer %s is consuming topic %s (held=%v, committed advanced); yielding this session",
		h.peer, h.topic, sortedPartitions(view.Held))
	h.yield()
}

// yield 让出:复位 released(下一次 Consume 之前重新过闸)、清空 settled、取消当前 session 的派发。
func (h *groupHandoff) yield() {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.released = false
	h.settled = nil
	if h.sessionYield != nil {
		h.sessionYield()
	}
}

// setSessionYield 登记当前 session 的让出函数,返回登记时是否仍处于放行状态。返回 false 说明守望在 Setup 与登记之间
// 已经让出,调用方须立即取消本 session,堵住这个竞态。
func (h *groupHandoff) setSessionYield(cancel context.CancelFunc) bool {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.sessionYield = cancel
	return h.released
}

// clearSessionYield 在 session Cleanup 时摘掉让出函数。
func (h *groupHandoff) clearSessionYield() {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.sessionYield = nil
}

// Close 关掉当前 inspector(若有)。
func (h *groupHandoff) Close() error {
	h.inspMu.Lock()
	defer h.inspMu.Unlock()
	if h.insp == nil {
		return nil
	}
	err := h.insp.Close()
	h.insp = nil
	return err
}

// unsettled 返回 claimed 中不在 settled 里的分区(在 mu 内取快照)。
func (h *groupHandoff) unsettled(claimed []int32) []int32 {
	h.mu.Lock()
	defer h.mu.Unlock()
	out := make([]int32, 0, len(claimed))
	for _, p := range claimed {
		if _, ok := h.settled[p]; !ok {
			out = append(out, p)
		}
	}
	return out
}

func (h *groupHandoff) markSettled(p int32) {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.settled == nil {
		h.settled = make(map[int32]struct{})
	}
	h.settled[p] = struct{}{}
}

// settledFrom 返回位点 ≥0 的分区集合(闸放行时本组已有位点的分区直接算落定)。
func settledFrom(own map[int32]int64) map[int32]struct{} {
	out := make(map[int32]struct{}, len(own))
	for p, o := range own {
		if o >= 0 {
			out[p] = struct{}{}
		}
	}
	return out
}

func countSeed(result string, n int) {
	for i := 0; i < n; i++ {
		metrics.CountHandoffSeed(result)
	}
}

// offsetOf 取分区位点;不在 map 里按 -1(没有位点)。
func offsetOf(m map[int32]int64, p int32) int64 {
	if o, ok := m[p]; ok {
		return o
	}
	return -1
}

func noOffsets(partitions []int32) map[int32]int64 {
	out := make(map[int32]int64, len(partitions))
	for _, p := range partitions {
		out[p] = -1
	}
	return out
}

func sortedPartitions(set map[int32]struct{}) []int32 {
	out := make([]int32, 0, len(set))
	for p := range set {
		out = append(out, p)
	}
	sort.Slice(out, func(i, j int) bool { return out[i] < out[j] })
	return out
}

// consumerClientID 返回 sarama ClientID:db-z<zone>-<host>。host 里 [A-Za-z0-9._-] 以外的字符替换成 '-',
// 去掉首尾的 '-',总长截到 consumerClientIDMaxLen;host 为空(或全是非法字符)时只返回 db-z<zone>。
// 用途:broker 侧(DescribeGroups 的 client_id、配额、日志)能认出是哪个 zone 的哪个 Pod。
func consumerClientID(zoneID uint32, hostname string) string {
	base := fmt.Sprintf("db-z%d", zoneID)
	host := strings.Map(func(r rune) rune {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9', r == '.', r == '_', r == '-':
			return r
		default:
			return '-'
		}
	}, hostname)
	host = strings.Trim(host, "-")
	if host == "" {
		return base
	}
	id := base + "-" + host
	if len(id) > consumerClientIDMaxLen {
		// 替换之后全是 ASCII,按字节截断安全。
		id = strings.TrimRight(id[:consumerClientIDMaxLen], "-")
	}
	return id
}

// rebalanceGapTracker 量「上一个 session 结束 → 下一个 session Setup 完成」的间隔(含交接闸等待)。
// 只有 Setup 成功过的 session 才在 Cleanup 时调 sessionEnded:Setup 失败后的重新入组算进同一个空窗。
type rebalanceGapTracker struct {
	mu      sync.Mutex
	lastEnd time.Time
}

// sessionEnded 记一个 session 的结束时刻。
func (t *rebalanceGapTracker) sessionEnded(at time.Time) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.lastEnd = at
}

// sessionStarted 返回距上一次 sessionEnded 的间隔;没有过 sessionEnded(进程第一个 session)时返回 false。
// 每个结束时刻只被消费一次。
func (t *rebalanceGapTracker) sessionStarted(at time.Time) (time.Duration, bool) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.lastEnd.IsZero() {
		return 0, false
	}
	d := at.Sub(t.lastEnd)
	t.lastEnd = time.Time{}
	if d < 0 {
		d = 0
	}
	return d, true
}
