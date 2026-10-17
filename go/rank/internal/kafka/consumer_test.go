package kafka

import (
	"context"
	"errors"
	"fmt"
	"testing"
	"time"

	"rank/internal/data"
	"rank/internal/metrics"

	kafkapb "proto/contracts/kafka"

	"github.com/prometheus/client_golang/prometheus"
	kafkago "github.com/segmentio/kafka-go"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// 消费循环的单测只用替身:fakeSource 按顺序吐消息、记提交;fakeWriter 按脚本返回结果。不连 Kafka、不连 Redis。

type fakeSource struct {
	msgs      []kafkago.Message
	next      int
	committed []int64
	// afterLast:消息吐完后取消 ctx,让循环退出(否则 FetchMessage 会一直阻塞)。
	afterLast context.CancelFunc
}

func (s *fakeSource) FetchMessage(ctx context.Context) (kafkago.Message, error) {
	if s.next >= len(s.msgs) {
		if s.afterLast != nil {
			s.afterLast()
		}
		<-ctx.Done()
		return kafkago.Message{}, ctx.Err()
	}
	m := s.msgs[s.next]
	s.next++
	return m, nil
}

func (s *fakeSource) CommitMessages(_ context.Context, msgs ...kafkago.Message) error {
	for _, m := range msgs {
		s.committed = append(s.committed, m.Offset)
	}
	return nil
}

type writeResult struct {
	outcome data.WriteOutcome
	err     error
}

type fakeWriter struct {
	results []writeResult // 按调用顺序返回;用完后重复最后一个
	calls   []data.Snapshot
}

func (w *fakeWriter) Write(_ context.Context, s data.Snapshot) (data.WriteOutcome, error) {
	w.calls = append(w.calls, s)
	i := len(w.calls) - 1
	if i >= len(w.results) {
		i = len(w.results) - 1
	}
	return w.results[i].outcome, w.results[i].err
}

// recordingSleep 记下每次退避时长;cancelAfter > 0 时第 cancelAfter 次退避取消 ctx 并返回 false(模拟停机)。
type recordingSleep struct {
	waits       []time.Duration
	cancelAfter int
	cancel      context.CancelFunc
}

func (r *recordingSleep) sleep(_ context.Context, d time.Duration) bool {
	r.waits = append(r.waits, d)
	if r.cancelAfter > 0 && len(r.waits) >= r.cancelAfter {
		r.cancel()
		return false
	}
	return true
}

func snapshotMessage(t *testing.T, offset int64, ev *kafkapb.PlayerRatingSnapshotEvent) kafkago.Message {
	t.Helper()
	value, err := proto.Marshal(ev)
	require.NoError(t, err)
	return kafkago.Message{Partition: 0, Offset: offset, Key: []byte(fmt.Sprint(ev.GetPlayerId())), Value: value}
}

func validEvent(playerID uint64) *kafkapb.PlayerRatingSnapshotEvent {
	return &kafkapb.PlayerRatingSnapshotEvent{
		PlayerId: playerID, RatingCenti: 151629, Games: 3, UpdatedAtMs: 1_800_000_000_123,
		RatingGen: 1_800_000_000_000, BattleId: 7, FinishedAtMs: 1_800_000_000_100,
	}
}

// counterValue 从默认 registry 读一条计数器的当前值(指标在 internal/metrics 里是未导出变量,只能经 Gather 读)。
func counterValue(t *testing.T, name string, labels map[string]string) float64 {
	t.Helper()
	metrics.Register()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, f := range families {
		if f.GetName() != name {
			continue
		}
	next:
		for _, m := range f.GetMetric() {
			for _, lp := range m.GetLabel() {
				if want, ok := labels[lp.GetName()]; ok && want != lp.GetValue() {
					continue next
				}
			}
			if c := m.GetCounter(); c != nil {
				return c.GetValue()
			}
		}
	}
	t.Fatalf("metric %s%v not found", name, labels)
	return 0
}

// gaugeValue 从默认 registry 读一个无 label 的 gauge 的当前值。
func gaugeValue(t *testing.T, name string) float64 {
	t.Helper()
	metrics.Register()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, f := range families {
		if f.GetName() != name {
			continue
		}
		for _, m := range f.GetMetric() {
			if g := m.GetGauge(); g != nil {
				return g.GetValue()
			}
		}
	}
	t.Fatalf("metric %s not found", name)
	return 0
}

func runOnce(t *testing.T, msgs []kafkago.Message, w *fakeWriter, sl *recordingSleep) *fakeSource {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	src := &fakeSource{msgs: msgs, afterLast: cancel}
	if sl == nil {
		sl = &recordingSleep{}
	}
	sl.cancel = cancel
	done := make(chan struct{})
	go func() {
		defer close(done)
		runConsumer(ctx, src, w, time.Now, sl.sleep)
	}()
	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("消费循环没有退出")
	}
	return src
}

