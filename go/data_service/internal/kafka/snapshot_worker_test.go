package kafka

import (
	"context"
	"testing"
	"time"

	"data_service/internal/store"

	"github.com/go-sql-driver/mysql"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// G2a:player_snapshot 消费者「按分区并发 + 可配批量」的单测(无外部依赖)。
// 现有 snapshot_consumer_test.go 的五条用例一字不改即覆盖「默认配置 = 逐条串行」的等价性;这里覆盖新增的行为。

// counterValue 读进程默认注册表里一条计数器的当前值;序列还不存在时返回 0。用例只比较前后差值。
func counterValue(t *testing.T, name string, labels map[string]string) float64 {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, mf := range families {
		if mf.GetName() != name {
			continue
		}
	metric:
		for _, m := range mf.GetMetric() {
			got := map[string]string{}
			for _, l := range m.GetLabel() {
				got[l.GetName()] = l.GetValue()
			}
			for k, v := range labels {
				if got[k] != v {
					continue metric
				}
			}
			return m.GetCounter().GetValue()
		}
	}
	return 0
}

func snapshotMessages(t *testing.T, outcome string) float64 {
	t.Helper()
	return counterValue(t, "data_service_kafka_consumer_messages_total",
		map[string]string{"consumer": ConsumerSnapshot, "outcome": outcome})
}

func snapshotIsolations(t *testing.T, result string) float64 {
	t.Helper()
	return counterValue(t, "data_service_kafka_consumer_isolation_total",
		map[string]string{"consumer": ConsumerSnapshot, "result": result})
}

func guidsOf(rows []*store.SnapshotRow) []uint64 {
	out := make([]uint64, len(rows))
	for i, r := range rows {
		out[i] = r.SnapshotGuid
	}
	return out
}

// TestSnapshotConsumer_DefaultConfigNeverCallsBatchSink:默认配置(批 1 条)下批量接口一次都不调,
// 每条消息单独落库、单独提交且 offset 递增 —— 打开批量之前,线上行为与 2026-10-12 之前逐条消费相同。
func TestSnapshotConsumer_DefaultConfigNeverCallsBatchSink(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 1, validSnap(11)), snapMsg(t, 0, 2, validSnap(12)), snapMsg(t, 0, 3, validSnap(13)))

	require.Eventually(t, func() bool { return reader.committedMax()[0] == 3 }, eventually, 10*time.Millisecond)
	assert.Equal(t, 3, sink.callCount())
	assert.Equal(t, 0, sink.batchCallCount(), "默认配置不许走批量接口")
	calls := reader.commitCalls()
	require.Len(t, calls, 3, "每条消息单独提交")
	for i, call := range calls {
		require.Len(t, call, 1)
		assert.Equal(t, int64(i+1), call[0].Offset)
	}

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_BatchesByRowsAndCommitsHighestOffset:凑满 BatchMaxRows 即落库,一次批量调用、行序 = offset 序,
// 只提交一次、提交的是本批最高 offset。
func TestSnapshotConsumer_BatchesByRowsAndCommitsHighestOffset(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: 3, FlushInterval: time.Hour}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 5, validSnap(21)), snapMsg(t, 0, 6, validSnap(22)), snapMsg(t, 0, 7, validSnap(23)))

	require.Eventually(t, func() bool { return len(reader.commitCalls()) == 1 }, eventually, 10*time.Millisecond)
	batches := sink.batchesSnapshot()
	require.Len(t, batches, 1)
	assert.Equal(t, []uint64{21, 22, 23}, guidsOf(batches[0]), "批内行序必须是 offset 序")
	assert.Equal(t, 0, sink.callCount(), "3 行的批走批量接口")
	call := reader.commitCalls()[0]
	require.Len(t, call, 1, "每个分区只提交本批最高的那一条")
	assert.Equal(t, int64(7), call[0].Offset)

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_BatchFlushesOnInterval:凑不满时,批非空后最多等 FlushInterval 就落库(低流量时延迟有上限)。
func TestSnapshotConsumer_BatchFlushesOnInterval(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: 100, FlushInterval: 100 * time.Millisecond}, reader, sink)
	reader.push(snapMsg(t, 0, 1, validSnap(31)), snapMsg(t, 0, 2, validSnap(32)))
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	require.Eventually(t, func() bool { return reader.committedMax()[0] == 2 }, eventually, 10*time.Millisecond)
	batches := sink.batchesSnapshot()
	require.Len(t, batches, 1)
	assert.Equal(t, []uint64{31, 32}, guidsOf(batches[0]))

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_BatchFlushesOnBytes:批字节是**软上限** —— 每加入一条后检查,累计 >= 它即落库,
// 所以一批最多超出「一条消息」;只有批为空时到达的超大单条才独占一批(1 行 → 单行接口)。
func TestSnapshotConsumer_BatchFlushesOnBytes(t *testing.T) {
	m1 := snapMsg(t, 0, 1, validSnap(41))
	m2 := snapMsg(t, 0, 2, validSnap(42))
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		BatchMaxRows:  100,
		BatchMaxBytes: len(m1.Value) + 1, // 一条不够、两条超过
		FlushInterval: time.Hour,
	}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(m1, m2)
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 2 }, eventually, 10*time.Millisecond)
	batches := sink.batchesSnapshot()
	require.Len(t, batches, 1)
	assert.Equal(t, []uint64{41, 42}, guidsOf(batches[0]), "第二条进批后累计字节越过软上限,两条一起落")

	big := validSnap(43)
	big.PlayerDatabaseBlob = make([]byte, 2*len(m1.Value))
	reader.push(snapMsg(t, 0, 3, big))
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 3 }, eventually, 10*time.Millisecond)
	assert.Equal(t, 1, sink.callCount(), "批为空时到达的超大单条独占一批,1 行走单行接口")
	assert.Equal(t, uint64(43), sink.row(2).SnapshotGuid)
	assert.Len(t, sink.batchesSnapshot(), 1)

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_ZeroIntervalDrainsQueuedWithoutWaiting:FlushInterval=0 时不等待,但会把**已排队**的消息拼进同一批
// (库慢时自然攒批)。第一条单独落库(1 行 → 单行接口)期间排进来的 4 条,放行后作为一个 4 行的批落库。
func TestSnapshotConsumer_ZeroIntervalDrainsQueuedWithoutWaiting(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{gate: make(chan struct{})}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: 10}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(50)))
	require.Eventually(t, func() bool { return sink.inflightNow() == 1 }, eventually, 5*time.Millisecond, "第一条应已进入 sink 并被 gate 挡住")
	reader.push(snapMsg(t, 0, 1, validSnap(51)), snapMsg(t, 0, 2, validSnap(52)), snapMsg(t, 0, 3, validSnap(53)), snapMsg(t, 0, 4, validSnap(54)))
	// dispatcher 投递完第 5 条才会第 6 次调 FetchMessage:到这里 4 条都已在分区 worker 的通道里排队。
	require.Eventually(t, func() bool { return reader.fetches.Load() >= 6 }, eventually, 5*time.Millisecond)
	close(sink.gate)

	require.Eventually(t, func() bool { return reader.committedMax()[0] == 4 }, eventually, 10*time.Millisecond)
	assert.Equal(t, 1, sink.callCount(), "第一条走单行接口")
	batches := sink.batchesSnapshot()
	require.Len(t, batches, 1, "排队的 4 条拼成一批")
	assert.Equal(t, []uint64{51, 52, 53, 54}, guidsOf(batches[0]))

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_InflightFlushesBoundedBySemaphore:实例级落库并发上限。=1 时两个分区永远不会同时在 sink 里;
// =2 时用屏障证明两个分区确实能同时在途(不是碰巧串行)。
func TestSnapshotConsumer_InflightFlushesBoundedBySemaphore(t *testing.T) {
	for _, tc := range []struct {
		name     string
		inflight int
	}{{"inflight=1", 1}, {"inflight=2", 2}} {
		t.Run(tc.name, func(t *testing.T) {
			reader := newFakeReader(nil)
			sink := &fakeSnapSink{gate: make(chan struct{})}
			c := NewSnapshotConsumer(SnapshotConsumerConfig{MaxInflightFlushes: tc.inflight}, reader, sink)
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			done := runSnapshot(t, ctx, c)

			reader.push(snapMsg(t, 0, 0, validSnap(61)), snapMsg(t, 1, 0, validSnap(62)))
			require.Eventually(t, func() bool { return reader.fetches.Load() >= 3 }, eventually, 5*time.Millisecond,
				"两条都已投递给各自的分区 worker")
			require.Eventually(t, func() bool { return sink.inflightNow() == tc.inflight }, eventually, 5*time.Millisecond)
			if tc.inflight == 1 {
				// 给第二个分区足够的机会越过信号量;它若越过,峰值就会变成 2。
				time.Sleep(50 * time.Millisecond)
				assert.Equal(t, 1, sink.peak(), "MaxInflightFlushes=1 时同一时刻只能有一个分区在落库")
			}
			close(sink.gate)

			require.Eventually(t, func() bool {
				m := reader.committedMax()
				return m[0] == 0 && m[1] == 0 && len(m) == 2
			}, eventually, 10*time.Millisecond)
			assert.Equal(t, tc.inflight, sink.peak())

			cancel()
			assert.NoError(t, waitDone(t, done))
		})
	}
}

