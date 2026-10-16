package logic

// sweep.go —— 过期邮件物理清理的节拍侧(设计 §4.6;SQL 侧与删除顺序在 internal/data/sweep_repo.go)。
//
// 本文件只负责:节拍(ticker + 首轮抖动)、panic 边界(safego)、单轮预算、截止点计算、指标与日志。
// 删什么、按什么顺序删、expire_ms = 0 的行只数不删,都在 SQL 侧。
//
// 多副本各跑各的,不引入 leader:DELETE … LIMIT 天然可并发,重复删 = 0 行(照 friend)。
// 调用方是 go/mail/mail.go 的 runMail(metrics.Start 之后),ctx 是 signal.NotifyContext 的那个:
// 收到 SIGTERM 循环自行退出;停机**不等它**(被打断只是少删一批,下一轮接着删,不挤占 24s 硬预算)。
//
// ⚠ 默认 Sweep.Mode = report_only:只统计 + 打 WARN + 刷 gauge,一行都不删。真删必须显式改配置成 delete。

import (
	"context"
	"math/rand/v2"
	"time"

	"github.com/zeromicro/go-zero/core/logx"

	"mail/internal/config"
	"mail/internal/data"
	"mail/internal/metrics"

	"shared/safego"
)

// sweepPoint 是 safego 的点位名,会变成 Prometheus label safego_panic_total{point=...}:编译期常量,改它等于改告警规则。
const sweepPoint = "mail.expired_sweep"

// sweepRoundBudget 是单轮清理的上限(设计 §4.6:30s)。跑不完的留给下一轮,不丢任何东西。
const sweepRoundBudget = 30 * time.Second

// StartSweep 起后台清理循环,立即返回;ctx 结束时循环自行退出。
// 首轮在 [0, Interval) 的随机抖动之后**立即**跑(多副本同时重启时不要在同一秒一起对 mail 表发 DELETE),之后每 Interval 一轮。
//
// 启动时先写一次 mail_sweep_last_run_unixtime:它是告警 MailSweepStalled(time() − max(...) > 3 × Interval)的唯一输入,
// 注册时是 0,不先写的话每次全量重启后、首轮跑完之前,这条告警都会误报。
func StartSweep(ctx context.Context, deps *Deps) {
	cfg := deps.Config.Sweep
	if deps.Sweeps == nil {
		logx.Errorf("[mail] SweepStore not wired; sweep not started (expired mail will never be physically deleted)")
		return
	}
	// config.Validate 拒收 Interval <= 0;走到这里说明校验被绕过。0 间隔的 ticker 会 panic,宁可不启动并报出来。
	if cfg.Interval <= 0 {
		logx.Errorf("[mail] Invalid sweep Interval=%v; sweep not started", cfg.Interval)
		return
	}
	logx.Infof("[mail] Sweep started mode=%s interval=%v retention_days=%d batch=%d",
		cfg.Mode, cfg.Interval, cfg.RetentionDays, cfg.BatchLimit)
	metrics.MarkSweepRun(deps.now())

	safego.Go(sweepPoint+".start", func() {
		jitter := time.Duration(rand.Int64N(int64(cfg.Interval)))
		select {
		case <-ctx.Done():
			return
		case <-time.After(jitter):
		}
		// 首轮立即跑:safego.Loop 要先等满一个节拍才跑第一轮。recover 同样精确到这一轮。
		safego.Run(sweepPoint, func() { deps.runSweepRound(ctx) })
		// safego.Loop 的 recover 精确到"一轮":某一轮 panic 只丢那一轮,节拍继续。
		safego.Loop(ctx, sweepPoint, cfg.Interval, deps.runSweepRound)
	})
}

// runSweepRound 跑一轮清理。失败只打日志:后台任务,一轮失败等下一轮。
//
// mail_sweep_last_run_unixtime 每轮结束都写(含失败、含 0 行):它停止前进 = 循环卡死或停止调度
// (告警 MailSweepStalled)。失败本身由 ERROR 日志表达,不由这个 gauge 表达。
func (d *Deps) runSweepRound(ctx context.Context) {
	cfg := d.Config.Sweep
	defer func() { metrics.MarkSweepRun(d.now()) }()

	deleteRows, known := sweepDeletes(cfg.Mode)
	if !known {
		// config.Validate 本该拒掉。未知模式一行都不删,表现与 report_only 一样 —— 必须报出来,否则会以为清理在跑。
		logx.WithContext(ctx).Errorf("[mail] Sweep mode %q is neither %q nor %q; nothing cleaned this round",
			cfg.Mode, config.SweepModeReportOnly, config.SweepModeDelete)
		return
	}
	cutoff := sweepCutoffMs(d.now(), cfg.RetentionDays)
	if cutoff == 0 {
		observeSweepReport(cfg.Mode, data.SweepReport{}) // 每轮都刷候选 gauge(这一轮确实是 0 行)
		// 时钟或配置异常(now ≤ 保留期):无符号列与负数比较会匹配全表,这一轮什么都不做(friend sweepCutoffMs 的教训)。
		logx.WithContext(ctx).Errorf("[mail] Sweep cutoff <= 0 (now=%v retention_days=%d); nothing cleaned this round", d.now(), cfg.RetentionDays)
		return
	}

	ctx, cancel := context.WithTimeout(ctx, sweepRoundTimeout(cfg.Interval))
	defer cancel()
	report, err := d.Sweeps.Sweep(ctx, cutoff, cfg.BatchLimit, deleteRows)
	// 出错时 report 里是出错前已经得到的读数:前面几步删掉的行是真删了,要如实上报。
	observeSweepReport(cfg.Mode, report)
	if err != nil {
		logx.WithContext(ctx).Errorf("[mail] Sweep failed mode=%s cutoff_ms=%d (personal deleted=%d, system deleted=%d, state deleted=%d before abort): %v",
			cfg.Mode, cutoff, report.Personal.Deleted, report.System.Deleted, report.SystemState.Deleted, err)
		return
	}
	logSweepReport(ctx, cfg, cutoff, report)
}

