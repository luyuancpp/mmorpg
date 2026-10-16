package data

// mail 存储层的真 MySQL 回归(设计 docs/design/mail-system.md §10 ④)。
//
// 为什么必须是真 MySQL:发件的正确性压在锁语义上(守卫行 FOR UPDATE 串行化、RC 下无间隙锁、
// 条件 upsert 的 RowsAffected 语义)。mock 只能复述我们自己的假设;TiDB 没有间隙锁,锁序相关用例在它上面恒绿。
//
// 门控、库名白名单、拒绝 TiDB / MariaDB、删表重建都在 internal/mailtest(logic 包的真库用例共用同一套):
// MAIL_TEST_MYSQL_DSN 指向一次性库(mail_test 或 mail_it_<n>_<n>);MAIL_REQUIRE_MYSQL_TESTS=1 时缺 DSN 判红。
// 表结构经 schemamigrate.Up 按 proto/mail/mail_table.proto 建出,与生产同一条建表路径,测试里不手写 DDL。
//
// 构建方命令见 docs/design/mail-system.md §12.2。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"mail/internal/mailtest"

	mailpb "proto/mail"
)

// testNowMs 是用例里的"现在"。远离 0 与 2^63,让"now − 若干天"的运算不碰边界。
const testNowMs = uint64(1_800_000_000_000)

const dayMs = uint64(24 * time.Hour / time.Millisecond)

// TestMailIntegrationGateIsHonored:验收模式下没给 DSN = 红(照 friend 的 TestFriendIntegrationGateIsHonored)。
// 不在验收模式时本用例跳过;在验收模式且 DSN 齐全时它什么都不做(真正的用例在下面)。
func TestMailIntegrationGateIsHonored(t *testing.T) {
	_ = mailtest.RequireDSN(t)
}

// openMailTestDB 门控 + 建连 + 重建 schema + 注册清理(见 internal/mailtest)。
func openMailTestDB(t *testing.T) (*sql.DB, context.Context) {
	t.Helper()
	return mailtest.OpenMySQL(t, Tables())
}

// testIDs 是测试用的 mail_id 发号器(单调递增,与号段一样全局不撞号)。
type testIDs struct{ next atomic.Uint64 }

func newTestIDs(start uint64) *testIDs {
	ids := &testIDs{}
	ids.next.Store(start)
	return ids
}

func (g *testIDs) alloc(context.Context) (uint64, error) { return g.next.Add(1), nil }

// countingAlloc 记下被调用的次数(验"拒绝 / 重放不该消耗号段"这类断言)。
type countingAlloc struct {
	ids   *testIDs
	calls atomic.Int64
}

func (c *countingAlloc) alloc(ctx context.Context) (uint64, error) {
	c.calls.Add(1)
	return c.ids.alloc(ctx)
}

func personalDraft(owner, requestID uint64, digest string, sentMs, expireMs uint64) *mailpb.MailPersonalRecord {
	return &mailpb.MailPersonalRecord{
		OwnerPlayerId: owner,
		SenderKind:    uint32(mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL),
		RequestId:     requestID,
		RequestDigest: []byte(digest),
		SenderName:    "运营",
		Title:         "标题",
		Body:          "正文",
		SentMs:        sentMs,
		ExpireMs:      expireMs,
	}
}

func sendPersonal(t *testing.T, ctx context.Context, repo *MailRepo, ids IDAllocator, rec *mailpb.MailPersonalRecord, limit uint32) SendResult {
	t.Helper()
	res, err := repo.SendPersonal(ctx, PersonalSend{
		Record:      rec,
		MaxExpireMs: rec.GetSentMs() + 30*dayMs,
		BoxLimit:    limit,
		NextID:      ids,
	})
	require.NoError(t, err)
	return res
}

func countRows(t *testing.T, ctx context.Context, db *sql.DB, query string, args ...any) int {
	t.Helper()
	var n int
	require.NoError(t, db.QueryRowContext(ctx, query, args...).Scan(&n))
	return n
}

// insertPersonalRow 绕过发件路径直写一行(造"已过期""未知 claim_state"这类 API 造不出的前置状态)。
func insertPersonalRow(t *testing.T, ctx context.Context, db *sql.DB, mailID, owner, requestID uint64, claimState uint32, expireMs, readMs, deletedMs uint64) {
	t.Helper()
	_, err := db.ExecContext(ctx,
		`INSERT INTO mail_personal (mail_id, owner_player_id, sender_kind, request_id, request_digest, sender_name, title, body,
		   attachments, claim_state, sent_ms, expire_ms, read_ms, updated_ms, deleted_ms)
		 VALUES (?, ?, 1, ?, 'd', 's', 't', 'b', '', ?, 1, ?, ?, 1, ?)`,
		mailID, owner, requestID, claimState, expireMs, readMs, deletedMs)
	require.NoError(t, err)
}