// TestSnapshotConsumer_PerPartitionOrderAndMonotonicCommits:两个分区交错到达、并发落库,每个分区 sink 看到的 guid 序
// 都是 offset 序,每个分区的提交序列严格递增 —— 分区内顺序与「先落库后提交」不因并发而改变。
func TestSnapshotConsumer_PerPartitionOrderAndMonotonicCommits(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		BatchMaxRows:       4,
		FlushInterval:      5 * time.Millisecond,
		MaxInflightFlushes: 2,
	}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	const perPartition = 10
	for i := 0; i < perPartition; i++ {
		reader.push(snapMsg(t, 0, int64(i), validSnap(uint64(1000+i))), snapMsg(t, 1, int64(i), validSnap(uint64(2000+i))))
	}
	require.Eventually(t, func() bool {
		m := reader.committedMax()
		return m[0] == perPartition-1 && m[1] == perPartition-1
	}, eventually, 10*time.Millisecond)

	byPartition := map[uint64][]uint64{}
	for _, g := range sink.guidsInOrder() {
		byPartition[g/1000] = append(byPartition[g/1000], g)
	}
	for p, guids := range byPartition {
		require.Lenf(t, guids, perPartition, "partition %d", p-1)
		for i := 1; i < len(guids); i++ {
			assert.Lessf(t, guids[i-1], guids[i], "partition %d 的落库顺序必须是 offset 序: %v", p-1, guids)
		}
	}
	last := map[int]int64{}
	for _, call := range reader.commitCalls() {
		for _, m := range call {
			if prev, ok := last[m.Partition]; ok {
				assert.Greaterf(t, m.Offset, prev, "partition %d 的提交必须严格递增", m.Partition)
			}
			last[m.Partition] = m.Offset
		}
	}

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_BatchFailureIsolatesPoisonRowAndCommitsPrefix:批量恒失败 → 逐条兜底;毒丸行之前的行落库,
// 只提交到它之前(含夹在中间的坏消息),消费者停在它那里 —— 与逐条消费「落到它为止」的结局相同,不新增跳过。
func TestSnapshotConsumer_BatchFailureIsolatesPoisonRowAndCommitsPrefix(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{failBatch: true, failGuids: map[uint64]bool{84: true}}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		BatchMaxRows:   5,
		FlushInterval:  time.Hour,
		DBMaxAttempts:  2,
		DBRetryBackoff: time.Millisecond,
	}, reader, sink)
	stoppedBefore := snapshotIsolations(t, "stopped")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(
		snapMsg(t, 0, 10, validSnap(81)),
		snapMsg(t, 0, 11, validSnap(82)),
		badMsg(0, 12),
		snapMsg(t, 0, 13, validSnap(84)), // 毒丸
		snapMsg(t, 0, 14, validSnap(85)),
	)

	err := waitDone(t, done)
	require.Error(t, err)
	assert.ErrorIs(t, err, ErrConsumerStoppedOnDB)
	assert.ErrorIs(t, err, errDBDown, "停机错误必须能 errors.Is 到底层 DB 错误")
	assert.True(t, c.Health().StoppedOnError())
	assert.ErrorIs(t, c.Health().LastError(), errDBDown)
	assert.Equal(t, []uint64{81, 82}, sink.guidsInOrder(), "毒丸之前的行逐条落库,之后的行不落")
	assert.Equal(t, map[int]int64{0: 12}, reader.committedMax(), "只提交到毒丸之前(含坏消息 offset 12)")
	assert.Equal(t, float64(1), snapshotIsolations(t, "stopped")-stoppedBefore)
}