// sweepDeletes 把配置的模式映射成"删不删";known=false 表示模式非法。
func sweepDeletes(mode string) (deleteRows, known bool) {
	switch mode {
	case config.SweepModeDelete:
		return true, true
	case config.SweepModeReportOnly:
		return false, true
	default:
		return false, false
	}
}

// sweepCutoffMs = now − RetentionDays(毫秒),用有符号运算;结果 ≤ 0 返回 0(Sweep 对 0 什么都不做)。
func sweepCutoffMs(now time.Time, retentionDays int) uint64 {
	cutoff := now.UnixMilli() - int64(retentionDays)*int64(24*time.Hour/time.Millisecond)
	if cutoff <= 0 {
		return 0
	}
	return uint64(cutoff)
}

// sweepRoundTimeout 取单轮预算:不超过 sweepRoundBudget,也不超过一个 ticker 间隔(否则两轮会叠在一起压库)。
func sweepRoundTimeout(interval time.Duration) time.Duration {
	if interval > 0 && interval < sweepRoundBudget {
		return interval
	}
	return sweepRoundBudget
}

// observeSweepReport 刷 sweep 指标。候选 gauge 每轮都写(包括 0):只在 >0 时写会让"没有积压"与"循环死了"长得一样。
func observeSweepReport(mode string, r data.SweepReport) {
	for _, t := range []struct {
		table string
		stats data.SweepTableStats
	}{
		{metrics.TablePersonal, r.Personal},
		{metrics.TableSystem, r.System},
		{metrics.TableSystemState, r.SystemState},
	} {
		metrics.SetSweepCandidateRows(t.table, mode, float64(t.stats.Candidates))
		metrics.AddSweepDeleted(t.table, float64(t.stats.Deleted))
		metrics.AddSweepInvalidRows(t.table, float64(t.stats.Invalid))
	}
}

// logSweepReport 打一轮的结论。go-zero 的 logx 没有 Warn 级,按仓内先例用 Errorf 打 "WARN " 前缀。
func logSweepReport(ctx context.Context, cfg config.SweepConf, cutoff uint64, r data.SweepReport) {
	if invalid := r.Personal.Invalid + r.System.Invalid; invalid > 0 {
		// expire_ms = 0 的行不应存在(发件校验要求 expire_ms > sent_ms):有人绕过了发件路径直接写库。只数不删(fail-closed)。
		logx.WithContext(ctx).Errorf("[mail] Sweep found rows with expire_ms = 0 (personal=%d, system=%d); they are never deleted automatically — send-side validation was bypassed (mail_sweep_invalid_rows_total / MailSweepInvalidRows)",
			r.Personal.Invalid, r.System.Invalid)
	}
	candidates := r.Personal.Candidates + r.System.Candidates + r.SystemState.Candidates
	deleted := r.Personal.Deleted + r.System.Deleted + r.SystemState.Deleted
	switch {
	case cfg.Mode == config.SweepModeDelete && deleted > 0:
		logx.WithContext(ctx).Infof("[mail] Sweep deleted personal=%d system=%d system_state=%d (cutoff_ms=%d retention_days=%d batch=%d)",
			r.Personal.Deleted, r.System.Deleted, r.SystemState.Deleted, cutoff, cfg.RetentionDays, cfg.BatchLimit)
	case cfg.Mode == config.SweepModeReportOnly && candidates > 0:
		logx.WithContext(ctx).Errorf("[mail] WARN sweep(report_only) found expired rows beyond retention: personal=%d system=%d system_state=%d (each capped at %d; cutoff_ms=%d retention_days=%d); switch Sweep.Mode to delete after confirming the counts",
			r.Personal.Candidates, r.System.Candidates, r.SystemState.Candidates, cfg.BatchLimit, cutoff, cfg.RetentionDays)
	}
}
