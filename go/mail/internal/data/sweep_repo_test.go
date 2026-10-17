package data

// sweep 的真 MySQL 回归(设计 docs/design/mail-system.md §4.6、§10 ④ 回修 ⑧)。门控与建库见 mail_repo_mysql_test.go。

import (
	"context"
	"database/sql"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestSweepRejectsNonPositiveBatch(t *testing.T) {
	repo := NewMailRepo(nil) // 参数校验在碰库之前
	for _, limit := range []int{0, -1} {
		_, err := repo.Sweep(t.Context(), testNowMs, limit, true)
		assert.Error(t, err, "batchLimit=%d", limit)
	}
}

func TestSweepZeroCutoffIsNoop(t *testing.T) {
	repo := NewMailRepo(nil) // cutoff = 0 不碰库:nil db 一碰就 panic
	report, err := repo.Sweep(t.Context(), 0, 100, true)
	require.NoError(t, err)
	assert.Equal(t, SweepReport{}, report)
}

// 定向邮件:只删 cutoff 之前过期、claim_state ∈ {0,1} 的行(含已软删的);2 / 4 与 expire_ms = 0 的保留。
func TestSweepPersonal(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	cutoff := testNowMs - 7*dayMs
	insertPersonalRow(t, ctx, db, 1, 42, 101, ClaimStateNone, cutoff-1, 0, 0)               // 删
	insertPersonalRow(t, ctx, db, 2, 42, 102, ClaimStateUnclaimed, cutoff-1, 0, 0)          // 删
	insertPersonalRow(t, ctx, db, 3, 42, 103, ClaimStateNone, cutoff-1, 0, cutoff-5)        // 已软删、过期:删
	insertPersonalRow(t, ctx, db, 4, 42, 104, 2, cutoff-1, 0, 0)                            // CLAIMING:保留
	insertPersonalRow(t, ctx, db, 5, 42, 105, 4, cutoff-1, 0, 0)                            // NEEDS_MANUAL:保留
	insertPersonalRow(t, ctx, db, 6, 42, 106, ClaimStateNone, cutoff, 0, 0)                 // 恰在 cutoff:保留(严格小于)
	insertPersonalRow(t, ctx, db, 7, 42, 107, ClaimStateNone, testNowMs-1, 0, 0)            // 过期但在保留期内:保留
	insertPersonalRow(t, ctx, db, 8, 42, 108, ClaimStateNone, 0, 0, 0)                      // 坏数据:只计数
	insertPersonalRow(t, ctx, db, 9, 42, 109, ClaimStateNone, testNowMs+dayMs, 0, cutoff-5) // 软删但未过期:保留(软删不缩短寿命)

	report, err := repo.Sweep(ctx, cutoff, 100, false)
	require.NoError(t, err)
	assert.Equal(t, int64(3), report.Personal.Candidates)
	assert.Equal(t, int64(1), report.Personal.Invalid)
	assert.Zero(t, report.Personal.Deleted, "report_only 不删")
	assert.Equal(t, 9, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))

	report, err = repo.Sweep(ctx, cutoff, 100, true)
	require.NoError(t, err)
	assert.Equal(t, int64(3), report.Personal.Deleted)
	assert.Equal(t, int64(1), report.Personal.Invalid)
	assert.Equal(t, []uint64{4, 5, 6, 7, 8, 9}, remainingPersonalIDs(t, db))

	again, err := repo.Sweep(ctx, cutoff, 100, true)
	require.NoError(t, err)
	assert.Zero(t, again.Personal.Deleted, "再跑一轮 = 0 行")
}

func TestSweepPersonalHonorsBatchLimit(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	cutoff := testNowMs - 7*dayMs
	for id := uint64(1); id <= 5; id++ {
		insertPersonalRow(t, ctx, db, id, 42, 100+id, ClaimStateNone, cutoff-1, 0, 0)
	}
	report, err := repo.Sweep(ctx, cutoff, 2, true)
	require.NoError(t, err)
	assert.Equal(t, int64(2), report.Personal.Candidates, "有界计数:等于 batchLimit 只说明至少这么多")
	assert.Equal(t, int64(2), report.Personal.Deleted)
	assert.Equal(t, 3, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))
}