// TestSnapshotConsumer_BatchFailureRecoveredRowByRowContinues:批量恒失败、逐条全成功 → 整批提交、继续消费下一批、健康。
// 批量**永远不会**比逐条更容易卡住。
func TestSnapshotConsumer_BatchFailureRecoveredRowByRowContinues(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{failBatch: true}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		BatchMaxRows:   3,
		FlushInterval:  time.Hour,
		DBMaxAttempts:  2,
		DBRetryBackoff: time.Millisecond,
	}, reader, sink)
	recoveredBefore := snapshotIsolations(t, "recovered")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(91)), snapMsg(t, 0, 1, validSnap(92)), snapMsg(t, 0, 2, validSnap(93)))
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 2 }, eventually, 10*time.Millisecond)
	reader.push(snapMsg(t, 0, 3, validSnap(94)), snapMsg(t, 0, 4, validSnap(95)), snapMsg(t, 0, 5, validSnap(96)))
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 5 }, eventually, 10*time.Millisecond)

	assert.Equal(t, []uint64{91, 92, 93, 94, 95, 96}, sink.guidsInOrder())
	assert.True(t, c.Health().Healthy())
	assert.Equal(t, float64(2), snapshotIsolations(t, "recovered")-recoveredBefore)
	assert.Equal(t, 4, sink.batchCallCount(), "每批按 DBMaxAttempts=2 试过批量才兜底")

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_BatchTooLargeSkipsBatchRetries:「批本身太大」(包过大 / 事务过大)是确定性错误,
// 批量只试一次就逐条兜底,不按 DBMaxAttempts 白等。
func TestSnapshotConsumer_BatchTooLargeSkipsBatchRetries(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{failBatch: true, batchErr: mysql.ErrPktTooLarge}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		BatchMaxRows:   2,
		FlushInterval:  time.Hour,
		DBMaxAttempts:  5,
		DBRetryBackoff: time.Hour, // 若误走了批级重试,这里会让用例超时,而不是悄悄通过
	}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(97)), snapMsg(t, 0, 1, validSnap(98)))
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 1 }, eventually, 10*time.Millisecond)
	assert.Equal(t, 1, sink.batchCallCount())
	assert.Equal(t, []uint64{97, 98}, sink.guidsInOrder())

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_OnePartitionStopStopsAllWithoutCommittingInFlight:一个分区 DB 重试用尽停机 → 整个 Run 停下;
// 另一个分区正在落库的那批不提交(重建后重放、被 guid 去重);Run 返回时所有 worker 都已退出(之后 sink 不再被调)。
// MaxInflightFlushes 必须是 2:=1 时被挡住的分区占着唯一名额,失败的分区永远拿不到名额,用例会死等。
func TestSnapshotConsumer_OnePartitionStopStopsAllWithoutCommittingInFlight(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{
		gate:      make(chan struct{}),
		gateOnly:  map[uint64]bool{2001: true},
		failGuids: map[uint64]bool{1001: true},
	}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{
		MaxInflightFlushes: 2,
		DBMaxAttempts:      2,
		DBRetryBackoff:     time.Millisecond,
	}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 1, 0, validSnap(2001)))
	require.Eventually(t, func() bool { return sink.inflightNow() == 1 }, eventually, 5*time.Millisecond, "分区 1 的批应已在途")
	reader.push(snapMsg(t, 0, 0, validSnap(1001)))

	err := waitDone(t, done)
	require.Error(t, err)
	assert.ErrorIs(t, err, ErrConsumerStoppedOnDB)
	assert.ErrorIs(t, err, errDBDown)
	assert.True(t, c.Health().StoppedOnError())
	assert.Empty(t, reader.commitCalls(), "失败分区与在途分区都不许提交")

	entered := sink.enteredCount()
	time.Sleep(50 * time.Millisecond)
	assert.Equal(t, entered, sink.enteredCount(), "Run 返回后不许再有 worker 调 sink")
	assert.Equal(t, 0, sink.inflightNow())
}

