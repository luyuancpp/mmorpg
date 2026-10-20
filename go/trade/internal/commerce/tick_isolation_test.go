package commerce

// tick_isolation_test.go —— Tick 单行隔离的纯函数部分(2026-10-20,第一轮审计 §2.5 go-asset-services#0),不连库。
// 端到端行为(队头失败不挡其余、坏数据隔离、持有竞价不占队头、ACK 翻页)在 tick_isolation_db_test.go 的真库用例里。

import (
	"database/sql"
	"errors"
	"fmt"
	"testing"

	"shared/assetop"

	"github.com/stretchr/testify/require"
)

func TestTickFailureClassification(t *testing.T) {
	pending := fmt.Errorf("推进交易合同: %w", fmt.Errorf("%w (player=1 stream=3 pending=16)", assetop.ErrTooManyPending))
	require.Equal(t, failureTooManyPending, failureClass(pending))
	require.Equal(t, failureRetry, failureClass(errors.New("i/o timeout")))
	bad := corrupt(sql.ErrNoRows)
	require.Equal(t, failureCorrupt, failureClass(bad))
	require.ErrorIs(t, bad, sql.ErrNoRows, "corrupt 包装不能吞掉原错误")
	require.Equal(t, failureCorrupt, failureClass(fmt.Errorf("推进交易合同: %w", corrupt(pending))),
		"坏数据优先于暂时失败:否则会按暂时失败无限重试")
}

// 推迟必须有界、带抖动,且积压背压(ErrTooManyPending)比一般暂时失败退得更远。
func TestTickFailureBackoffIsBoundedAndJittered(t *testing.T) {
	const now = uint64(1_800_000_000_000)
	pending := fmt.Errorf("%w (player=1 stream=3 pending=16)", assetop.ErrTooManyPending)
	cases := []struct {
		name     string
		cause    error
		min, max uint64
	}{
		{"积压背压", pending, uint64(tickPendingBackoff.Milliseconds()) * 8 / 10, uint64(tickPendingBackoff.Milliseconds()) * 12 / 10},
		{"一般暂时失败", errors.New("store timeout"), uint64(tickRetryBackoff.Milliseconds()) * 8 / 10, uint64(tickRetryBackoff.Milliseconds()) * 12 / 10},
	}
	for _, c := range cases {
		var seen []uint64
		for _, r := range []float64{0, 0.5, 0.999999} {
			delay := tickFailureBackoff(now, c.cause, func() float64 { return r }) - now
			require.GreaterOrEqual(t, delay, c.min, c.name)
			require.LessOrEqual(t, delay, c.max, c.name)
			seen = append(seen, delay)
		}
		require.Less(t, seen[0], seen[2], "%s:抖动必须生效,多行同时失败不能在同一毫秒一起回来", c.name)
	}
	require.Greater(t, tickFailureBackoff(now, pending, nil), tickFailureBackoff(now, errors.New("x"), nil))
}
