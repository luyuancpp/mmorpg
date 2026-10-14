package kafkautil

import (
	"errors"
	"fmt"
	"strings"
	"testing"

	"github.com/IBM/sarama"
)

// groupOffsetFake 是 groupOffsetReader 的假实现:按分区给 committed / start / end,并记下调用顺序。
type groupOffsetFake struct {
	committed map[int32]int64 // 分区 → 已提交 offset(-1 = 没提交过);不在 map 里 = 响应里没有这个 block
	blockErr  map[int32]sarama.KError
	start     map[int32]int64
	end       map[int32]int64

	topErr    sarama.KError // 响应顶层 Err(OffsetFetch v2+)
	listErr   error         // ListConsumerGroupOffsets 调用本身失败
	offsetErr error         // GetOffset 调用失败

	calls []string
}

func (f *groupOffsetFake) ListConsumerGroupOffsets(group string, tps map[string][]int32) (*sarama.OffsetFetchResponse, error) {
	f.calls = append(f.calls, "list")
	if f.listErr != nil {
		return nil, f.listErr
	}
	resp := &sarama.OffsetFetchResponse{Err: f.topErr, Blocks: map[string]map[int32]*sarama.OffsetFetchResponseBlock{}}
	for topic, partitions := range tps {
		resp.Blocks[topic] = map[int32]*sarama.OffsetFetchResponseBlock{}
		for _, p := range partitions {
			c, ok := f.committed[p]
			if !ok {
				continue
			}
			resp.Blocks[topic][p] = &sarama.OffsetFetchResponseBlock{Offset: c, Err: f.blockErr[p]}
		}
	}
	return resp, nil
}

func (f *groupOffsetFake) GetOffset(_ string, partition int32, at int64) (int64, error) {
	f.calls = append(f.calls, fmt.Sprintf("offset:%d", partition))
	if f.offsetErr != nil {
		return 0, f.offsetErr
	}
	switch at {
	case sarama.OffsetNewest:
		return f.end[partition], nil
	case sarama.OffsetOldest:
		return f.start[partition], nil
	}
	return 0, fmt.Errorf("unexpected offset query %d", at)
}