// 正常路径:payload 原样映射成 Snapshot,写入后提交;stale / gen_reset 同样是终态、照常提交并按 outcome 计数。
func TestConsumerWritesAndCommits(t *testing.T) {
	stale0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteStale})
	reset0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteGenReset})
	w := &fakeWriter{results: []writeResult{{data.WriteWritten, nil}, {data.WriteStale, nil}, {data.WriteGenReset, nil}}}
	src := runOnce(t, []kafkago.Message{
		snapshotMessage(t, 10, validEvent(42)),
		snapshotMessage(t, 11, validEvent(42)),
		snapshotMessage(t, 12, validEvent(43)),
	}, w, nil)

	require.Equal(t, []int64{10, 11, 12}, src.committed, "三条都到达终态,逐条提交")
	require.Len(t, w.calls, 3)
	ev := validEvent(42)
	require.Equal(t, data.Snapshot{PlayerID: 42, RatingCenti: ev.GetRatingCenti(), Games: ev.GetGames(),
		UpdatedAtMs: ev.GetUpdatedAtMs(), RatingGen: ev.GetRatingGen()}, w.calls[0], "payload 字段逐个映射,不取 Kafka 时间戳")
	require.Equal(t, stale0+1, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteStale}))
	require.Equal(t, reset0+1, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteGenReset}))
}

// 永久错误(反序列化失败 / 快照非法 / 越界 / 版本值坏了):跳过并提交,不重试、不卡分区;每种按各自 outcome 计数,
// 都不记进瞬时的 error(告警 RankWriteSkipped 按这三档出现一次就报,RankWriteFailing 只看持续的 error)。
// 反序列化失败与快照非法同记 invalid。
func TestConsumerSkipsPermanentErrors(t *testing.T) {
	invalid0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteInvalid})
	oor0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteOutOfRange})
	malformed0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteMalformedVersion})
	error0 := counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteError})
	errs0 := counterValue(t, "rank_consume_errors_total", nil)
	w := &fakeWriter{results: []writeResult{
		{data.WriteStale, fmt.Errorf("%w: player_id=0", data.ErrInvalidSnapshot)},
		{data.WriteStale, fmt.Errorf("wrap: %w", data.ErrOutOfRange)},
		{data.WriteStale, fmt.Errorf("%w: player=9", data.ErrMalformedVersion)},
	}}
	sl := &recordingSleep{}
	src := runOnce(t, []kafkago.Message{
		{Offset: 20, Value: []byte{0xff, 0xff, 0xff}}, // 反序列化失败:不碰 writer
		snapshotMessage(t, 21, validEvent(1)),
		snapshotMessage(t, 22, validEvent(2)),
		snapshotMessage(t, 23, validEvent(3)),
	}, w, sl)

	require.Equal(t, []int64{20, 21, 22, 23}, src.committed, "坏消息跳过并提交,后面的照常处理")
	require.Len(t, w.calls, 3, "反序列化失败的那条不碰 writer;永久错误各只写一次、不重试")
	require.Empty(t, sl.waits, "永久错误不退避")
	require.Equal(t, invalid0+2, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteInvalid}), "反序列化失败 1 + 快照非法 1")
	require.Equal(t, oor0+1, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteOutOfRange}))
	require.Equal(t, malformed0+1, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteMalformedVersion}))
	require.Equal(t, error0, counterValue(t, "rank_write_total", map[string]string{"outcome": metrics.WriteError}), "永久跳过不记进瞬时的 error")
	require.Equal(t, errs0+4, counterValue(t, "rank_consume_errors_total", nil), "反序列化 1 + 三种永久写错误各 1")
}

// 消费循环一开始就写"最后消费时刻":新进程在第一条消息之前不能报 0(安静期滚动发布会让 RankConsumerStalled 误报)。
func TestConsumerMarksStartTimeBeforeFirstMessage(t *testing.T) {
	started := time.Unix(1_900_000_000, 0)
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	src := &fakeSource{afterLast: cancel} // 一条消息都没有
	done := make(chan struct{})
	go func() {
		defer close(done)
		runConsumer(ctx, src, &fakeWriter{results: []writeResult{{data.WriteWritten, nil}}}, func() time.Time { return started }, (&recordingSleep{}).sleep)
	}()
	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("消费循环没有退出")
	}
	require.Equal(t, float64(started.Unix()), gaugeValue(t, "rank_last_consume_unixtime"))
}

