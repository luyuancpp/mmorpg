package data

import (
	"context"
	"fmt"
)

// sweep_repo.go —— 过期邮件的物理清理(设计 docs/design/mail-system.md §4.6)。sweep 是 mail 表**唯一**的物理删除者。
//
// # 顺序是正确性的一部分
//
//  1. 定向邮件:DELETE … WHERE expire_ms > 0 AND expire_ms < cutoff AND claim_state IN (0,1) LIMIT n。
//     已软删的行同样只按 expire_ms 判(软删不缩短寿命:唯一键在这一行上,它最短要活 expire + RetentionDays,§2.4)。
//     ⚠ M2 起扩成 IN (0,1,3),**刻意不含 2(CLAIMING)与 4(NEEDS_MANUAL)**:它们挂着未终结的资产 op,删了就断了对账线索。
//     写成 IN 而不是 <>(friend 的索引经验)。
//  2. 系统邮件:**先删物化行再删系统行**。反过来会留下无主的 state 行,而它们唯一的清理索引就是 sys_mail_id ——
//     系统行没了之后再也没人会按这个 id 去扫,孤儿永久残留。按本顺序,中途崩溃只会留下"系统行还在、state 行删了一半",
//     下一轮继续删。与 ReadMail / DeleteMails 的条件 upsert 交错也不会生出孤儿:upsert 只认 expire_ms > now 的系统行,
//     sweep 只删 expire_ms < now − RetentionDays(RetentionDays ≥ 1)的系统行,两个集合不相交(§4.2)。
//     ⚠ M2 起系统邮件的 state 行若有 claim_state IN (2,4),整封系统邮件本轮跳过。
//  3. expire_ms = 0 的行(不应存在:发件校验要求 expire_ms > sent_ms)**只统计不删**,由调用方打 ERROR 并计
//     mail_sweep_invalid_rows_total —— fail-closed 的保险,不让一条坏数据被当成"早已过期"删掉。
//
// 计数一律"派生表 LIMIT 的有界 COUNT",删除一律单条自动提交的 DELETE … LIMIT(前提 binlog_format=ROW)。
// 多副本各跑各的,不选主:DELETE … LIMIT 天然可并发,重复删 = 0 行。

// SweepTableStats 是一张表在一轮 sweep 里的读数。Candidates / Invalid 是有界计数(最多 batchLimit),
// 等于 batchLimit 只说明"至少这么多",不是精确值。
type SweepTableStats struct {
	Candidates int64
	Deleted    int64
	Invalid    int64
}

// SweepReport 是一轮 sweep 的全部读数(指标 mail_sweep_* 的来源)。
type SweepReport struct {
	Personal    SweepTableStats
	System      SweepTableStats
	SystemState SweepTableStats
}

// Sweep 跑一轮清理。cutoffMs = now − RetentionDays;deleteRows=false 即 report_only:只计数,不删。
//
// cutoffMs == 0 本轮什么都不做:调用方用有符号运算算出 cutoff ≤ 0 时传 0(无符号列与负数比较会匹配全表,
// friend sweepCutoffMs 的教训)。batchLimit ≤ 0 视为调用方 bug,直接报错 —— 不带 LIMIT 的 DELETE 会锁住整张表。
//
// 出错时返回已经得到的读数与错误:前面几步删掉的行是真删了,读数要如实上报。
func (r *MailRepo) Sweep(ctx context.Context, cutoffMs uint64, batchLimit int, deleteRows bool) (SweepReport, error) {
	var report SweepReport
	if batchLimit <= 0 {
		return report, fmt.Errorf("sweep: batchLimit must be positive, got %d", batchLimit)
	}
	if cutoffMs == 0 {
		return report, nil
	}
	if err := r.sweepPersonal(ctx, cutoffMs, batchLimit, deleteRows, &report.Personal); err != nil {
		return report, err
	}
	if err := r.sweepSystem(ctx, cutoffMs, batchLimit, deleteRows, &report); err != nil {
		return report, err
	}
	return report, nil
}