// TestSnapshotConsumer_ShutdownMidFlushIsNotAFailure:落库途中进程关停 → Run 返回 nil、不标记故障、不提交、不计 db_error。
// 关停不是 DB 故障:把它记成故障会让每次发布都刷一次停机告警。
func TestSnapshotConsumer_ShutdownMidFlushIsNotAFailure(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{gate: make(chan struct{})}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{}, reader, sink)
	dbErrBefore := snapshotMessages(t, "db_error")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(111)))
	require.Eventually(t, func() bool { return sink.inflightNow() == 1 }, eventually, 5*time.Millisecond)
	cancel()

	assert.NoError(t, waitDone(t, done))
	assert.False(t, c.Health().StoppedOnError())
	assert.Empty(t, reader.commitCalls())
	assert.Equal(t, float64(0), snapshotMessages(t, "db_error")-dbErrBefore)
}

// TestSnapshotConsumer_WorkerPanicStopsRunInsteadOfHanging:sink panic → Run 在时限内返回 errSnapshotWorkerAborted
// (而不是其它分区继续跑、这个分区静默停摆),该批不提交,落库名额已归还。
func TestSnapshotConsumer_WorkerPanicStopsRunInsteadOfHanging(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{panicGuid: 77}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{}, reader, sink)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(77)))

	err := waitDone(t, done)
	assert.ErrorIs(t, err, errSnapshotWorkerAborted)
	assert.True(t, c.Health().StoppedOnError())
	assert.Empty(t, reader.commitCalls())
	assert.Equal(t, 0, len(c.sem), "panic 展开时落库名额必须归还,否则同一实例重建后会卡在取名额上")
}