func insertSystemRow(t *testing.T, ctx context.Context, db *sql.DB, sysID, requestID uint64, attachments []byte, expireMs uint64) {
	t.Helper()
	if attachments == nil {
		attachments = []byte{}
	}
	_, err := db.ExecContext(ctx,
		`INSERT INTO mail_system (sys_mail_id, sender_kind, request_id, request_digest, sender_name, title, body, attachments,
		   sent_ms, expire_ms, updated_ms)
		 VALUES (?, 1, ?, 'd', 's', 't', 'b', ?, 1, ?, 1)`,
		sysID, requestID, attachments, expireMs)
	require.NoError(t, err)
}

// ── schema ──────────────────────────────────────────────────────────────────

// 表由 schemamigrate 按 proto 建出:恰好两个唯一键,列序与设计一致(U1 已核过源码,这里在真库上再钉一次)。
func TestSchemaHasDesignedUniqueKeys(t *testing.T) {
	db, ctx := openMailTestDB(t)
	rows, err := db.QueryContext(ctx,
		`SELECT TABLE_NAME, INDEX_NAME, GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX)
		 FROM information_schema.STATISTICS
		 WHERE TABLE_SCHEMA = DATABASE() AND NON_UNIQUE = 0 AND INDEX_NAME <> 'PRIMARY'
		 GROUP BY TABLE_NAME, INDEX_NAME ORDER BY TABLE_NAME`)
	require.NoError(t, err)
	defer rows.Close()
	got := map[string]string{}
	for rows.Next() {
		var table, index, cols string
		require.NoError(t, rows.Scan(&table, &index, &cols))
		got[table+"."+index] = cols
	}
	require.NoError(t, rows.Err())
	assert.Equal(t, map[string]string{
		"mail_personal.uk_mail_personal": "sender_kind,request_id,owner_player_id",
		"mail_system.uk_mail_system":     "sender_kind,request_id",
	}, got)
}

// ── 定向发件 ────────────────────────────────────────────────────────────────

func TestSendPersonalCreatedThenReplayThenConflict(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)

	first := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "digest-a", testNowMs, testNowMs+dayMs), 100)
	require.Equal(t, SendCreated, first.Outcome)
	require.NotZero(t, first.MailID)

	replay := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "digest-a", testNowMs+5, testNowMs+dayMs), 100)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID, "同键同摘要必回首次的 mail_id")

	conflict := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "digest-b", testNowMs+5, testNowMs+dayMs), 100)
	assert.Equal(t, SendConflict, conflict.Outcome)
	assert.Zero(t, conflict.MailID)

	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`), "重放与冲突都不能多出一行")

	// 同一个 request_id 发给另一个收件人是另一封(唯一键带 owner:一次结算给 N 个人各发一封)。
	other := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(43, 7, "digest-a", testNowMs, testNowMs+dayMs), 100)
	assert.Equal(t, SendCreated, other.Outcome)

	// claim_state 按附件推导:无附件 = NONE。
	var claim uint32
	require.NoError(t, db.QueryRowContext(ctx, `SELECT claim_state FROM mail_personal WHERE mail_id = ?`, first.MailID).Scan(&claim))
	assert.Equal(t, ClaimStateNone, claim)
}

func TestSendPersonalDerivesClaimStateFromAttachments(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	rec := personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs)
	rec.Attachments = []byte{0x0a, 0x02, 0x08, 0x01}
	res := sendPersonal(t, ctx, repo, newTestIDs(1000).alloc, rec, 100)
	require.Equal(t, SendCreated, res.Outcome)
	var claim uint32
	require.NoError(t, db.QueryRowContext(ctx, `SELECT claim_state FROM mail_personal WHERE mail_id = ?`, res.MailID).Scan(&claim))
	assert.Equal(t, ClaimStateUnclaimed, claim, "带附件的信建行时就是 UNCLAIMED(M1 写推导值,不是恒 0)")
}

// 同一收件人 20 路并发首发、上限 5:恰好成功 5 封,其余满箱,没有任何错误(包括 1213 死锁)。
func TestSendPersonalConcurrentRespectsBoxLimit(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)

	const senders = 20
	const limit = 5
	var wg sync.WaitGroup
	results := make([]SendResult, senders)
	errs := make([]error, senders)
	start := make(chan struct{})
	for i := 0; i < senders; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			<-start
			rec := personalDraft(42, uint64(100+i), fmt.Sprintf("d%d", i), testNowMs, testNowMs+dayMs)
			results[i], errs[i] = repo.SendPersonal(ctx, PersonalSend{Record: rec, MaxExpireMs: testNowMs + 30*dayMs, BoxLimit: limit, NextID: ids.alloc})
		}(i)
	}
	close(start)
	wg.Wait()

	created, full := 0, 0
	for i := 0; i < senders; i++ {
		require.NoError(t, errs[i], "第 %d 路不该出错(1213 / 1205 都算缺陷)", i)
		switch results[i].Outcome {
		case SendCreated:
			created++
		case SendBoxFull:
			full++
		default:
			t.Errorf("第 %d 路意外结果 %v", i, results[i].Outcome)
		}
	}
	assert.Equal(t, limit, created)
	assert.Equal(t, senders-limit, full)
	assert.Equal(t, limit, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal WHERE owner_player_id = 42`))
}

