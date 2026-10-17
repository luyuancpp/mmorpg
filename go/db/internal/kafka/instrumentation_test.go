package kafka

// 测量打点(db_task_redis_step_seconds、db_ordering_guard_total、db_subshard_batch_size、db_task_stage_seconds 的
// sql / task_total / queue_wait / kafka_e2e)的单测。指标是进程级的累计值,用例一律比较前后差值。
//
// metricSample 经 prometheus.DefaultGatherer 读取,不 import client_golang 的 testutil(避免改 go.sum)。

import (
	"context"
	"fmt"
	"strconv"
	"sync"
	"testing"
	"time"

	"db/internal/metrics"

	"github.com/IBM/sarama"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// metricSample 返回 family 里 label 恰好等于 labels 的那条序列:直方图返回 (count, sum),计数器 / 仪表返回 (value, 0)。
// 序列不存在时返回 (0, 0) —— 直方图在第一次 Observe 之前没有序列。
func metricSample(t *testing.T, family string, labels map[string]string) (float64, float64) {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	require.NoError(t, err)
	for _, mf := range families {
		if mf.GetName() != family {
			continue
		}
		for _, m := range mf.GetMetric() {
			pairs := m.GetLabel()
			if len(pairs) != len(labels) {
				continue
			}
			matched := true
			for _, l := range pairs {
				if want, ok := labels[l.GetName()]; !ok || want != l.GetValue() {
					matched = false
					break
				}
			}
			if !matched {
				continue
			}
			switch {
			case m.GetHistogram() != nil:
				return float64(m.GetHistogram().GetSampleCount()), m.GetHistogram().GetSampleSum()
			case m.GetCounter() != nil:
				return m.GetCounter().GetValue(), 0
			case m.GetGauge() != nil:
				return m.GetGauge().GetValue(), 0
			}
		}
	}
	return 0, 0
}

// metricCount 是 metricSample 的第一个返回值。
func metricCount(t *testing.T, family string, labels map[string]string) float64 {
	t.Helper()
	count, _ := metricSample(t, family, labels)
	return count
}

func redisStepCount(t *testing.T, op, step string) float64 {
	t.Helper()
	return metricCount(t, "db_task_redis_step_seconds", map[string]string{"op": op, "step": step})
}

func redisStepCounts(t *testing.T, op string) map[string]float64 {
	t.Helper()
	out := make(map[string]float64, len(metrics.RedisSteps))
	for _, step := range metrics.RedisSteps {
		out[step] = redisStepCount(t, op, step)
	}
	return out
}

func stageCount(t *testing.T, op, stage string) float64 {
	t.Helper()
	return metricCount(t, "db_task_stage_seconds", map[string]string{"op": op, "stage": stage})
}

func orderingGuardCount(t *testing.T, outcome string) float64 {
	t.Helper()
	return metricCount(t, "db_ordering_guard_total", map[string]string{"outcome": outcome})
}

func orderingGuardCounts(t *testing.T) map[string]float64 {
	t.Helper()
	out := map[string]float64{}
	for _, outcome := range []string{metrics.OrderingApply, metrics.OrderingApplyNoCursor, metrics.OrderingStale,
		metrics.OrderingIncomparable, metrics.OrderingError} {
		out[outcome] = orderingGuardCount(t, outcome)
	}
	return out
}

// 一条 Kafka 来源的写(带 owner_epoch、seq>0、不走离线扩容)走完 handleTask 的成功路径:11 个 step 在 op=write 下各记一次。
// 这条用例同时钉住「每个往返只记一次」:任何一步被记两次(例如挂了 Hook 又在调用点计时)都会让差值变成 2。
func TestRedisSteps_KafkaWriteObservesEachStepOnce(t *testing.T) {
	w, _, h := newTestWorker(t, 0)
	const key uint64 = 9701
	wt := makeEpochWriteTask(t, key, placementTestMsgType, 10, 3)
	session := attachKafkaClaim(t, w, wt)

	before := redisStepCounts(t, "write")
	w.handleTask(wt, false)
	after := redisStepCounts(t, "write")

	require.Len(t, h.callsForKey(key), 1, "the write must reach SQL")
	require.Equal(t, []int64{9}, session.marked(), "the write must complete and ACK")
	for _, step := range metrics.RedisSteps {
		assert.Equalf(t, before[step]+1, after[step], "step %s must be observed exactly once for one write", step)
	}
}

// 读任务只有选库、扩容状态、复核、缓存回写四步;另外七步在 op=read 下不出现,也不会记到 op=write 上。
func TestRedisSteps_ReadObservesReadSteps(t *testing.T) {
	w, _, h := newTestWorker(t, 0)
	const key uint64 = 9702
	rt := makeReadTask(t, key, placementTestMsgType, "preload")
	rt.seq = 3

	readBefore := redisStepCounts(t, "read")
	writeBefore := redisStepCounts(t, "write")
	w.handleTask(rt, false)
	readAfter := redisStepCounts(t, "read")
	writeAfter := redisStepCounts(t, "write")

	require.Len(t, h.callsForKey(key), 1)
	readSteps := map[string]bool{
		metrics.RedisStepPlacementLookup:  true,
		metrics.RedisStepExpandGet:        true,
		metrics.RedisStepPlacementRecheck: true,
		metrics.RedisStepCacheSet:         true,
	}
	for _, step := range metrics.RedisSteps {
		want := readBefore[step]
		if readSteps[step] {
			want++
		}
		assert.Equalf(t, want, readAfter[step], "op=read step %s", step)
		assert.Equalf(t, writeBefore[step], writeAfter[step], "a read must not touch op=write step %s", step)
	}
}

// 游标守卫的每个判定各记一次;读任务不计数。直接调 orderingForTask:经 handleTask 时 Redis 故障会先卡在取锁上,
// 走不到游标守卫。
func TestOrderingGuardCounter_Outcomes(t *testing.T) {
	w, mr, _ := newTestWorker(t, 0)
	ctx := context.Background()
	const key uint64 = 9703
	cursorKey := appliedSeqKey(w.topic, key, placementTestMsgType)

	check := func(name string, task *workerTask, wantOutcome string) {
		t.Helper()
		before := orderingGuardCounts(t)
		_, _ = orderingForTask(ctx, w.redisClient, w.topic, task.dbTask, task)
		after := orderingGuardCounts(t)
		for outcome, b := range before {
			want := b
			if outcome == wantOutcome {
				want++
			}
			assert.Equalf(t, want, after[outcome], "%s: outcome %s", name, outcome)
		}
	}

	check("first write without a cursor", makeWriteTask(t, key, placementTestMsgType, 10), metrics.OrderingApplyNoCursor)

	require.NoError(t, mr.Set(cursorKey, "v2:0:10"))
	check("same partition, larger seq", makeWriteTask(t, key, placementTestMsgType, 11), metrics.OrderingApply)
	check("same partition, smaller seq", makeWriteTask(t, key, placementTestMsgType, 9), metrics.OrderingStale)

	otherPartition := makeWriteTask(t, key, placementTestMsgType, 50)
	otherPartition.originPartition = 5
	check("another partition", otherPartition, metrics.OrderingIncomparable)

	require.NoError(t, mr.Set(cursorKey, "garbage"))
	check("unparsable cursor", makeWriteTask(t, key, placementTestMsgType, 12), metrics.OrderingError)

	mr.SetError("ERR injected redis failure")
	check("cursor read failure", makeWriteTask(t, key, placementTestMsgType, 12), metrics.OrderingError)
	mr.SetError("")

	before := orderingGuardCounts(t)
	read := makeReadTask(t, key, placementTestMsgType, "guard")
	_, _ = orderingForTask(ctx, w.redisClient, w.topic, read.dbTask, read)
	assert.Equal(t, before, orderingGuardCounts(t), "read tasks are not counted")
}

// 子 shard 一次把通道里已有的 3 条合成一批交给 processTaskBatch:batch 直方图 count+1、sum+3。
// 先放好任务再关通道,runSubShard 同步跑到通道关闭为止,结果确定。
func TestSubShardBatchSizeObserved(t *testing.T) {
	w, _, h := newTestWorker(t, 0)
	ch := make(chan *workerTask, 8)
	w.subShardCount = 1
	w.subShardChans = []chan *workerTask{ch}
	for i := 0; i < 3; i++ {
		ch <- makeReadTask(t, uint64(9710+i), placementTestMsgType, fmt.Sprintf("batch-%d", i))
	}
	close(ch)

	countBefore, sumBefore := metricSample(t, "db_subshard_batch_size", map[string]string{})
	var subWg sync.WaitGroup
	subWg.Add(1)
	w.runSubShard(0, true, &subWg)
	subWg.Wait()
	countAfter, sumAfter := metricSample(t, "db_subshard_batch_size", map[string]string{})

	assert.Equal(t, countBefore+1, countAfter)
	assert.Equal(t, sumBefore+3, sumAfter)
	assert.Len(t, h.snapshot(), 3, "all three tasks of the batch are executed")
}

// kafkaWriteTaskAt 拼一条 Kafka 来源的写(像 ConsumeClaim 那样把解码后的 dbTask 挂在信封上),Timestamp 由调用方给。
func kafkaWriteTaskAt(t *testing.T, w *worker, key uint64, seq uint64, ts time.Time) *workerTask {
	t.Helper()
	wt := makeWriteTask(t, key, placementTestMsgType, seq)
	body, err := proto.Marshal(wt.dbTask)
	require.NoError(t, err)
	wt.kafkaMsg = &sarama.ConsumerMessage{
		Topic: w.topic, Partition: 0, Offset: int64(seq) - 1,
		Key: []byte(strconv.FormatUint(key, 10)), Value: body, Timestamp: ts,
	}
	return wt
}

// queue_wait 只在 enqueuedAt 非零时记;task_total 每次 handleTask 都记;kafka_e2e 只记 Kafka 来源、Timestamp 非零、
// 差值非负的任务(生产者时钟比本机快导致的负值丢弃)。
func TestStages_QueueWaitTaskTotalKafkaE2E(t *testing.T) {
	w, _, _ := newTestWorker(t, 0)
	now := time.Now()

	type sample struct{ queue, total, e2e float64 }
	read := func() sample {
		return sample{
			queue: stageCount(t, "write", metrics.StageQueueWait),
			total: stageCount(t, "write", metrics.StageTaskTotal),
			e2e:   stageCount(t, "write", metrics.StageKafkaE2E),
		}
	}
	run := func(name string, task *workerTask, want sample) {
		t.Helper()
		before := read()
		w.handleTask(task, true)
		after := read()
		assert.Equalf(t, before.queue+want.queue, after.queue, "%s: queue_wait", name)
		assert.Equalf(t, before.total+want.total, after.total, "%s: task_total", name)
		assert.Equalf(t, before.e2e+want.e2e, after.e2e, "%s: kafka_e2e", name)
	}

	kafkaTask := kafkaWriteTaskAt(t, w, 9720, 10, now.Add(-2*time.Second))
	kafkaTask.enqueuedAt = now.Add(-time.Second)
	run("kafka task with enqueue time", kafkaTask, sample{queue: 1, total: 1, e2e: 1})

	// 没有 kafkaMsg 的任务:不进 kafka_e2e;没有 enqueuedAt:不进 queue_wait。
	run("task without kafka message or enqueue time", makeWriteTask(t, 9721, placementTestMsgType, 10),
		sample{queue: 0, total: 1, e2e: 0})

	// 真正的重试源:经生产的 consumeOneRetryTask 认领(fromRetry、enqueuedAt 由认领时赋值、没有 kafkaMsg)。
	// queue_wait 只看 enqueuedAt 非零、不看时长,结果与墙钟无关。
	require.NoError(t, saveToRetryQueue(w.ctx, w.redisClient, w.retryQueueKey,
		makeWriteTask(t, 9724, placementTestMsgType, 10).dbTask, 10, 0))
	retryTask := claimRetry(t, w)
	require.NotNil(t, retryTask)
	require.True(t, retryTask.fromRetry)
	require.Nil(t, retryTask.kafkaMsg, "retry-origin tasks carry no Kafka message")
	require.False(t, retryTask.enqueuedAt.IsZero(), "claiming a retry stamps its enqueue time")
	run("retry task", retryTask, sample{queue: 1, total: 1, e2e: 0})

	run("kafka task without CreateTime", kafkaWriteTaskAt(t, w, 9722, 10, time.Time{}), sample{queue: 0, total: 1, e2e: 0})

	run("kafka task with a producer clock ahead of ours", kafkaWriteTaskAt(t, w, 9723, 10, now.Add(time.Hour)),
		sample{queue: 0, total: 1, e2e: 0})
}