// TestSnapshotConsumer_BatchDuplicatesCountedAndCommitted:批量回报的新插入数少于行数 → 差值计 duplicate,整批照常提交。
func TestSnapshotConsumer_BatchDuplicatesCountedAndCommitted(t *testing.T) {
	reader := newFakeReader(nil)
	sink := &fakeSnapSink{batchDuplicates: 1}
	c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: 3, FlushInterval: time.Hour}, reader, sink)
	insertedBefore := snapshotMessages(t, "inserted")
	dupBefore := snapshotMessages(t, "duplicate")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := runSnapshot(t, ctx, c)

	reader.push(snapMsg(t, 0, 0, validSnap(121)), snapMsg(t, 0, 1, validSnap(122)), snapMsg(t, 0, 2, validSnap(123)))
	require.Eventually(t, func() bool { return reader.committedMax()[0] == 2 }, eventually, 10*time.Millisecond)
	assert.Equal(t, float64(2), snapshotMessages(t, "inserted")-insertedBefore)
	assert.Equal(t, float64(1), snapshotMessages(t, "duplicate")-dupBefore)

	cancel()
	assert.NoError(t, waitDone(t, done))
}

// TestSnapshotConsumer_FirstRowFailureMakesNoEmptyCommitAndBatchDropsPayload:
//   - 第一行就失败(前面没有坏消息)时不发空提交:真 kafka-go 会白发一次请求,「停机不提交」的断言也会失真;
//   - 批里留着的消息只为提交:载荷(Value / Key)已剥掉(否则每条快照在批里有两份),但 Topic / Partition / Offset 保留。
func TestSnapshotConsumer_FirstRowFailureMakesNoEmptyCommitAndBatchDropsPayload(t *testing.T) {
	t.Run("第一行失败不发空提交", func(t *testing.T) {
		reader := newFakeReader(nil)
		sink := &fakeSnapSink{failGuids: map[uint64]bool{131: true}}
		c := NewSnapshotConsumer(SnapshotConsumerConfig{DBMaxAttempts: 2, DBRetryBackoff: time.Millisecond}, reader, sink)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		done := runSnapshot(t, ctx, c)

		reader.push(snapMsg(t, 0, 0, validSnap(131)))
		err := waitDone(t, done)
		assert.ErrorIs(t, err, ErrConsumerStoppedOnDB)
		assert.Empty(t, reader.commitCalls())
	})

	t.Run("批里的消息剥掉载荷但保留 Topic", func(t *testing.T) {
		const topic = "player_snapshot_topic_g1"
		reader := newFakeReader(nil)
		sink := &fakeSnapSink{}
		c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: 2, FlushInterval: time.Hour}, reader, sink)
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		done := runSnapshot(t, ctx, c)

		m1 := snapMsg(t, 0, 0, validSnap(132))
		m1.Topic = topic
		m2 := snapMsg(t, 0, 1, validSnap(133))
		m2.Topic = topic
		reader.push(m1, m2)

		require.Eventually(t, func() bool { return len(reader.commitCalls()) == 1 }, eventually, 10*time.Millisecond)
		committed := reader.commitCalls()[0]
		require.Len(t, committed, 1)
		assert.Nil(t, committed[0].Value, "批里的消息不该再带载荷")
		assert.Nil(t, committed[0].Key)
		assert.Equal(t, topic, committed[0].Topic, "kafka-go 按 Topic + Partition 提交,Topic 必须保留")
		assert.Equal(t, int64(1), committed[0].Offset)
		assert.Equal(t, []uint64{132, 133}, sink.guidsInOrder(), "落库的数据来自 decode 时拷出的 row.Data,不受剥载荷影响")

		cancel()
		assert.NoError(t, waitDone(t, done))
	})
}

// TestSnapshotConsumerConfig_ClampsBatchRowsToStoreCap:直接构造配置(不经 yaml 校验)给出超过 store 上限的批行数时钳到上限,
// 否则 store 每批都拒绝、每批都退回逐条兜底,静默变慢。
func TestSnapshotConsumerConfig_ClampsBatchRowsToStoreCap(t *testing.T) {
	c := NewSnapshotConsumer(SnapshotConsumerConfig{BatchMaxRows: store.MaxSnapshotBatchRows + 44}, newFakeReader(nil), &fakeSnapSink{})
	assert.Equal(t, store.MaxSnapshotBatchRows, c.cfg.BatchMaxRows)
	assert.Equal(t, 1, cap(c.sem), "MaxInflightFlushes 零值 = 1")
}