// 回修 ①:发件 → 收件人删除 → 同键重放:同一 mail_id、duplicated,表里不多一行(软删保住了幂等键)。
func TestReplayAfterRecipientDeleteIsDuplicated(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)
	first := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), 100)
	require.Equal(t, SendCreated, first.Outcome)

	deleted, err := repo.DeletePersonal(ctx, 42, []uint64{first.MailID}, testNowMs+1)
	require.NoError(t, err)
	require.Equal(t, int64(1), deleted)

	replay := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs+2, testNowMs+dayMs), 100)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID)
	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))
}

// 回修 ②:首发成功 → 邮箱被填满 → 同键重放:duplicated 同一 id,不是 BoxFull(先判重,后做依赖"现在"的校验)。
func TestReplayWhenBoxFullIsDuplicated(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)
	const limit = 3
	first := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), limit)
	require.Equal(t, SendCreated, first.Outcome)
	for i := 0; i < limit-1; i++ {
		require.Equal(t, SendCreated, sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, uint64(50+i), "x", testNowMs, testNowMs+dayMs), limit).Outcome)
	}
	require.Equal(t, SendBoxFull, sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 99, "y", testNowMs, testNowMs+dayMs), limit).Outcome)

	replay := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs+1, testNowMs+dayMs), limit)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID)
}

// 回修 ③:首发成功 → 时钟推到 expire_ms 之后 → 同键重放:duplicated(不是"过期时间非法")。
func TestReplayAfterExpiryIsDuplicated(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)
	first := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), 100)
	require.Equal(t, SendCreated, first.Outcome)

	later := testNowMs + 2*dayMs
	replay := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", later, testNowMs+dayMs), 100)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID)
}

// 回修 ④:100 封全部已过期(sweep 只在 report_only 下统计、没删)时第 101 封照样发得进去 —— 过期的不占名额。
func TestExpiredMailDoesNotOccupyBox(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	for i := uint64(0); i < 100; i++ {
		insertPersonalRow(t, ctx, db, 10+i, 42, 1000+i, ClaimStateNone, testNowMs-dayMs, 0, 0)
	}
	res := sendPersonal(t, ctx, repo, newTestIDs(100000).alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), 100)
	assert.Equal(t, SendCreated, res.Outcome)
}

// 软删的也不占名额(口径与 ListMails 一致)。
func TestDeletedMailDoesNotOccupyBox(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertPersonalRow(t, ctx, db, 10, 42, 1000, ClaimStateNone, testNowMs+dayMs, 0, testNowMs-1)
	res := sendPersonal(t, ctx, repo, newTestIDs(100000).alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), 1)
	assert.Equal(t, SendCreated, res.Outcome)
}

func TestSendPersonalRejectsExpireOutsideWindow(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(1000)
	for _, expire := range []uint64{testNowMs, testNowMs - 1, testNowMs + 30*dayMs + 1} {
		res := sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 7, "d", testNowMs, expire), 100)
		assert.Equal(t, SendExpireOutOfRange, res.Outcome, "expire_ms=%d", expire)
	}
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))
	// 上界本身合法。
	assert.Equal(t, SendCreated, sendPersonal(t, ctx, repo, ids.alloc, personalDraft(42, 8, "d", testNowMs, testNowMs+30*dayMs), 100).Outcome)
}

