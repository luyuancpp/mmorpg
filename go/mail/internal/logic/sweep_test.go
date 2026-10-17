package logic

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/zeromicro/go-zero/core/logx/logtest"

	"mail/internal/config"
	"mail/internal/data"
	"mail/internal/metrics"
)

// fakeSweeps 记录 Sweep 的调用参数并返回预设读数。
type fakeSweeps struct {
	calls      int
	cutoff     uint64
	batch      int
	deleteRows bool
	report     data.SweepReport
	err        error
}

func (f *fakeSweeps) Sweep(_ context.Context, cutoff uint64, batch int, deleteRows bool) (data.SweepReport, error) {
	f.calls++
	f.cutoff, f.batch, f.deleteRows = cutoff, batch, deleteRows
	return f.report, f.err
}

func sweepDeps(mode string, sweeps *fakeSweeps) *Deps {
	deps := testDeps(newFakeStore(), "dev")
	deps.Config.Sweep.Mode = mode
	deps.Sweeps = sweeps
	return deps
}

func TestSweepCutoff(t *testing.T) {
	now := time.UnixMilli(1_800_000_000_000)
	assert.Equal(t, uint64(1_800_000_000_000-7*24*3600*1000), sweepCutoffMs(now, 7))
	assert.Zero(t, sweepCutoffMs(time.UnixMilli(1000), 7), "now ≤ 保留期 → 0(本轮什么都不做,不和无符号列比负数)")
	assert.Zero(t, sweepCutoffMs(time.UnixMilli(0), 1))
}

func TestSweepRoundTimeout(t *testing.T) {
	assert.Equal(t, 10*time.Second, sweepRoundTimeout(10*time.Second), "比节拍还长会两轮叠在一起")
	assert.Equal(t, sweepRoundBudget, sweepRoundTimeout(5*time.Minute))
	assert.Equal(t, sweepRoundBudget, sweepRoundTimeout(0))
}

func TestRunSweepRoundModes(t *testing.T) {
	logtest.Discard(t)
	for _, tc := range []struct {
		mode       string
		wantCall   bool
		wantDelete bool
	}{
		{config.SweepModeReportOnly, true, false},
		{config.SweepModeDelete, true, true},
		{"Delete", false, false}, // 大小写不符 = 未知模式:一行不删,报错
		{"", false, false},
	} {
		sweeps := &fakeSweeps{}
		sweepDeps(tc.mode, sweeps).runSweepRound(context.Background())
		assert.Equal(t, tc.wantCall, sweeps.calls == 1, "mode=%q", tc.mode)
		if tc.wantCall {
			assert.Equal(t, tc.wantDelete, sweeps.deleteRows, "mode=%q", tc.mode)
			assert.Equal(t, sweepCutoffMs(testNow, 7), sweeps.cutoff)
			assert.Equal(t, 1000, sweeps.batch)
		}
	}
}

// 每轮都写 last_run(含失败、含未知模式):它停止前进 = 循环卡死(告警 MailSweepStalled)。
func TestRunSweepRoundAlwaysMarksLastRun(t *testing.T) {
	logtest.Discard(t)
	for i, tc := range []struct {
		mode   string
		sweeps *fakeSweeps
	}{
		{config.SweepModeReportOnly, &fakeSweeps{}},
		{config.SweepModeReportOnly, &fakeSweeps{err: errBoom}},
		{"Delete", &fakeSweeps{}}, // 未知模式:一行不删,但这一轮照样算"跑过"
	} {
		deps := sweepDeps(tc.mode, tc.sweeps)
		later := testNow.Add(time.Duration(i+1) * time.Hour) // 每轮一个新时刻:证明失败的那一轮也把它往前推了
		deps.Now = func() time.Time { return later }
		deps.runSweepRound(context.Background())
		assert.Equal(t, float64(later.Unix()), counterValue(t, "mail_sweep_last_run_unixtime", nil))
	}
}

// 读数照实进指标:候选 gauge 每轮写(含 0),实删 / 坏数据累加。
func TestRunSweepRoundObservesReport(t *testing.T) {
	logtest.Discard(t)
	sweeps := &fakeSweeps{report: data.SweepReport{
		Personal:    data.SweepTableStats{Candidates: 3, Deleted: 3, Invalid: 1},
		System:      data.SweepTableStats{Candidates: 2, Deleted: 2},
		SystemState: data.SweepTableStats{Candidates: 5, Deleted: 5},
	}}
	personal := map[string]string{"table": "mail_personal"}
	deletedBefore := counterValue(t, "mail_sweep_deleted_total", personal)
	invalidBefore := counterValue(t, "mail_sweep_invalid_rows_total", personal)

	sweepDeps(config.SweepModeDelete, sweeps).runSweepRound(context.Background())

	assert.Equal(t, deletedBefore+3, counterValue(t, "mail_sweep_deleted_total", personal))
	assert.Equal(t, invalidBefore+1, counterValue(t, "mail_sweep_invalid_rows_total", personal))
	assert.Equal(t, float64(5), counterValue(t, "mail_sweep_candidate_rows", map[string]string{"table": "mail_system_state", "mode": "delete"}))

	// 下一轮没有候选:gauge 被刷回 0,而不是停在上一轮的值。
	sweeps.report = data.SweepReport{}
	sweepDeps(config.SweepModeDelete, sweeps).runSweepRound(context.Background())
	assert.Zero(t, counterValue(t, "mail_sweep_candidate_rows", map[string]string{"table": "mail_system_state", "mode": "delete"}))
}

func TestStartSweepRefusesBadWiring(t *testing.T) {
	logtest.Discard(t)
	sentinel := time.Unix(123, 0)
	started := testNow.Add(time.Hour)

	deps := testDeps(newFakeStore(), "dev") // Sweeps 留空(nil 接口,不是装着 nil 指针的接口)
	deps.Now = func() time.Time { return started }
	metrics.MarkSweepRun(sentinel)
	StartSweep(context.Background(), deps) // 不 panic、不起循环
	assert.Equal(t, float64(sentinel.Unix()), counterValue(t, "mail_sweep_last_run_unixtime", nil), "没接 SweepStore 时不该启动")

	deps = sweepDeps(config.SweepModeReportOnly, &fakeSweeps{})
	deps.Config.Sweep.Interval = 0
	deps.Now = func() time.Time { return started }
	StartSweep(context.Background(), deps) // 0 间隔的 ticker 会 panic:必须拒绝启动
	assert.Equal(t, float64(sentinel.Unix()), counterValue(t, "mail_sweep_last_run_unixtime", nil), "Interval=0 时不该启动")

	// 正常接线:启动即写一次 last_run(免得重启后、首轮跑完之前 MailSweepStalled 误报)。
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	deps = sweepDeps(config.SweepModeReportOnly, &fakeSweeps{})
	deps.Config.Sweep.Interval = time.Hour // 首轮抖动在 [0,1h) 内,本用例结束前不会跑到
	deps.Now = func() time.Time { return started }
	StartSweep(ctx, deps)
	assert.Equal(t, float64(started.Unix()), counterValue(t, "mail_sweep_last_run_unixtime", nil))
}