// 瞬时错误(RankRedis 不可达):不跳过,指数退避重试到成功,只提交一次。
func TestConsumerRetriesTransientErrorsWithBackoff(t *testing.T) {
	transient := errors.New("dial tcp: connection refused")
	w := &fakeWriter{results: []writeResult{
		{data.WriteStale, transient}, {data.WriteStale, transient}, {data.WriteStale, transient}, {data.WriteWritten, nil},
	}}
	sl := &recordingSleep{}
	src := runOnce(t, []kafkago.Message{snapshotMessage(t, 30, validEvent(5))}, w, sl)

	require.Equal(t, []int64{30}, src.committed, "重试成功后才提交,且只提交一次")
	require.Len(t, w.calls, 4)
	require.Equal(t, []time.Duration{writeRetryInitialBackoff, 2 * writeRetryInitialBackoff, 4 * writeRetryInitialBackoff}, sl.waits, "指数退避")
}

// 退避有上限:长时间宕机时每次等待封顶 writeRetryMaxBackoff,不会越等越久。
func TestConsumerBackoffIsCapped(t *testing.T) {
	transient := errors.New("redis down")
	results := make([]writeResult, 0, 12)
	for i := 0; i < 11; i++ {
		results = append(results, writeResult{data.WriteStale, transient})
	}
	results = append(results, writeResult{data.WriteWritten, nil})
	w := &fakeWriter{results: results}
	sl := &recordingSleep{}
	runOnce(t, []kafkago.Message{snapshotMessage(t, 40, validEvent(6))}, w, sl)

	require.Len(t, sl.waits, 11)
	for _, d := range sl.waits {
		require.LessOrEqual(t, d, writeRetryMaxBackoff)
	}
	require.Equal(t, writeRetryMaxBackoff, sl.waits[len(sl.waits)-1])
}

// 停机打断重试:不提交(下次从这条重来),也不再碰后面的消息。
func TestConsumerDoesNotCommitWhenStoppedMidRetry(t *testing.T) {
	transient := errors.New("redis down")
	w := &fakeWriter{results: []writeResult{{data.WriteStale, transient}}}
	sl := &recordingSleep{cancelAfter: 2}
	src := runOnce(t, []kafkago.Message{
		snapshotMessage(t, 50, validEvent(7)),
		snapshotMessage(t, 51, validEvent(8)),
	}, w, sl)

	require.Empty(t, src.committed, "没到终态的消息不得提交")
	for _, s := range w.calls {
		require.Equal(t, uint64(7), s.PlayerID, "停机后不再处理后面的消息")
	}
}

// key 与 payload 的 player_id 不一致:只记日志,以 payload 为准照常写。
func TestConsumerKeyMismatchStillWrites(t *testing.T) {
	w := &fakeWriter{results: []writeResult{{data.WriteWritten, nil}}}
	msg := snapshotMessage(t, 60, validEvent(77))
	msg.Key = []byte("78")
	src := runOnce(t, []kafkago.Message{msg}, w, nil)

	require.Equal(t, []int64{60}, src.committed)
	require.Len(t, w.calls, 1)
	require.Equal(t, uint64(77), w.calls[0].PlayerID)
}

// 参数守卫按哨兵断言(不连 Kafka):守卫被删时下游 sarama 拨不通也会报错,只断言 Error 会空转通过。
func TestEnsureSnapshotTopicRejectsIncompleteConfig(t *testing.T) {
	valid := ConsumerConfig{Brokers: []string{"127.0.0.1:1"}, Topic: "match-rating-snapshot", GroupID: "rank-rating", Partitions: 3}
	for name, mutate := range map[string]func(c *ConsumerConfig){
		"没有 broker": func(c *ConsumerConfig) { c.Brokers = nil },
		"topic 为空":  func(c *ConsumerConfig) { c.Topic = "" },
		"消费组为空":     func(c *ConsumerConfig) { c.GroupID = "" },
		"分区为 0":     func(c *ConsumerConfig) { c.Partitions = 0 },
	} {
		c := valid
		mutate(&c)
		require.ErrorIs(t, EnsureSnapshotTopic(c), ErrInvalidConsumerConfig, name)
	}
	done, err := StartSnapshotConsumer(context.Background(), valid, nil)
	require.ErrorIs(t, err, ErrInvalidConsumerConfig, "nil writer")
	require.Nil(t, done)
}