// TestComputeGroupLag 覆盖 lag 探针的算法与 fail-closed 口径:
//   - 正常 / 没提交过(-1,从日志起点算)/ 已提交位点早于日志起点(保留期删掉了未消费的消息 = RetentionGap);
//   - MaxPartitionLag 是单分区最大值而不是合计(broker 的 retention.bytes 按分区执行,积压告警比的是它);
//   - committed > end(topic 被删了重建)、某分区 block 带错 / 缺失、响应顶层 Err、调用本身失败 —— 一律报错,不静默当 0;
//   - 调用顺序:先读组的已提交 offset,再读任何分区的 end / start。反过来时消费者在两次读之间提交一次
//     就会得到 committed > end,繁忙 topic 上探针会周期性误报。
func TestComputeGroupLag(t *testing.T) {
	const group, topic = "g", "player_snapshot_topic_g1"

	t.Run("正常", func(t *testing.T) {
		f := &groupOffsetFake{
			committed: map[int32]int64{0: 10, 1: 20, 2: 30},
			start:     map[int32]int64{0: 0, 1: 0, 2: 0},
			end:       map[int32]int64{0: 15, 1: 20, 2: 40},
		}
		got, err := computeGroupLag(f, group, topic, 3)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if got != (GroupLag{Lag: 15, MaxPartitionLag: 10}) {
			t.Fatalf("got %+v, want Lag=15 (5+0+10), MaxPartitionLag=10, RetentionGap=0, Uncommitted=0", got)
		}
		if len(f.calls) == 0 || f.calls[0] != "list" {
			t.Fatalf("committed offsets must be read before any partition offset, calls=%v", f.calls)
		}
		for _, c := range f.calls[1:] {
			if c == "list" {
				t.Fatalf("committed offsets must be read exactly once, first, calls=%v", f.calls)
			}
		}
	})

	t.Run("没提交过的分区从日志起点算", func(t *testing.T) {
		f := &groupOffsetFake{
			committed: map[int32]int64{0: -1, 1: 8},
			start:     map[int32]int64{0: 5, 1: 0},
			end:       map[int32]int64{0: 12, 1: 10},
		}
		got, err := computeGroupLag(f, group, topic, 2)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if got != (GroupLag{Lag: 7 + 2, MaxPartitionLag: 7, Uncommitted: 1}) {
			t.Fatalf("got %+v, want Lag=9 MaxPartitionLag=7 (the uncommitted partition counts too) Uncommitted=1 RetentionGap=0", got)
		}
	})

	t.Run("积压集中在一个分区时 MaxPartitionLag 取单分区最大值", func(t *testing.T) {
		f := &groupOffsetFake{
			committed: map[int32]int64{0: 100, 1: 100, 2: 0},
			start:     map[int32]int64{0: 0, 1: 0, 2: 0},
			end:       map[int32]int64{0: 100, 1: 101, 2: 1750},
		}
		got, err := computeGroupLag(f, group, topic, 3)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if got != (GroupLag{Lag: 1751, MaxPartitionLag: 1750}) {
			t.Fatalf("got %+v, want Lag=1751 (0+1+1750) MaxPartitionLag=1750 (retention is enforced per partition)", got)
		}
	})

	t.Run("已提交位点早于日志起点 = 保留期已删掉未消费的消息", func(t *testing.T) {
		f := &groupOffsetFake{
			committed: map[int32]int64{0: 3},
			start:     map[int32]int64{0: 10},
			end:       map[int32]int64{0: 25},
		}
		got, err := computeGroupLag(f, group, topic, 1)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if got != (GroupLag{Lag: 15, MaxPartitionLag: 15, RetentionGap: 7}) {
			t.Fatalf("got %+v, want Lag=15 MaxPartitionLag=15 (end-start) RetentionGap=7 (start-committed)", got)
		}
	})

	t.Run("committed 越过 end 必须报错", func(t *testing.T) {
		f := &groupOffsetFake{
			committed: map[int32]int64{0: 50, 1: 5},
			start:     map[int32]int64{0: 0, 1: 0},
			end:       map[int32]int64{0: 20, 1: 9},
		}
		got, err := computeGroupLag(f, group, topic, 2)
		if err == nil || !strings.Contains(err.Error(), "beyond the log end") {
			t.Fatalf("committed > end must be reported, got err=%v", err)
		}
		if got.Lag != 4 || got.MaxPartitionLag != 4 {
			t.Fatalf("the recreated partition counts 0 lag, the healthy one still counts: got %+v", got)
		}
	})

	for _, tc := range []struct {
		name string
		f    *groupOffsetFake
		n    int32
	}{
		{"分区数非法", &groupOffsetFake{}, 0},
		{"block 带错", &groupOffsetFake{
			committed: map[int32]int64{0: 1},
			blockErr:  map[int32]sarama.KError{0: sarama.ErrUnknownTopicOrPartition},
			start:     map[int32]int64{0: 0},
			end:       map[int32]int64{0: 1},
		}, 1},
		{"block 缺失", &groupOffsetFake{
			committed: map[int32]int64{},
			start:     map[int32]int64{0: 0},
			end:       map[int32]int64{0: 1},
		}, 1},
		{"响应顶层 Err", &groupOffsetFake{
			committed: map[int32]int64{0: 1},
			topErr:    sarama.ErrNotCoordinatorForConsumer,
			start:     map[int32]int64{0: 0},
			end:       map[int32]int64{0: 1},
		}, 1},
		{"ListConsumerGroupOffsets 失败", &groupOffsetFake{listErr: errors.New("broker down")}, 1},
		{"GetOffset 失败", &groupOffsetFake{
			committed: map[int32]int64{0: 1},
			offsetErr: errors.New("leader not available"),
		}, 1},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if _, err := computeGroupLag(tc.f, group, topic, tc.n); err == nil {
				t.Fatal("must return an error, not a silent zero")
			}
		})
	}
}
