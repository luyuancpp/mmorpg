package kafka

import (
	"context"
	"testing"

	db_proto "proto/db"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// 子 shard 路由(subShardIndex / routeToSubShard)的回归用例。
//
// 背景:存量 PlayerId 是 bwmarrin 布局(41 位毫秒 | 13 位 node | 9 位 step),低频建角时
// step 几乎全为 0。旧实现直接 key % SubShardCount,这批玩家全部挤进 0 号子 shard。

// bwmarrinPlayerID 按 go/login/etc/login.yaml 的 Snowflake 段(NodeBits 13 / StepBits 9)拼一个 PlayerId。
func bwmarrinPlayerID(msSinceEpoch, node, step uint64) uint64 {
	const stepBits, nodeBits = 9, 13
	return msSinceEpoch<<(nodeBits+stepBits) | node<<stepBits | step
}

// 与 C++ SpreadPlayerId 共用的 splitmix64 终混步,用公开的 splitmix64(seed=0) 输出序列对拍:
// spreadKey(k·γ) 就是 seed=0 的第 k+1 个输出。
func TestSpreadKeyMatchesSplitMix64Reference(t *testing.T) {
	cases := []struct {
		in, want uint64
	}{
		{0, 0xe220a8397b1dcdaf},
		{0x9e3779b97f4a7c15, 0x6e789e6aa1b965f4}, // γ
		{0x3c6ef372fe94f82a, 0x06c45d188009454f}, // 2γ(uint64 回绕后)
		{1, 0x910a2dec89025cc1},
	}
	for _, c := range cases {
		assert.Equalf(t, c.want, spreadKey(c.in), "spreadKey(%#x)", c.in)
	}
}

func TestSubShardIndexIsStablePerKey(t *testing.T) {
	keys := []uint64{0, 1, 7, 8, 1 << 40, bwmarrinPlayerID(123456789, 3, 0), ^uint64(0)}
	for _, k := range keys {
		first := subShardIndex(k, 8)
		require.GreaterOrEqual(t, first, 0)
		require.Less(t, first, 8)
		for i := 0; i < 3; i++ {
			assert.Equalf(t, first, subShardIndex(k, 8), "key %#x must always map to the same sub-shard", k)
		}
	}
}

// 低频建角:每 5ms 建一个角色、step 恒为 0。旧实现全部落 0 号子 shard,新实现要大致均分。
func TestSubShardIndexSpreadsBwmarrinIDsWithZeroStep(t *testing.T) {
	const shardCount = 8
	const n = 4096
	counts := make([]int, shardCount)
	for i := uint64(0); i < n; i++ {
		id := bwmarrinPlayerID(1_000_000+i*5, 7, 0)
		require.Equal(t, uint64(0), id%shardCount, "precondition: raw modulo would pin every such id to shard 0")
		counts[subShardIndex(id, shardCount)]++
	}
	expected := n / shardCount
	for shard, got := range counts {
		assert.GreaterOrEqualf(t, got, expected/2, "shard %d starved: counts=%v", shard, counts)
		assert.LessOrEqualf(t, got, expected*3/2, "shard %d overloaded: counts=%v", shard, counts)
	}
}

// 号段发放的连续 ID(2026-09-08 起的新角色)同样要均匀。
func TestSubShardIndexSpreadsSequentialIDs(t *testing.T) {
	const shardCount = 8
	const n = 4096
	counts := make([]int, shardCount)
	for i := uint64(1); i <= n; i++ {
		counts[subShardIndex(i, shardCount)]++
	}
	expected := n / shardCount
	for shard, got := range counts {
		assert.GreaterOrEqualf(t, got, expected/2, "shard %d starved: counts=%v", shard, counts)
		assert.LessOrEqualf(t, got, expected*3/2, "shard %d overloaded: counts=%v", shard, counts)
	}
}

// routeToSubShard 必须按 subShardIndex 投递,而不是自己再取一次模。
func TestRouteToSubShardUsesSpreadIndex(t *testing.T) {
	const shardCount = 8
	w := &worker{
		ctx:           context.Background(),
		subShardCount: shardCount,
		subShardChans: make([]chan *workerTask, shardCount),
	}
	for i := range w.subShardChans {
		w.subShardChans[i] = make(chan *workerTask, 1)
	}

	key := bwmarrinPlayerID(987654321, 11, 0)
	task := &workerTask{dbTask: &db_proto.DBTask{Key: key}}
	w.routeToSubShard(task)

	want := subShardIndex(key, shardCount)
	for i, ch := range w.subShardChans {
		if i == want {
			require.Len(t, ch, 1, "task must land in sub-shard %d", want)
			assert.Same(t, task, <-ch)
			continue
		}
		assert.Lenf(t, ch, 0, "sub-shard %d must stay empty", i)
	}
}