func TestSendPersonalAllocatorFailureIsError(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	_, err := repo.SendPersonal(ctx, PersonalSend{
		Record:      personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs),
		MaxExpireMs: testNowMs + 30*dayMs,
		BoxLimit:    100,
		NextID:      func(context.Context) (uint64, error) { return 0, errors.New("data_service down") },
	})
	assert.Error(t, err, "号段取不到是依赖故障")
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))
}

// 已成功的请求重放不依赖号段:领号之前的判重快路径命中即返回,号段不可达也拿得回首次结果,而且不消耗号。
func TestPersonalReplayDoesNotNeedIDSegment(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	first := sendPersonal(t, ctx, repo, newTestIDs(1000).alloc, personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs), 100)
	require.Equal(t, SendCreated, first.Outcome)

	down := &countingAlloc{ids: newTestIDs(0)}
	broken := func(ctx context.Context) (uint64, error) {
		_, _ = down.alloc(ctx)
		return 0, errors.New("data_service down")
	}
	replay, err := repo.SendPersonal(ctx, PersonalSend{
		Record: personalDraft(42, 7, "d", testNowMs+1, testNowMs+dayMs), MaxExpireMs: testNowMs + 31*dayMs, BoxLimit: 100, NextID: broken,
	})
	require.NoError(t, err)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID)
	assert.Zero(t, down.calls.Load(), "重放不该去领号")
}

// 号段把已经用过的 id 又发了一次(水位回退):撞的是主键,必须报成"重发",而不是被误报成"行被 sweep 删了"或当成重放。
func TestReissuedIDIsReportedNotReplayed(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertPersonalRow(t, ctx, db, 777, 43, 9999, ClaimStateNone, testNowMs+dayMs, 0, 0)
	_, err := repo.SendPersonal(ctx, PersonalSend{
		Record:      personalDraft(42, 7, "d", testNowMs, testNowMs+dayMs),
		MaxExpireMs: testNowMs + 30*dayMs, BoxLimit: 100,
		NextID: func(context.Context) (uint64, error) { return 777, nil },
	})
	require.Error(t, err)
	assert.Contains(t, err.Error(), "re-issued")
	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_personal`))

	insertSystemRow(t, ctx, db, 888, 1, nil, testNowMs+dayMs)
	_, err = repo.SendSystem(ctx, SystemSend{
		Record: systemDraft(2, "d", testNowMs, testNowMs+dayMs), MaxExpireMs: testNowMs + 30*dayMs, ActiveLimit: 100,
		NextID: func(context.Context) (uint64, error) { return 888, nil },
	})
	require.Error(t, err)
	assert.Contains(t, err.Error(), "re-issued")
	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system`))
}

// ── 系统发件 ────────────────────────────────────────────────────────────────

func systemDraft(requestID uint64, digest string, sentMs, expireMs uint64) *mailpb.MailSystemRecord {
	return &mailpb.MailSystemRecord{
		SenderKind:    uint32(mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL),
		RequestId:     requestID,
		RequestDigest: []byte(digest),
		SenderName:    "运营",
		Title:         "公告",
		Body:          "正文",
		SentMs:        sentMs,
		ExpireMs:      expireMs,
	}
}

func sendSystem(t *testing.T, ctx context.Context, repo *MailRepo, ids IDAllocator, rec *mailpb.MailSystemRecord, limit uint32) SendResult {
	t.Helper()
	res, err := repo.SendSystem(ctx, SystemSend{Record: rec, MaxExpireMs: rec.GetSentMs() + 30*dayMs, ActiveLimit: limit, NextID: ids})
	require.NoError(t, err)
	return res
}

