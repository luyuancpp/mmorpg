package kafka

// luaMarkAppliedEpoch 的十进制串比较回归。miniredis 的 Lua(gopher-lua)数字同样是 double,旧脚本的
// tonumber 比较在这里能复现「大于 2^53 推不进去」的缺陷:下面标了「旧脚本红」的用例在旧实现下失败。

import (
	"context"
	"math"
	"strconv"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func newAppliedEpochLuaFixture(t *testing.T) (*miniredis.Miniredis, *redis.Client, redisAppliedEpochStore) {
	t.Helper()
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	return mr, rc, redisAppliedEpochStore{rc: rc}
}

func TestMarkAppliedEpoch_DecimalCompareBeyondDoublePrecision(t *testing.T) {
	const topic, msgType = "db_task_zone_1", "taskpb.TaskResult"
	cases := []struct {
		name    string
		stored  string // "" = 无键
		mark    uint64
		want    string // "" = 仍无键
	}{
		{name: "no key: first mark is stored", stored: "", mark: 7, want: "7"},
		{name: "2^53 then 2^53+1 advances (old script red)", stored: "9007199254740992", mark: 9007199254740993, want: "9007199254740993"},
		{name: "MaxUint64-1 then MaxUint64 advances (old script red)", stored: "18446744073709551614", mark: math.MaxUint64, want: "18446744073709551615"},
		{name: "2^53+1 then 2^53 does not regress", stored: "9007199254740993", mark: 9007199254740992, want: "9007199254740993"},
		{name: "10 then 9 does not regress (a plain string compare would)", stored: "10", mark: 9, want: "10"},
		{name: "9 then 10 advances (a plain string compare would not)", stored: "9", mark: 10, want: "10"},
		{name: "equal is a no-op", stored: "42", mark: 42, want: "42"},
		{name: "zero on no key stays absent", stored: "", mark: 0, want: ""},
		{name: "zero on an existing key does not regress", stored: "5", mark: 0, want: "5"},
	}
	for i, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			mr, _, store := newAppliedEpochLuaFixture(t)
			key := uint64(9900 + i)
			redisKey := appliedEpochKey(topic, key, msgType)
			if tc.stored != "" {
				require.NoError(t, mr.Set(redisKey, tc.stored))
			}

			require.NoError(t, store.MarkAppliedEpoch(context.Background(), topic, key, msgType, tc.mark))

			if tc.want == "" {
				assert.False(t, mr.Exists(redisKey), "marking epoch 0 must not create the key")
				return
			}
			got, err := mr.Get(redisKey)
			require.NoError(t, err)
			assert.Equal(t, tc.want, got)
			epoch, found, err := store.ReadAppliedEpoch(context.Background(), topic, key, msgType)
			require.NoError(t, err)
			assert.True(t, found)
			assert.Equal(t, tc.want, strconv.FormatUint(epoch, 10), "the stored value still round-trips through ReadAppliedEpoch")
		})
	}
}

// 存量值不是规范十进制(人手改坏、带前导 0):Mark 返回错误、不改值;ReadAppliedEpoch 的既有语义不变
// (abc 报错,007 仍按 7 读出)。
func TestMarkAppliedEpoch_RejectsNonCanonicalStoredValue(t *testing.T) {
	const topic, msgType = "db_task_zone_1", "taskpb.TaskResult"
	for i, stored := range []string{"abc", "007"} {
		t.Run(stored, func(t *testing.T) {
			mr, _, store := newAppliedEpochLuaFixture(t)
			key := uint64(9950 + i)
			redisKey := appliedEpochKey(topic, key, msgType)
			require.NoError(t, mr.Set(redisKey, stored))

			require.Error(t, store.MarkAppliedEpoch(context.Background(), topic, key, msgType, 8))
			got, err := mr.Get(redisKey)
			require.NoError(t, err)
			assert.Equal(t, stored, got, "a rejected mark must not touch the stored value")

			epoch, found, err := store.ReadAppliedEpoch(context.Background(), topic, key, msgType)
			if stored == "abc" {
				assert.Error(t, err)
				return
			}
			require.NoError(t, err)
			assert.True(t, found)
			assert.Equal(t, uint64(7), epoch)
		})
	}
}

// 参数不是规范十进制时脚本直接报错(生产只传 strconv.FormatUint,不会走到;钉住脚本自身的入参校验)。
func TestMarkAppliedEpoch_RejectsNonCanonicalArgument(t *testing.T) {
	mr, rc, _ := newAppliedEpochLuaFixture(t)
	const redisKey = "consumer:applied_epoch:db_task_zone_1:9990:taskpb.TaskResult"
	require.NoError(t, mr.Set(redisKey, "5"))
	for _, arg := range []string{"07", "-1", "1.5", ""} {
		err := rc.Eval(context.Background(), luaMarkAppliedEpoch, []string{redisKey}, arg).Err()
		assert.Errorf(t, err, "argument %q must be rejected", arg)
	}
	got, err := mr.Get(redisKey)
	require.NoError(t, err)
	assert.Equal(t, "5", got)
}