func (r *MailRepo) sweepPersonal(ctx context.Context, cutoffMs uint64, batchLimit int, deleteRows bool, stats *SweepTableStats) error {
	var err error
	if stats.Invalid, err = r.boundedCount(ctx,
		`SELECT COUNT(*) FROM (SELECT 1 FROM mail_personal WHERE expire_ms = 0 LIMIT ?) AS invalid_rows`, batchLimit); err != nil {
		return fmt.Errorf("sweep: count invalid personal mail: %w", err)
	}
	if stats.Candidates, err = r.boundedCount(ctx,
		`SELECT COUNT(*) FROM (SELECT 1 FROM mail_personal
		   WHERE expire_ms > 0 AND expire_ms < ? AND claim_state IN (?, ?) LIMIT ?) AS candidates`,
		cutoffMs, ClaimStateNone, ClaimStateUnclaimed, batchLimit); err != nil {
		return fmt.Errorf("sweep: count expired personal mail: %w", err)
	}
	if !deleteRows || stats.Candidates == 0 {
		return nil
	}
	res, err := r.db.ExecContext(ctx,
		`DELETE FROM mail_personal WHERE expire_ms > 0 AND expire_ms < ? AND claim_state IN (?, ?) LIMIT ?`,
		cutoffMs, ClaimStateNone, ClaimStateUnclaimed, batchLimit)
	if err != nil {
		return fmt.Errorf("sweep: delete expired personal mail: %w", err)
	}
	stats.Deleted, err = res.RowsAffected()
	if err != nil {
		return fmt.Errorf("sweep: delete expired personal mail: rows affected: %w", err)
	}
	return nil
}

func (r *MailRepo) sweepSystem(ctx context.Context, cutoffMs uint64, batchLimit int, deleteRows bool, report *SweepReport) error {
	var err error
	if report.System.Invalid, err = r.boundedCount(ctx,
		`SELECT COUNT(*) FROM (SELECT 1 FROM mail_system WHERE expire_ms = 0 LIMIT ?) AS invalid_rows`, batchLimit); err != nil {
		return fmt.Errorf("sweep: count invalid system mail: %w", err)
	}
	ids, err := r.expiredSystemIDs(ctx, cutoffMs, batchLimit)
	if err != nil {
		return err
	}
	report.System.Candidates = int64(len(ids))
	for _, id := range ids {
		states, err := r.boundedCount(ctx,
			`SELECT COUNT(*) FROM (SELECT 1 FROM mail_system_state WHERE sys_mail_id = ? LIMIT ?) AS states`, id, batchLimit)
		if err != nil {
			return fmt.Errorf("sweep: count states of system mail %d: %w", id, err)
		}
		report.SystemState.Candidates += states
		if !deleteRows {
			continue
		}
		// 先把这封信的物化行删干净(可能不止一批),再删系统行;任何一步失败都停在这里,下一轮从头再来。
		for {
			res, err := r.db.ExecContext(ctx, `DELETE FROM mail_system_state WHERE sys_mail_id = ? LIMIT ?`, id, batchLimit)
			if err != nil {
				return fmt.Errorf("sweep: delete states of system mail %d: %w", id, err)
			}
			n, err := res.RowsAffected()
			if err != nil {
				return fmt.Errorf("sweep: delete states of system mail %d: rows affected: %w", id, err)
			}
			report.SystemState.Deleted += n
			if n < int64(batchLimit) {
				break
			}
			if err := ctx.Err(); err != nil {
				return fmt.Errorf("sweep: deleting states of system mail %d: %w", id, err)
			}
		}
		// WHERE 复核过期条件:选出 id 之后到这里之间,不可能有人把 expire_ms 改大(没有任何路径改它),
		// 复核是为了让这条语句单独拿出来读也是安全的。
		res, err := r.db.ExecContext(ctx,
			`DELETE FROM mail_system WHERE sys_mail_id = ? AND expire_ms > 0 AND expire_ms < ?`, id, cutoffMs)
		if err != nil {
			return fmt.Errorf("sweep: delete system mail %d: %w", id, err)
		}
		n, err := res.RowsAffected()
		if err != nil {
			return fmt.Errorf("sweep: delete system mail %d: rows affected: %w", id, err)
		}
		report.System.Deleted += n
	}
	return nil
}

// expiredSystemIDs 选出一批 expire_ms < cutoff 的系统邮件 id(走 idx_mail_system_0 (expire_ms))。
func (r *MailRepo) expiredSystemIDs(ctx context.Context, cutoffMs uint64, batchLimit int) ([]uint64, error) {
	rows, err := r.db.QueryContext(ctx,
		`SELECT sys_mail_id FROM mail_system WHERE expire_ms > 0 AND expire_ms < ? ORDER BY expire_ms LIMIT ?`,
		cutoffMs, batchLimit)
	if err != nil {
		return nil, fmt.Errorf("sweep: select expired system mail: %w", err)
	}
	defer rows.Close()
	var ids []uint64
	for rows.Next() {
		var id uint64
		if err := rows.Scan(&id); err != nil {
			return nil, fmt.Errorf("sweep: scan expired system mail id: %w", err)
		}
		ids = append(ids, id)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("sweep: select expired system mail: %w", err)
	}
	return ids, nil
}

// boundedCount 跑一条"派生表 LIMIT 的有界 COUNT"。
func (r *MailRepo) boundedCount(ctx context.Context, query string, args ...any) (int64, error) {
	var n int64
	if err := r.db.QueryRowContext(ctx, query, args...).Scan(&n); err != nil {
		return 0, err
	}
	return n, nil
}