func TestSendSystemCreatedReplayConflictAndLimit(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	alloc := &countingAlloc{ids: newTestIDs(5000)}

	first := sendSystem(t, ctx, repo, alloc.alloc, systemDraft(7, "a", testNowMs, testNowMs+dayMs), 2)
	require.Equal(t, SendCreated, first.Outcome)
	replay := sendSystem(t, ctx, repo, alloc.alloc, systemDraft(7, "a", testNowMs+1, testNowMs+dayMs), 2)
	assert.Equal(t, SendDuplicated, replay.Outcome)
	assert.Equal(t, first.MailID, replay.MailID)
	assert.Equal(t, SendConflict, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(7, "b", testNowMs+1, testNowMs+dayMs), 2).Outcome)
	assert.Equal(t, int64(1), alloc.calls.Load(), "重放与冲突不该消耗号段")

	require.Equal(t, SendCreated, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(8, "c", testNowMs, testNowMs+dayMs), 2).Outcome)
	before := alloc.calls.Load()
	assert.Equal(t, SendSystemLimit, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(9, "d", testNowMs, testNowMs+dayMs), 2).Outcome)
	assert.Equal(t, SendExpireOutOfRange, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(10, "e", testNowMs, testNowMs), 2).Outcome)
	assert.Equal(t, before, alloc.calls.Load(), "被拒的发件不该消耗号段")

	// 上限过后再重放第一封:先判重,所以仍回 duplicated,而不是 SystemLimit。
	assert.Equal(t, SendDuplicated, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(7, "a", testNowMs+2, testNowMs+dayMs), 2).Outcome)
	// 过期的不占上限。
	assert.Equal(t, SendCreated, sendSystem(t, ctx, repo, alloc.alloc, systemDraft(11, "f", testNowMs+2*dayMs, testNowMs+3*dayMs), 2).Outcome)
}