// 回修 ⑧:系统邮件先删 state 行再删系统行;state 行多于一批时循环删干净;结束后没有孤儿 state 行。
func TestSweepSystemDeletesStatesFirstAndLeavesNoOrphans(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	cutoff := testNowMs - 7*dayMs
	insertSystemRow(t, ctx, db, 11, 1, nil, cutoff-1)        // 删(带 5 条 state,批大小 2 → 要删三批)
	insertSystemRow(t, ctx, db, 12, 2, nil, cutoff-2)        // 删(无 state)
	insertSystemRow(t, ctx, db, 13, 3, nil, testNowMs-1)     // 过期但在保留期内:保留
	insertSystemRow(t, ctx, db, 14, 4, nil, 0)               // 坏数据:只计数
	insertSystemRow(t, ctx, db, 15, 5, nil, testNowMs+dayMs) // 有效:保留
	for player := uint64(1); player <= 5; player++ {
		insertStateRow(t, ctx, db, player, 11)
	}
	insertStateRow(t, ctx, db, 1, 13)
	insertStateRow(t, ctx, db, 1, 15)

	report, err := repo.Sweep(ctx, cutoff, 2, false)
	require.NoError(t, err)
	assert.Equal(t, int64(2), report.System.Candidates)
	assert.Equal(t, int64(1), report.System.Invalid)
	assert.Equal(t, int64(2), report.SystemState.Candidates, "state 计数逐封有界(每封最多 batchLimit)")
	assert.Zero(t, report.System.Deleted)
	assert.Zero(t, report.SystemState.Deleted)

	report, err = repo.Sweep(ctx, cutoff, 2, true)
	require.NoError(t, err)
	assert.Equal(t, int64(2), report.System.Deleted)
	assert.Equal(t, int64(5), report.SystemState.Deleted)
	assert.Equal(t, 3, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system`))
	assert.Equal(t, 2, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system_state`))
	assert.Zero(t, countRows(t, ctx, db,
		`SELECT COUNT(*) FROM mail_system_state s LEFT JOIN mail_system m ON m.sys_mail_id = s.sys_mail_id WHERE m.sys_mail_id IS NULL`),
		"不能留下无主的 state 行")
}

// sweep 的候选(expire_ms < now − RetentionDays)与条件 upsert 的可见集(expire_ms > now)不相交(§4.2):
// 对一封**还在表里、已进入清理候选**的系统邮件做 upsert,不建任何 state 行 —— 所以 sweep 先删 state、再删系统行之间
// 不可能有人插进新的 state 行、留下孤儿。(在 sweep 之后再 upsert 证明不了什么:那时源行已经不在了。)
func TestUpsertNeverTouchesSweepCandidates(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	cutoff := testNowMs - 7*dayMs
	insertSystemRow(t, ctx, db, 11, 1, nil, cutoff-1)

	applied, err := repo.MarkSystemRead(ctx, 42, 11, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied)
	applied, err = repo.MarkSystemDeleted(ctx, 42, 11, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied)
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system_state`))

	report, err := repo.Sweep(ctx, cutoff, 100, true)
	require.NoError(t, err)
	assert.Equal(t, int64(1), report.System.Deleted)
}

// 回修 ⑧ 的"能变红"版本:sweep 删 state 行删到一半失败(另一个事务锁着其中一行,1205),系统行必须**还在** ——
// 顺序写反(先删系统行)时这里系统行已经没了,用例就红。成功路径的终态两种顺序一模一样,只有中途失败才分得出来。
func TestSweepKeepsSystemRowWhenStateDeletionFails(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	cutoff := testNowMs - 7*dayMs
	insertSystemRow(t, ctx, db, 11, 1, nil, cutoff-1)
	for player := uint64(1); player <= 5; player++ {
		insertStateRow(t, ctx, db, player, 11)
	}

	// 另开一个 RC 事务锁住 (5, 11):sweep 按批删(批大小 2),删到这一行时锁等待超时(innodb_lock_wait_timeout=2)。
	holder, err := db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	require.NoError(t, err)
	defer func() { _ = holder.Rollback() }()
	var locked uint64
	require.NoError(t, holder.QueryRowContext(ctx,
		`SELECT player_id FROM mail_system_state WHERE player_id = 5 AND sys_mail_id = 11 FOR UPDATE`).Scan(&locked))

	_, err = repo.Sweep(ctx, cutoff, 2, true)
	require.Error(t, err, "锁着一条 state 行时 sweep 应在删 state 的阶段失败")
	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system WHERE sys_mail_id = 11`),
		"state 行没删干净之前,系统行不能被删(否则剩下的 state 行永远成了孤儿)")
	require.NoError(t, holder.Rollback())

	// 锁放开后下一轮把剩下的删干净,且没有孤儿。
	_, err = repo.Sweep(ctx, cutoff, 2, true)
	require.NoError(t, err)
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system`))
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system_state`))
}

func insertStateRow(t *testing.T, ctx context.Context, db *sql.DB, playerID, sysMailID uint64) {
	t.Helper()
	_, err := db.ExecContext(ctx,
		`INSERT INTO mail_system_state (player_id, sys_mail_id, read_ms, deleted_ms, claim_state, updated_ms) VALUES (?, ?, 1, 0, 0, 1)`,
		playerID, sysMailID)
	require.NoError(t, err)
}

func remainingPersonalIDs(t *testing.T, db *sql.DB) []uint64 {
	t.Helper()
	rows, err := db.QueryContext(t.Context(), `SELECT mail_id FROM mail_personal ORDER BY mail_id`)
	require.NoError(t, err)
	defer rows.Close()
	var ids []uint64
	for rows.Next() {
		var id uint64
		require.NoError(t, rows.Scan(&id))
		ids = append(ids, id)
	}
	require.NoError(t, rows.Err())
	return ids
}