// 并发首发撞唯一键:一个 Created,其余在插入撞 1062 后读回比摘要得 Duplicated,同一个 id,表里一行。
//
// 号段里加了一道屏障:8 路都过了"先判重"(都没看到行)才一起去领号、插入。没有它,后到的几路可能在判重那一步就看到行,
// 1062 → 读回这条兜底路径一次都没走到,用例照样绿。
func TestSendSystemConcurrentFirstSendsConverge(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	ids := newTestIDs(5000)
	const n = 8
	var arrived atomic.Int32
	allIn := make(chan struct{})
	barrier := func(ctx context.Context) (uint64, error) {
		if arrived.Add(1) == n {
			close(allIn)
		}
		select {
		case <-allIn:
		case <-ctx.Done():
			return 0, ctx.Err()
		}
		return ids.alloc(ctx)
	}
	var wg sync.WaitGroup
	results := make([]SendResult, n)
	errs := make([]error, n)
	start := make(chan struct{})
	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			<-start
			results[i], errs[i] = repo.SendSystem(ctx, SystemSend{
				Record: systemDraft(7, "same", testNowMs, testNowMs+dayMs), MaxExpireMs: testNowMs + 30*dayMs, ActiveLimit: 100, NextID: barrier,
			})
		}(i)
	}
	close(start)
	wg.Wait()
	created := 0
	var id uint64
	for i := 0; i < n; i++ {
		require.NoError(t, errs[i])
		if results[i].Outcome == SendCreated {
			created++
		} else {
			require.Equal(t, SendDuplicated, results[i].Outcome)
		}
		if id == 0 {
			id = results[i].MailID
		}
		assert.Equal(t, id, results[i].MailID, "所有并发首发必须收敛到同一个 id")
	}
	assert.Equal(t, 1, created)
	assert.Equal(t, int32(n), arrived.Load(), "8 路都必须走到领号(= 都过了判重、都去插入),否则 1062 兜底没被覆盖")
	assert.Equal(t, 1, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system`))
}

// ── 读 ──────────────────────────────────────────────────────────────────────

func TestListPersonalPaginatesAndFilters(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	for id := uint64(1); id <= 5; id++ {
		insertPersonalRow(t, ctx, db, id, 42, 100+id, ClaimStateNone, testNowMs+dayMs, 0, 0)
	}
	insertPersonalRow(t, ctx, db, 6, 42, 106, ClaimStateNone, testNowMs-1, 0, 0)     // 已过期
	insertPersonalRow(t, ctx, db, 7, 42, 107, ClaimStateNone, testNowMs+dayMs, 0, 5) // 已软删
	insertPersonalRow(t, ctx, db, 8, 43, 108, ClaimStateNone, testNowMs+dayMs, 0, 0) // 别人的
	insertPersonalRow(t, ctx, db, 9, 42, 109, ClaimStateUnclaimed, testNowMs+dayMs, testNowMs-5, 0)

	page, err := repo.ListPersonal(ctx, 42, 0, testNowMs, 3)
	require.NoError(t, err)
	assert.Equal(t, []uint64{9, 5, 4}, personalIDs(page), "按 mail_id 降序;过期 / 软删 / 别人的都不出现")

	page, err = repo.ListPersonal(ctx, 42, 4, testNowMs, 10)
	require.NoError(t, err)
	assert.Equal(t, []uint64{3, 2, 1}, personalIDs(page), "游标:只返回 mail_id 小于它的")

	unread, claimable, err := repo.PersonalCounts(ctx, 42, testNowMs)
	require.NoError(t, err)
	assert.Equal(t, uint32(5), unread, "1–5 未读;9 已读;6 过期、7 软删不计")
	assert.Equal(t, uint32(1), claimable)
}

func personalIDs(recs []*mailpb.MailPersonalRecord) []uint64 {
	out := make([]uint64, 0, len(recs))
	for _, r := range recs {
		out = append(out, r.GetMailId())
	}
	return out
}

func TestListActiveSystemOrdersAndTruncates(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertSystemRow(t, ctx, db, 11, 1, nil, testNowMs+dayMs)
	insertSystemRow(t, ctx, db, 12, 2, nil, testNowMs+dayMs)
	insertSystemRow(t, ctx, db, 13, 3, nil, testNowMs+dayMs)
	insertSystemRow(t, ctx, db, 14, 4, nil, testNowMs-1) // 已过期

	mails, truncated, err := repo.ListActiveSystem(ctx, testNowMs, 2)
	require.NoError(t, err)
	assert.True(t, truncated)
	require.Len(t, mails, 2)
	assert.Equal(t, uint64(13), mails[0].GetSysMailId(), "截断时丢的是最早发出的(sys_mail_id 最小)")
	assert.Equal(t, uint64(12), mails[1].GetSysMailId())

	mails, truncated, err = repo.ListActiveSystem(ctx, testNowMs, 3)
	require.NoError(t, err)
	assert.False(t, truncated)
	assert.Len(t, mails, 3)
}

// ── 已读 ────────────────────────────────────────────────────────────────────

func TestMarkPersonalRead(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertPersonalRow(t, ctx, db, 1, 42, 101, ClaimStateNone, testNowMs+dayMs, 0, 0)
	insertPersonalRow(t, ctx, db, 2, 42, 102, ClaimStateNone, testNowMs-1, 0, 0) // 已过期
	insertPersonalRow(t, ctx, db, 3, 42, 103, ClaimStateNone, testNowMs+dayMs, 0, 9)

	rec, err := repo.MarkPersonalRead(ctx, 42, 1, testNowMs)
	require.NoError(t, err)
	require.NotNil(t, rec)
	assert.Equal(t, testNowMs, rec.GetReadMs())
	assert.Equal(t, "b", rec.GetBody(), "已读返回正文")

	again, err := repo.MarkPersonalRead(ctx, 42, 1, testNowMs+100)
	require.NoError(t, err)
	assert.Equal(t, testNowMs, again.GetReadMs(), "已读再读 = 原样,不改已读时刻")

	other, err := repo.MarkPersonalRead(ctx, 43, 1, testNowMs)
	require.NoError(t, err)
	assert.Nil(t, other, "别人的信 = 不存在")

	expired, err := repo.MarkPersonalRead(ctx, 42, 2, testNowMs)
	require.NoError(t, err)
	require.NotNil(t, expired, "过期的信读得回来,由调用方回 MailExpired")
	assert.Zero(t, expired.GetReadMs(), "过期的信不标已读")

	deleted, err := repo.MarkPersonalRead(ctx, 42, 3, testNowMs)
	require.NoError(t, err)
	assert.Nil(t, deleted, "软删的信 = 不存在")
}

// 回修 ⑥:系统邮件已过期时标已读不建 state 行;回修 ⑦:带附件的系统邮件物化后 claim_state = 1。
func TestMarkSystemReadVisibilityAndDerivedClaimState(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertSystemRow(t, ctx, db, 11, 1, nil, testNowMs-1)                    // 已过期
	insertSystemRow(t, ctx, db, 12, 2, []byte{0x0a, 0x00}, testNowMs+dayMs) // 带附件
	insertSystemRow(t, ctx, db, 13, 3, nil, testNowMs+dayMs)

	applied, err := repo.MarkSystemRead(ctx, 42, 11, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied)
	applied, err = repo.MarkSystemRead(ctx, 42, 999, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied, "不存在的系统邮件不能被造出状态行")
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system_state WHERE sys_mail_id IN (11, 999)`))

	applied, err = repo.MarkSystemRead(ctx, 42, 12, testNowMs)
	require.NoError(t, err)
	assert.True(t, applied)
	st, err := repo.GetSystemState(ctx, 42, 12)
	require.NoError(t, err)
	require.NotNil(t, st)
	assert.Equal(t, ClaimStateUnclaimed, st.GetClaimState())
	assert.Equal(t, testNowMs, st.GetReadMs())

	applied, err = repo.MarkSystemRead(ctx, 42, 12, testNowMs+5)
	require.NoError(t, err)
	assert.False(t, applied, "已读再读:RowsAffected = 0,由调用方点查确认成功")
	st, err = repo.GetSystemState(ctx, 42, 12)
	require.NoError(t, err)
	assert.Equal(t, testNowMs, st.GetReadMs())

	applied, err = repo.MarkSystemRead(ctx, 42, 13, testNowMs)
	require.NoError(t, err)
	assert.True(t, applied)
	st, err = repo.GetSystemState(ctx, 42, 13)
	require.NoError(t, err)
	assert.Equal(t, ClaimStateNone, st.GetClaimState())
}

// ── 删除 ────────────────────────────────────────────────────────────────────

// 回修 ⑤:删除白名单 —— claim_state = 0 可删;= 1 且已过期可删;= 1 未过期受保护;未知值(模拟 M2 的 2 / 4)即使已过期也受保护。
func TestDeletePersonalWhitelist(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertPersonalRow(t, ctx, db, 1, 42, 101, ClaimStateNone, testNowMs+dayMs, 0, 0)
	insertPersonalRow(t, ctx, db, 2, 42, 102, ClaimStateUnclaimed, testNowMs-1, 0, 0)
	insertPersonalRow(t, ctx, db, 3, 42, 103, ClaimStateUnclaimed, testNowMs+dayMs, 0, 0)
	insertPersonalRow(t, ctx, db, 4, 42, 104, 2, testNowMs-1, 0, 0)
	insertPersonalRow(t, ctx, db, 5, 42, 105, 4, testNowMs-1, 0, 0)
	insertPersonalRow(t, ctx, db, 6, 43, 106, ClaimStateNone, testNowMs+dayMs, 0, 0) // 别人的

	n, err := repo.DeletePersonal(ctx, 42, []uint64{1, 2, 3, 4, 5, 6, 777}, testNowMs)
	require.NoError(t, err)
	assert.Equal(t, int64(2), n, "只有 1、2 可删")

	byID, err := repo.PersonalByIDs(ctx, 42, []uint64{1, 2, 3, 4, 5, 6, 777})
	require.NoError(t, err)
	assert.NotZero(t, byID[1].GetDeletedMs())
	assert.NotZero(t, byID[2].GetDeletedMs())
	for _, protected := range []uint64{3, 4, 5} {
		assert.Zero(t, byID[protected].GetDeletedMs(), "mail %d 受保护,不能被删", protected)
	}
	_, hasOthers := byID[6]
	_, hasMissing := byID[777]
	assert.False(t, hasOthers || hasMissing, "别人的 / 不存在的 id 不在结果里(调用方当作可能的系统邮件再查)")

	again, err := repo.DeletePersonal(ctx, 42, []uint64{1, 2}, testNowMs+1)
	require.NoError(t, err)
	assert.Zero(t, again, "已删的再删 = 0 行,调用方计成功")
}

func TestDeleteReadPersonal(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertPersonalRow(t, ctx, db, 1, 42, 101, ClaimStateNone, testNowMs+dayMs, testNowMs-5, 0)      // 已读可删
	insertPersonalRow(t, ctx, db, 2, 42, 102, ClaimStateNone, testNowMs+dayMs, 0, 0)                // 未读
	insertPersonalRow(t, ctx, db, 3, 42, 103, ClaimStateUnclaimed, testNowMs+dayMs, testNowMs-5, 0) // 已读但有未领附件
	insertPersonalRow(t, ctx, db, 4, 42, 104, ClaimStateUnclaimed, testNowMs-1, testNowMs-5, 0)     // 已读、附件已过期

	n, err := repo.DeleteReadPersonal(ctx, 42, testNowMs, 100)
	require.NoError(t, err)
	assert.Equal(t, int64(2), n)
	byID, err := repo.PersonalByIDs(ctx, 42, []uint64{1, 2, 3, 4})
	require.NoError(t, err)
	assert.NotZero(t, byID[1].GetDeletedMs())
	assert.Zero(t, byID[2].GetDeletedMs())
	assert.Zero(t, byID[3].GetDeletedMs())
	assert.NotZero(t, byID[4].GetDeletedMs())

	n, err = repo.DeleteReadPersonal(ctx, 42, testNowMs, 100)
	require.NoError(t, err)
	assert.Zero(t, n, "重复调用 = 0 行")
}

func TestMarkSystemDeleted(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertSystemRow(t, ctx, db, 11, 1, nil, testNowMs+dayMs)
	insertSystemRow(t, ctx, db, 12, 2, []byte{0x0a, 0x00}, testNowMs+dayMs) // 带附件:受保护
	insertSystemRow(t, ctx, db, 13, 3, nil, testNowMs-1)                    // 已过期

	applied, err := repo.MarkSystemDeleted(ctx, 42, 11, testNowMs)
	require.NoError(t, err)
	assert.True(t, applied)
	applied, err = repo.MarkSystemDeleted(ctx, 42, 11, testNowMs+1)
	require.NoError(t, err)
	assert.False(t, applied, "已删再删 = 0 行")

	applied, err = repo.MarkSystemDeleted(ctx, 42, 12, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied, "带未领附件的系统邮件不能删")
	// 已读物化过(claim_state=1)之后也不能删。
	_, err = repo.MarkSystemRead(ctx, 42, 12, testNowMs)
	require.NoError(t, err)
	applied, err = repo.MarkSystemDeleted(ctx, 42, 12, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied)
	st, err := repo.GetSystemState(ctx, 42, 12)
	require.NoError(t, err)
	assert.Zero(t, st.GetDeletedMs())

	applied, err = repo.MarkSystemDeleted(ctx, 42, 13, testNowMs)
	require.NoError(t, err)
	assert.False(t, applied)
	assert.Zero(t, countRows(t, ctx, db, `SELECT COUNT(*) FROM mail_system_state WHERE sys_mail_id = 13`), "过期的系统邮件不建 state 行")

	// 删过之后标已读不能把它"复活"成已读未删。
	applied, err = repo.MarkSystemRead(ctx, 42, 11, testNowMs+2)
	require.NoError(t, err)
	assert.False(t, applied)
	st, err = repo.GetSystemState(ctx, 42, 11)
	require.NoError(t, err)
	assert.NotZero(t, st.GetDeletedMs())
	assert.Zero(t, st.GetReadMs())
}

// 已物化的 state 行只有 claim_state = 0 才能删:ODKU 里的 claim_state 守卫要单独钉住 ——
// 带附件的系统邮件在 SELECT 那一步(LENGTH(attachments) = 0)就被挡掉,走不到 ODKU,证明不了这道守卫。
// 这里用无附件的有效系统邮件,直接写 claim_state = 1 / 2 / 4 的 state 行(模拟 M2 的状态)。
func TestMarkSystemDeletedRespectsStateClaim(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertSystemRow(t, ctx, db, 11, 1, nil, testNowMs+dayMs)
	for _, claim := range []uint32{1, 2, 4} {
		player := uint64(100 + claim)
		_, err := db.ExecContext(ctx,
			`INSERT INTO mail_system_state (player_id, sys_mail_id, read_ms, deleted_ms, claim_state, updated_ms) VALUES (?, 11, 1, 0, ?, 1)`,
			player, claim)
		require.NoError(t, err)
		applied, err := repo.MarkSystemDeleted(ctx, player, 11, testNowMs)
		require.NoError(t, err)
		assert.False(t, applied, "claim_state=%d 的 state 行不能被删", claim)
		st, err := repo.GetSystemState(ctx, player, 11)
		require.NoError(t, err)
		assert.Zero(t, st.GetDeletedMs(), "claim_state=%d", claim)
	}
}

func TestSystemStatesBatch(t *testing.T) {
	db, ctx := openMailTestDB(t)
	repo := NewMailRepo(db)
	insertSystemRow(t, ctx, db, 11, 1, nil, testNowMs+dayMs)
	insertSystemRow(t, ctx, db, 12, 2, nil, testNowMs+dayMs)
	_, err := repo.MarkSystemRead(ctx, 42, 11, testNowMs)
	require.NoError(t, err)
	_, err = repo.MarkSystemRead(ctx, 43, 12, testNowMs)
	require.NoError(t, err)

	states, err := repo.SystemStates(ctx, 42, []uint64{11, 12})
	require.NoError(t, err)
	assert.Len(t, states, 1, "只返回本人的状态行")
	assert.Contains(t, states, uint64(11))

	empty, err := repo.SystemStates(ctx, 42, nil)
	require.NoError(t, err)
	assert.Empty(t, empty)
	_, err = repo.SystemStates(ctx, 42, make([]uint64, MaxIDsPerCall+1))
	assert.Error(t, err)
}
