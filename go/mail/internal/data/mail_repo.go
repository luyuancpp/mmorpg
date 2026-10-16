package data

import (
	"bytes"
	"context"
	"database/sql"
	"errors"
	"fmt"
	"math"
	"strings"

	drivermysql "github.com/go-sql-driver/mysql"

	mailpb "proto/mail"
)

// mail_repo.go —— mail 的读写 SQL(设计 docs/design/mail-system.md §3、§4.2)。
//
// # 本文件的几条纪律(写新语句前先读)
//
//  1. **所有读路径都带 deleted_ms = 0**(定向邮件软删,§3.3):行留到 sweep,是为了让发件幂等键活满
//     expire_ms + RetentionDays(§2.4)。漏一处就是"删掉的信又冒出来"。
//  2. **系统邮件的"可见"只有一个定义:expire_ms > now**。对不可见的系统邮件,任何按 id 的路径都不建 state 行。
//  3. **state 行只经条件 upsert 建出**(INSERT … SELECT … FROM mail_system WHERE sys_mail_id = ? AND expire_ms > ?),
//     不许凭客户端给的 id 直接插 —— 否则任何人都能给不存在的系统邮件造行;也正因为它只认有效的系统行,
//     sweep(只删 expire_ms < now − RetentionDays 的系统行)与迟到的 upsert 交错不可能生出孤儿(§4.2)。
//  4. **删除保护是白名单**:claim_state = 0,或 claim_state = 1 且已过期。M2 追加状态时只允许往白名单里加 3(CLAIMED),
//     2(CLAIMING)/ 4(NEEDS_MANUAL)无论是否过期都不许删 —— 它们挂着未终结的资产 op,删了就断了对账线索(§4.2)。
//  5. 本层**只回"发生了哪件事"**,不碰 tip 码;返回的 error 一律是存储故障(logic 映射成 kServiceUnavailable)。
//     业务结果(重放、冲突、满箱……)走 SendOutcome,不走 error。
//  6. 写事务用 READ COMMITTED(§4.1:RR 的间隙锁在"同一玩家并发插不同行"时成环,而且只在 MySQL 上炸);
//     连接池本身也固定了会话级 RC(svc.BuildDSN),管事务之外的自动提交语句。
//  7. mail 对 MySQL **不做自动重试**(§2.5):调用方用同一个幂等键重放才是重试。

// writeTxIsolation 是发件事务的隔离级别(纪律 6)。
const writeTxIsolation = sql.LevelReadCommitted

// MaxIDsPerCall 是按 id 批量操作(DeleteMails / state 点查)一次最多带多少个 id。协议上 DeleteMails ≤ 20 个,
// 系统有效集 ≤ Mail.MaxActiveSystemMails(上限 500);这里给 IN 列表一个与两者都相容的硬上限,防止调用方传入无界列表。
const MaxIDsPerCall = 512

// ClaimState* 是 mail.proto MailClaimState 的取值(列是 uint32,取值的唯一定义在那个枚举,§3.3 / M-12)。
const (
	ClaimStateNone      = uint32(mailpb.MailClaimState_MAIL_CLAIM_STATE_NONE)
	ClaimStateUnclaimed = uint32(mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED)
)

// ClaimStateFor 按附件推导建行时的 claim_state:附件非空 → UNCLAIMED,否则 NONE(§3.3:M1 就写推导值,不是恒 0)。
func ClaimStateFor(attachments []byte) uint32 {
	if len(attachments) > 0 {
		return ClaimStateUnclaimed
	}
	return ClaimStateNone
}

// ErrBoxGuardMissing:发件事务里拿不到收件人的守卫行。正常不会发生(SendPersonal 先在事务外 INSERT IGNORE 补齐),
// 出现说明守卫行被人删了;按存储故障处理。
var ErrBoxGuardMissing = errors.New("mail_box guard row missing")

// IDAllocator 领一个 mail_id / sys_mail_id(号段客户端的 Next)。失败 = 依赖故障。
type IDAllocator func(ctx context.Context) (uint64, error)

// SendOutcome 是一次发件的业务结果。
type SendOutcome uint8

const (
	// SendCreated:首次发出,SendResult.MailID 是新 id。
	SendCreated SendOutcome = iota
	// SendDuplicated:同一幂等键、同一请求摘要的重放,SendResult.MailID 是首次发出的 id(不再推送)。
	SendDuplicated
	// SendConflict:同一幂等键、不同请求摘要 —— 调用方复用了键却改了内容(MailRequestConflict)。
	SendConflict
	// SendExpireOutOfRange:expire_ms 不在 (sent_ms, 最长有效期] 内(kInvalidParameter)。判重之后才判(§2.4)。
	SendExpireOutOfRange
	// SendBoxFull:收件人有效定向邮件已达上限(MailRecipientBoxFull)。
	SendBoxFull
	// SendSystemLimit:有效系统邮件已达上限(MailSystemMailLimit)。
	SendSystemLimit
)

// SendResult 是发件的结果。只有 Created / Duplicated 带 MailID。
type SendResult struct {
	Outcome SendOutcome
	MailID  uint64
}

// MailRepo 是 mmorpg_mail 上的全部读写。无状态,可被多个请求并发使用。
type MailRepo struct {
	db *sql.DB
}

// NewMailRepo 构造。db 是 mmorpg_mail 的连接池(svc.OpenMySQL,会话级 RC + 严格模式)。
func NewMailRepo(db *sql.DB) *MailRepo {
	return &MailRepo{db: db}
}

// ════════════════════════════════════════════════════════════════════════════
// 发件
// ════════════════════════════════════════════════════════════════════════════

// PersonalSend 是一封定向邮件的写入参数。Record 由 logic 校验形状、算摘要之后交来:
// OwnerPlayerId / SenderKind / RequestId / RequestDigest / SenderName / Title / Body / Attachments / SentMs / ExpireMs 已填;
// MailId / ClaimState / ReadMs / UpdatedMs / DeletedMs 由本层写(id 来自 NextID,claim_state 按附件推导)。
type PersonalSend struct {
	Record *mailpb.MailPersonalRecord
	// MaxExpireMs 是 expire_ms 的上界(SentMs + Mail.MaxPersonalMailTtlDays)。
	MaxExpireMs uint64
	// BoxLimit 是收件人有效定向邮件上限(Mail.MaxPersonalMailsPerPlayer)。
	BoxLimit uint32
	// NextID 领 mail_id。在开事务之前调用:号段 RPC 不能发生在持锁的事务里。
	NextID IDAllocator
}

// SendPersonal 发一封定向邮件(设计 §4.2 的 SendPersonalMail 行,顺序是正确性的一部分):
//
//	⓪ 不持锁的判重快路径:按唯一键点查,命中即比摘要返回。唯一键行除了 sweep 没人改,所以这一步命中就是终局;
//	   它让"已成功的请求重放"不依赖号段可用,也不白耗一个号。未命中不代表可以插 —— 并发首发靠 ④ 的锁内当前读兜住
//	① 事务外领 mail_id(拒绝时它只留空洞,不重号)
//	② 事务外 INSERT IGNORE 补齐守卫行(放进事务会形成 insert-intention 死锁,friend F8 同理)
//	③ BEGIN(RC):守卫行 FOR UPDATE —— 同一收件人的发件从这里起串行
//	④ 按唯一键当前读:命中即比摘要,回 Duplicated / Conflict(**先判重**:已成功的请求重放必回同一 id,
//	   不会被满箱 / 过期拒,§2.4)
//	⑤ 未命中才做依赖"现在"的校验:expire_ms 窗口 → 现数(与 ListMails 同口径:未删且未过期)→ 超限拒收
//	⑥ INSERT → COMMIT
//
// 判重与现数在同一把守卫锁下,所以不会两个并发首发都看到"未满"而超限。删除路径不拿守卫:删除只会让真实数变少,
// 发件方读到的数只可能偏高 → 只会保守地多拒一次,不会超限(§3.4)。
func (r *MailRepo) SendPersonal(ctx context.Context, in PersonalSend) (SendResult, error) {
	rec := in.Record
	if rec == nil || rec.GetOwnerPlayerId() == 0 || rec.GetRequestId() == 0 || len(rec.GetRequestDigest()) == 0 || in.NextID == nil {
		return SendResult{}, errors.New("SendPersonal: owner, request_id, request_digest and NextID are required")
	}
	if result, found, err := r.lookupPersonalKey(ctx, rec); err != nil || found {
		return result, err
	}
	mailID, err := in.NextID(ctx)
	if err != nil {
		return SendResult{}, fmt.Errorf("allocate mail_id: %w", err)
	}
	if err := r.EnsureBox(ctx, rec.GetOwnerPlayerId(), rec.GetSentMs()); err != nil {
		return SendResult{}, err
	}

	result, dupKey, err := r.sendPersonalTx(ctx, in, mailID)
	if err != nil || !dupKey {
		return result, err
	}
	// 兜底:插入撞了唯一键(守卫锁下本不该发生,保留它是因为"不该发生"不等于"不会发生")。按唯一键读回比摘要。
	return r.personalReplay(ctx, rec)
}

// sendPersonalTx 是 SendPersonal 的事务部分。dupKey=true 表示 INSERT 撞了唯一键、事务已回滚,由调用方读回。
func (r *MailRepo) sendPersonalTx(ctx context.Context, in PersonalSend, mailID uint64) (result SendResult, dupKey bool, err error) {
	rec := in.Record
	tx, err := r.db.BeginTx(ctx, &sql.TxOptions{Isolation: writeTxIsolation})
	if err != nil {
		return SendResult{}, false, fmt.Errorf("begin send tx: %w", err)
	}
	committed := false
	defer func() {
		if !committed {
			_ = tx.Rollback()
		}
	}()

	var guard uint64
	err = tx.QueryRowContext(ctx, `SELECT player_id FROM mail_box WHERE player_id = ? FOR UPDATE`, rec.GetOwnerPlayerId()).Scan(&guard)
	if errors.Is(err, sql.ErrNoRows) {
		return SendResult{}, false, fmt.Errorf("lock mail_box of player %d: %w", rec.GetOwnerPlayerId(), ErrBoxGuardMissing)
	}
	if err != nil {
		return SendResult{}, false, fmt.Errorf("lock mail_box of player %d: %w", rec.GetOwnerPlayerId(), err)
	}

	existingID, existingDigest, found, err := scanKeyRow(tx.QueryRowContext(ctx,
		`SELECT mail_id, request_digest FROM mail_personal
		 WHERE sender_kind = ? AND request_id = ? AND owner_player_id = ? FOR UPDATE`,
		rec.GetSenderKind(), rec.GetRequestId(), rec.GetOwnerPlayerId()))
	if err != nil {
		return SendResult{}, false, fmt.Errorf("look up personal mail by idempotency key: %w", err)
	}
	if found {
		return replayResult(existingID, existingDigest, rec.GetRequestDigest()), false, nil
	}

	if !expireInWindow(rec.GetSentMs(), rec.GetExpireMs(), in.MaxExpireMs) {
		return SendResult{Outcome: SendExpireOutOfRange}, false, nil
	}
	var active uint64
	if err := tx.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM mail_personal WHERE owner_player_id = ? AND deleted_ms = 0 AND expire_ms > ?`,
		rec.GetOwnerPlayerId(), rec.GetSentMs()).Scan(&active); err != nil {
		return SendResult{}, false, fmt.Errorf("count active personal mail of player %d: %w", rec.GetOwnerPlayerId(), err)
	}
	if active >= uint64(in.BoxLimit) {
		return SendResult{Outcome: SendBoxFull}, false, nil
	}

	_, err = tx.ExecContext(ctx,
		`INSERT INTO mail_personal (mail_id, owner_player_id, sender_kind, request_id, request_digest, sender_name, title, body,
		   attachments, claim_state, sent_ms, expire_ms, read_ms, updated_ms, deleted_ms)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, 0)`,
		mailID, rec.GetOwnerPlayerId(), rec.GetSenderKind(), rec.GetRequestId(), rec.GetRequestDigest(),
		rec.GetSenderName(), rec.GetTitle(), rec.GetBody(), nonNilBytes(rec.GetAttachments()), ClaimStateFor(rec.GetAttachments()),
		rec.GetSentMs(), rec.GetExpireMs(), rec.GetSentMs())
	if dup, primary := duplicateKey(err); dup {
		if primary {
			return SendResult{}, false, reissuedIDError("mail_id", mailID)
		}
		return SendResult{}, true, nil
	}
	if err != nil {
		return SendResult{}, false, fmt.Errorf("insert personal mail: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return SendResult{}, false, fmt.Errorf("commit send tx: %w", err)
	}
	committed = true
	return SendResult{Outcome: SendCreated, MailID: mailID}, false, nil
}

// lookupPersonalKey 按唯一键(不持锁)查已有的定向邮件;found=false 表示这个键没用过。
func (r *MailRepo) lookupPersonalKey(ctx context.Context, rec *mailpb.MailPersonalRecord) (SendResult, bool, error) {
	id, digest, found, err := scanKeyRow(r.db.QueryRowContext(ctx,
		`SELECT mail_id, request_digest FROM mail_personal WHERE sender_kind = ? AND request_id = ? AND owner_player_id = ?`,
		rec.GetSenderKind(), rec.GetRequestId(), rec.GetOwnerPlayerId()))
	if err != nil {
		return SendResult{}, false, fmt.Errorf("look up personal mail by idempotency key: %w", err)
	}
	if !found {
		return SendResult{}, false, nil
	}
	return replayResult(id, digest, rec.GetRequestDigest()), true, nil
}

// personalReplay 是插入撞了唯一键之后的读回。读回为空只可能是那一行刚被 sweep 物理删掉(§2.4)→ 存储故障,
// 调用方重试时会被当成新请求(这就是"调用方重试窗口必须远小于 expire + RetentionDays"的由来)。
func (r *MailRepo) personalReplay(ctx context.Context, rec *mailpb.MailPersonalRecord) (SendResult, error) {
	result, found, err := r.lookupPersonalKey(ctx, rec)
	if err != nil {
		return SendResult{}, err
	}
	if !found {
		return SendResult{}, errors.New("personal mail insert hit the idempotency key but the row is gone (swept between the two statements?)")
	}
	return result, nil
}

// EnsureBox 在事务外补齐收件人的守卫行(幂等;已存在时什么都不改)。
func (r *MailRepo) EnsureBox(ctx context.Context, playerID, nowMs uint64) error {
	if _, err := r.db.ExecContext(ctx, `INSERT IGNORE INTO mail_box (player_id, created_ms) VALUES (?, ?)`, playerID, nowMs); err != nil {
		return fmt.Errorf("ensure mail_box of player %d: %w", playerID, err)
	}
	return nil
}

// SystemSend 是一封系统邮件的写入参数。Record 的填法同 PersonalSend(SysMailId / UpdatedMs 由本层写)。
type SystemSend struct {
	Record *mailpb.MailSystemRecord
	// MaxExpireMs 是 expire_ms 的上界(SentMs + Mail.MaxSystemMailTtlDays)。
	MaxExpireMs uint64
	// ActiveLimit 是有效系统邮件上限(Mail.MaxActiveSystemMails)。
	ActiveLimit uint32
	// NextID 领 sys_mail_id。只在判重与校验都通过之后才调用(系统邮件没有守卫行,先判重不必持锁)。
	NextID IDAllocator
}

// SendSystem 发一封系统邮件(设计 §4.2 的 SendSystemMail 行):
//
//	① 按唯一键查已有行,命中即比摘要返回(先判重)
//	② 未命中才校验 expire_ms 窗口与有效系统邮件数(软上限:两个运维同时发可能各自看到 99,发件方只有运维,接受)
//	③ 领 sys_mail_id → 单条自动提交 INSERT;并发首发撞唯一键 → 读回比摘要
//
// 系统邮件不推送、不可撤回(§11 第 8 / 9 条)。
func (r *MailRepo) SendSystem(ctx context.Context, in SystemSend) (SendResult, error) {
	rec := in.Record
	if rec == nil || rec.GetRequestId() == 0 || len(rec.GetRequestDigest()) == 0 || in.NextID == nil {
		return SendResult{}, errors.New("SendSystem: request_id, request_digest and NextID are required")
	}
	if result, found, err := r.systemReplay(ctx, rec); err != nil || found {
		return result, err
	}
	if !expireInWindow(rec.GetSentMs(), rec.GetExpireMs(), in.MaxExpireMs) {
		return SendResult{Outcome: SendExpireOutOfRange}, nil
	}
	active, err := r.countActiveSystem(ctx, rec.GetSentMs(), in.ActiveLimit)
	if err != nil {
		return SendResult{}, err
	}
	if active >= in.ActiveLimit {
		return SendResult{Outcome: SendSystemLimit}, nil
	}

	sysID, err := in.NextID(ctx)
	if err != nil {
		return SendResult{}, fmt.Errorf("allocate sys_mail_id: %w", err)
	}
	_, err = r.db.ExecContext(ctx,
		`INSERT INTO mail_system (sys_mail_id, sender_kind, request_id, request_digest, sender_name, title, body, attachments,
		   sent_ms, expire_ms, updated_ms)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		sysID, rec.GetSenderKind(), rec.GetRequestId(), rec.GetRequestDigest(), rec.GetSenderName(), rec.GetTitle(), rec.GetBody(),
		nonNilBytes(rec.GetAttachments()), rec.GetSentMs(), rec.GetExpireMs(), rec.GetSentMs())
	if dup, primary := duplicateKey(err); dup {
		if primary {
			return SendResult{}, reissuedIDError("sys_mail_id", sysID)
		}
		result, found, err := r.systemReplay(ctx, rec)
		if err != nil {
			return SendResult{}, err
		}
		if !found {
			return SendResult{}, errors.New("system mail insert hit the idempotency key but the row is gone (swept between the two statements?)")
		}
		return result, nil
	}
	if err != nil {
		return SendResult{}, fmt.Errorf("insert system mail: %w", err)
	}
	return SendResult{Outcome: SendCreated, MailID: sysID}, nil
}

// systemReplay 按唯一键查已有的系统邮件;found=false 表示这个键没用过。
func (r *MailRepo) systemReplay(ctx context.Context, rec *mailpb.MailSystemRecord) (SendResult, bool, error) {
	id, digest, found, err := scanKeyRow(r.db.QueryRowContext(ctx,
		`SELECT sys_mail_id, request_digest FROM mail_system WHERE sender_kind = ? AND request_id = ?`,
		rec.GetSenderKind(), rec.GetRequestId()))
	if err != nil {
		return SendResult{}, false, fmt.Errorf("look up system mail by idempotency key: %w", err)
	}
	if !found {
		return SendResult{}, false, nil
	}
	return replayResult(id, digest, rec.GetRequestDigest()), true, nil
}

// countActiveSystem 数有效系统邮件,最多数到 limit(有界计数:派生表 LIMIT,不扫全表)。
func (r *MailRepo) countActiveSystem(ctx context.Context, nowMs uint64, limit uint32) (uint32, error) {
	var n uint32
	if err := r.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM (SELECT 1 FROM mail_system WHERE expire_ms > ? LIMIT ?) AS active`,
		nowMs, limit).Scan(&n); err != nil {
		return 0, fmt.Errorf("count active system mail: %w", err)
	}
	return n, nil
}

// ════════════════════════════════════════════════════════════════════════════
// 读
// ════════════════════════════════════════════════════════════════════════════

// 列清单。列表不读正文(正文只在 ReadMail 返回);状态判定要用的 deleted_ms / claim_state / expire_ms 都在。
const (
	personalSummaryColumns = `mail_id, owner_player_id, sender_kind, sender_name, title, attachments, claim_state,
	  sent_ms, expire_ms, read_ms, updated_ms, deleted_ms`
	systemSummaryColumns = `sys_mail_id, sender_kind, sender_name, title, attachments, sent_ms, expire_ms, updated_ms`
	stateColumns         = `player_id, sys_mail_id, read_ms, deleted_ms, claim_state, updated_ms`
)

// ListPersonal 取一页有效定向邮件(未删、未过期),按 mail_id 降序。beforeMailID = 0 表示从最新开始。
// limit 由调用方给(要判 has_more 就多取一行);走 idx_mail_personal_0 (owner_player_id, mail_id) 的倒序范围扫描。
func (r *MailRepo) ListPersonal(ctx context.Context, ownerID, beforeMailID, nowMs uint64, limit uint32) ([]*mailpb.MailPersonalRecord, error) {
	if beforeMailID == 0 {
		// 号段 id < 2^55(shared/idsegment 的值域),取 MaxInt64 作"无上界"足够,也避开驱动对最高位为 1 的 uint64 的转换差异。
		beforeMailID = math.MaxInt64
	}
	rows, err := r.db.QueryContext(ctx,
		`SELECT `+personalSummaryColumns+` FROM mail_personal
		 WHERE owner_player_id = ? AND mail_id < ? AND deleted_ms = 0 AND expire_ms > ?
		 ORDER BY mail_id DESC LIMIT ?`,
		ownerID, beforeMailID, nowMs, limit)
	if err != nil {
		return nil, fmt.Errorf("list personal mail of player %d: %w", ownerID, err)
	}
	defer rows.Close()
	var out []*mailpb.MailPersonalRecord
	for rows.Next() {
		rec, err := scanPersonalSummary(rows)
		if err != nil {
			return nil, fmt.Errorf("scan personal mail: %w", err)
		}
		out = append(out, rec)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("list personal mail of player %d: %w", ownerID, err)
	}
	return out, nil
}

// PersonalCounts 对玩家**全部**有效定向邮件算未读数与待领数(红点以它为准,不是本页,§4.2 ListMails 行)。
// 有界:有效定向邮件 ≤ Mail.MaxPersonalMailsPerPlayer,保留期内的已删 / 过期行只回表过滤。
func (r *MailRepo) PersonalCounts(ctx context.Context, ownerID, nowMs uint64) (unread, claimable uint32, err error) {
	err = r.db.QueryRowContext(ctx,
		`SELECT COUNT(CASE WHEN read_ms = 0 THEN 1 END), COUNT(CASE WHEN claim_state = ? THEN 1 END)
		 FROM mail_personal WHERE owner_player_id = ? AND deleted_ms = 0 AND expire_ms > ?`,
		ClaimStateUnclaimed, ownerID, nowMs).Scan(&unread, &claimable)
	if err != nil {
		return 0, 0, fmt.Errorf("count personal mail of player %d: %w", ownerID, err)
	}
	return unread, claimable, nil
}

// ListActiveSystem 取有效系统邮件集,按 sys_mail_id 降序,最多 limit 封;超出时 truncated=true(调用方打 WARN 并计
// mail_list_truncated_total)。排序必须写进 SQL:截断时丢弃的必须是最早发出的几封,而不是由执行计划决定(§3.1)。
func (r *MailRepo) ListActiveSystem(ctx context.Context, nowMs uint64, limit uint32) (mails []*mailpb.MailSystemRecord, truncated bool, err error) {
	rows, err := r.db.QueryContext(ctx,
		`SELECT `+systemSummaryColumns+` FROM mail_system WHERE expire_ms > ? ORDER BY sys_mail_id DESC LIMIT ?`,
		nowMs, uint64(limit)+1)
	if err != nil {
		return nil, false, fmt.Errorf("list active system mail: %w", err)
	}
	defer rows.Close()
	for rows.Next() {
		rec, err := scanSystemSummary(rows)
		if err != nil {
			return nil, false, fmt.Errorf("scan system mail: %w", err)
		}
		mails = append(mails, rec)
	}
	if err := rows.Err(); err != nil {
		return nil, false, fmt.Errorf("list active system mail: %w", err)
	}
	if uint32(len(mails)) > limit {
		return mails[:limit], true, nil
	}
	return mails, false, nil
}

// SystemStates 点查玩家对一批系统邮件的状态行(主键前缀),返回 sys_mail_id → 行;没有行 = 未读未删。
func (r *MailRepo) SystemStates(ctx context.Context, playerID uint64, sysMailIDs []uint64) (map[uint64]*mailpb.MailSystemStateRecord, error) {
	out := make(map[uint64]*mailpb.MailSystemStateRecord, len(sysMailIDs))
	if len(sysMailIDs) == 0 {
		return out, nil
	}
	if len(sysMailIDs) > MaxIDsPerCall {
		return nil, fmt.Errorf("SystemStates: %d ids exceeds the per-call limit %d", len(sysMailIDs), MaxIDsPerCall)
	}
	args := append([]any{playerID}, uint64Args(sysMailIDs)...)
	rows, err := r.db.QueryContext(ctx,
		`SELECT `+stateColumns+` FROM mail_system_state WHERE player_id = ? AND sys_mail_id IN (`+placeholders(len(sysMailIDs))+`)`,
		args...)
	if err != nil {
		return nil, fmt.Errorf("read system mail states of player %d: %w", playerID, err)
	}
	defer rows.Close()
	for rows.Next() {
		st := &mailpb.MailSystemStateRecord{}
		if err := rows.Scan(&st.PlayerId, &st.SysMailId, &st.ReadMs, &st.DeletedMs, &st.ClaimState, &st.UpdatedMs); err != nil {
			return nil, fmt.Errorf("scan system mail state: %w", err)
		}
		out[st.GetSysMailId()] = st
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("read system mail states of player %d: %w", playerID, err)
	}
	return out, nil
}

// GetSystem 按主键取一封系统邮件(含正文);不存在返回 nil。是否可见(expire_ms > now)由调用方判。
func (r *MailRepo) GetSystem(ctx context.Context, sysMailID uint64) (*mailpb.MailSystemRecord, error) {
	rec, err := scanSystemFull(r.db.QueryRowContext(ctx,
		`SELECT `+systemSummaryColumns+`, body FROM mail_system WHERE sys_mail_id = ?`, sysMailID))
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("get system mail %d: %w", sysMailID, err)
	}
	return rec, nil
}

// GetSystemState 取玩家对一封系统邮件的状态行;没有行返回 nil。
func (r *MailRepo) GetSystemState(ctx context.Context, playerID, sysMailID uint64) (*mailpb.MailSystemStateRecord, error) {
	st := &mailpb.MailSystemStateRecord{}
	err := r.db.QueryRowContext(ctx,
		`SELECT `+stateColumns+` FROM mail_system_state WHERE player_id = ? AND sys_mail_id = ?`, playerID, sysMailID).
		Scan(&st.PlayerId, &st.SysMailId, &st.ReadMs, &st.DeletedMs, &st.ClaimState, &st.UpdatedMs)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("get system mail state (player %d, mail %d): %w", playerID, sysMailID, err)
	}
	return st, nil
}

// PersonalByIDs 取玩家名下这批 id 的定向邮件(**含已软删的行**,给删除结果分类用),返回 mail_id → 行。
// 不是本人的、不存在的 id 不在结果里 —— 调用方据此把它们当作"可能是系统邮件"再查,不泄露别人的邮件是否存在。
func (r *MailRepo) PersonalByIDs(ctx context.Context, ownerID uint64, mailIDs []uint64) (map[uint64]*mailpb.MailPersonalRecord, error) {
	out := make(map[uint64]*mailpb.MailPersonalRecord, len(mailIDs))
	if len(mailIDs) == 0 {
		return out, nil
	}
	if len(mailIDs) > MaxIDsPerCall {
		return nil, fmt.Errorf("PersonalByIDs: %d ids exceeds the per-call limit %d", len(mailIDs), MaxIDsPerCall)
	}
	args := append([]any{ownerID}, uint64Args(mailIDs)...)
	rows, err := r.db.QueryContext(ctx,
		`SELECT `+personalSummaryColumns+` FROM mail_personal WHERE owner_player_id = ? AND mail_id IN (`+placeholders(len(mailIDs))+`)`,
		args...)
	if err != nil {
		return nil, fmt.Errorf("read personal mail of player %d by id: %w", ownerID, err)
	}
	defer rows.Close()
	for rows.Next() {
		rec, err := scanPersonalSummary(rows)
		if err != nil {
			return nil, fmt.Errorf("scan personal mail: %w", err)
		}
		out[rec.GetMailId()] = rec
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("read personal mail of player %d by id: %w", ownerID, err)
	}
	return out, nil
}

// ════════════════════════════════════════════════════════════════════════════
// 已读 / 删除
// ════════════════════════════════════════════════════════════════════════════

// MarkPersonalRead 把一封自己的、未删、未过期、未读的定向邮件标已读(单条自动提交),再按 (mail_id, owner, 未删) 读回整行(含正文)。
//
// 返回 nil = 不是自己的 / 不存在 / 已软删(调用方回 MailNotFound,不区分这三种,不泄露存在性)。
// 返回的行可能已过期(expire_ms <= now,UPDATE 不会动它)—— 由调用方回 MailExpired。已读再读 = 原样返回。
func (r *MailRepo) MarkPersonalRead(ctx context.Context, ownerID, mailID, nowMs uint64) (*mailpb.MailPersonalRecord, error) {
	if _, err := r.db.ExecContext(ctx,
		`UPDATE mail_personal SET read_ms = ?, updated_ms = ?
		 WHERE mail_id = ? AND owner_player_id = ? AND deleted_ms = 0 AND read_ms = 0 AND expire_ms > ?`,
		nowMs, nowMs, mailID, ownerID, nowMs); err != nil {
		return nil, fmt.Errorf("mark personal mail %d read: %w", mailID, err)
	}
	rec, err := scanPersonalFull(r.db.QueryRowContext(ctx,
		`SELECT `+personalSummaryColumns+`, body FROM mail_personal WHERE mail_id = ? AND owner_player_id = ? AND deleted_ms = 0`,
		mailID, ownerID))
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("read back personal mail %d: %w", mailID, err)
	}
	return rec, nil
}

// MarkSystemRead 以"系统行存在且有效"为条件 upsert 玩家的 state 行并标已读(纪律 3)。
//
// applied=false 有三种可能,调用方再点查区分:系统行不存在 / 已过期(回 MailNotFound / MailExpired);
// state 行已被本人删掉(MailNotFound);已经是已读(成功)。MySQL 对"命中重复键但值没变"返回 RowsAffected = 0,
// 与"源行不存在"同值,所以不能只凭 RowsAffected 下结论,也不依赖驱动的 clientFoundRows(§4.2)。
//
// ON DUPLICATE KEY UPDATE 里 updated_ms 写在 read_ms 之前:MySQL 按书写顺序赋值,后面的表达式看到的是已更新的列,
// 先改 read_ms 会让 updated_ms 的条件恒假。claim_state 不在 UPDATE 里:它只在物化时按附件推导一次。
//
// ODKU 里的列**一律带表名**:INSERT … SELECT 的 ODKU 子句同时能看到目标表与 SELECT 的来源表,mail_system 也有
// updated_ms 列,不带表名就是 1052 "Column 'updated_ms' in field list is ambiguous" —— 语句每次执行都失败。
func (r *MailRepo) MarkSystemRead(ctx context.Context, playerID, sysMailID, nowMs uint64) (applied bool, err error) {
	res, err := r.db.ExecContext(ctx,
		`INSERT INTO mail_system_state (player_id, sys_mail_id, read_ms, deleted_ms, claim_state, updated_ms)
		 SELECT ?, sys_mail_id, ?, 0, IF(COALESCE(LENGTH(attachments), 0) > 0, ?, ?), ?
		   FROM mail_system WHERE sys_mail_id = ? AND expire_ms > ?
		 ON DUPLICATE KEY UPDATE
		   updated_ms = IF(mail_system_state.read_ms = 0 AND mail_system_state.deleted_ms = 0, ?, mail_system_state.updated_ms),
		   read_ms    = IF(mail_system_state.read_ms = 0 AND mail_system_state.deleted_ms = 0, ?, mail_system_state.read_ms)`,
		playerID, nowMs, ClaimStateUnclaimed, ClaimStateNone, nowMs, sysMailID, nowMs,
		nowMs, nowMs)
	if err != nil {
		return false, fmt.Errorf("mark system mail %d read for player %d: %w", sysMailID, playerID, err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, fmt.Errorf("mark system mail %d read: rows affected: %w", sysMailID, err)
	}
	return n > 0, nil
}

// DeletePersonal 软删玩家名下这批定向邮件里**允许删**的那些(白名单,纪律 4),一条自动提交的 UPDATE。
// 返回本次实际软删的行数。其余 id(不存在 / 已删 / 受保护 / 其实是系统邮件)由调用方用 PersonalByIDs 分类。
func (r *MailRepo) DeletePersonal(ctx context.Context, ownerID uint64, mailIDs []uint64, nowMs uint64) (int64, error) {
	if len(mailIDs) == 0 {
		return 0, nil
	}
	if len(mailIDs) > MaxIDsPerCall {
		return 0, fmt.Errorf("DeletePersonal: %d ids exceeds the per-call limit %d", len(mailIDs), MaxIDsPerCall)
	}
	args := []any{nowMs, nowMs, ownerID}
	args = append(args, uint64Args(mailIDs)...)
	args = append(args, ClaimStateNone, ClaimStateUnclaimed, nowMs)
	res, err := r.db.ExecContext(ctx,
		`UPDATE mail_personal SET deleted_ms = ?, updated_ms = ?
		 WHERE owner_player_id = ? AND mail_id IN (`+placeholders(len(mailIDs))+`) AND deleted_ms = 0
		   AND (claim_state = ? OR (claim_state = ? AND expire_ms <= ?))`,
		args...)
	if err != nil {
		return 0, fmt.Errorf("delete personal mail of player %d: %w", ownerID, err)
	}
	return res.RowsAffected()
}

// DeleteReadPersonal 软删玩家名下全部已读且允许删的定向邮件(含已过期未 sweep 的),最多 limit 行。返回实删行数。
func (r *MailRepo) DeleteReadPersonal(ctx context.Context, ownerID, nowMs uint64, limit uint32) (int64, error) {
	res, err := r.db.ExecContext(ctx,
		`UPDATE mail_personal SET deleted_ms = ?, updated_ms = ?
		 WHERE owner_player_id = ? AND deleted_ms = 0 AND read_ms > 0
		   AND (claim_state = ? OR (claim_state = ? AND expire_ms <= ?))
		 LIMIT ?`,
		nowMs, nowMs, ownerID, ClaimStateNone, ClaimStateUnclaimed, nowMs, limit)
	if err != nil {
		return 0, fmt.Errorf("delete read personal mail of player %d: %w", ownerID, err)
	}
	return res.RowsAffected()
}

// MarkSystemDeleted 以"系统行存在且有效、且允许删"为条件 upsert 玩家的 state 行并标删除(纪律 3、4)。
//
// 对**有效**的系统邮件,白名单"claim_state = 0,或 = 1 且已过期"退化为 claim_state = 0(有效即未过期):
// 没有 state 行时按附件推导,只有无附件的才插;已有 state 行时只改 claim_state = 0 的。
// applied=false:系统行不存在 / 已过期 / 受保护(带附件)/ 已经删过,调用方点查区分(同 MarkSystemRead)。
// ⚠ M2 追加 CLAIMED(3)后这里要放宽成"0 或 3"(已领完的允许删),推导插入那一支也要跟着改。
// ODKU 里的列一律带表名,理由同 MarkSystemRead。
func (r *MailRepo) MarkSystemDeleted(ctx context.Context, playerID, sysMailID, nowMs uint64) (applied bool, err error) {
	res, err := r.db.ExecContext(ctx,
		`INSERT INTO mail_system_state (player_id, sys_mail_id, read_ms, deleted_ms, claim_state, updated_ms)
		 SELECT ?, sys_mail_id, 0, ?, ?, ?
		   FROM mail_system WHERE sys_mail_id = ? AND expire_ms > ? AND COALESCE(LENGTH(attachments), 0) = 0
		 ON DUPLICATE KEY UPDATE
		   updated_ms = IF(mail_system_state.deleted_ms = 0 AND mail_system_state.claim_state = ?, ?, mail_system_state.updated_ms),
		   deleted_ms = IF(mail_system_state.deleted_ms = 0 AND mail_system_state.claim_state = ?, ?, mail_system_state.deleted_ms)`,
		playerID, nowMs, ClaimStateNone, nowMs, sysMailID, nowMs,
		ClaimStateNone, nowMs,
		ClaimStateNone, nowMs)
	if err != nil {
		return false, fmt.Errorf("mark system mail %d deleted for player %d: %w", sysMailID, playerID, err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, fmt.Errorf("mark system mail %d deleted: rows affected: %w", sysMailID, err)
	}
	return n > 0, nil
}

// ════════════════════════════════════════════════════════════════════════════
// 小工具
// ════════════════════════════════════════════════════════════════════════════

// expireInWindow:sent_ms < expire_ms ≤ maxExpireMs(maxExpireMs = sent_ms + 最长有效期,由调用方算)。
func expireInWindow(sentMs, expireMs, maxExpireMs uint64) bool {
	return sentMs < expireMs && expireMs <= maxExpireMs
}

// replayResult:同摘要 = 重放,回首次的 id;不同摘要 = 冲突。
func replayResult(existingID uint64, existingDigest, digest []byte) SendResult {
	if bytes.Equal(existingDigest, digest) {
		return SendResult{Outcome: SendDuplicated, MailID: existingID}
	}
	return SendResult{Outcome: SendConflict}
}

// scanKeyRow 扫 (id, request_digest) 一行;没有行返回 found=false。
func scanKeyRow(row *sql.Row) (id uint64, digest []byte, found bool, err error) {
	err = row.Scan(&id, &digest)
	if errors.Is(err, sql.ErrNoRows) {
		return 0, nil, false, nil
	}
	if err != nil {
		return 0, nil, false, err
	}
	return id, digest, true, nil
}

// rowScanner 是 *sql.Row 与 *sql.Rows 的公共扫描面。
type rowScanner interface {
	Scan(dest ...any) error
}

func scanPersonalSummary(s rowScanner) (*mailpb.MailPersonalRecord, error) {
	rec := &mailpb.MailPersonalRecord{}
	err := s.Scan(&rec.MailId, &rec.OwnerPlayerId, &rec.SenderKind, &rec.SenderName, &rec.Title, &rec.Attachments, &rec.ClaimState,
		&rec.SentMs, &rec.ExpireMs, &rec.ReadMs, &rec.UpdatedMs, &rec.DeletedMs)
	return rec, err
}

func scanPersonalFull(s rowScanner) (*mailpb.MailPersonalRecord, error) {
	rec := &mailpb.MailPersonalRecord{}
	err := s.Scan(&rec.MailId, &rec.OwnerPlayerId, &rec.SenderKind, &rec.SenderName, &rec.Title, &rec.Attachments, &rec.ClaimState,
		&rec.SentMs, &rec.ExpireMs, &rec.ReadMs, &rec.UpdatedMs, &rec.DeletedMs, &rec.Body)
	return rec, err
}

func scanSystemSummary(s rowScanner) (*mailpb.MailSystemRecord, error) {
	rec := &mailpb.MailSystemRecord{}
	err := s.Scan(&rec.SysMailId, &rec.SenderKind, &rec.SenderName, &rec.Title, &rec.Attachments, &rec.SentMs, &rec.ExpireMs, &rec.UpdatedMs)
	return rec, err
}

func scanSystemFull(s rowScanner) (*mailpb.MailSystemRecord, error) {
	rec := &mailpb.MailSystemRecord{}
	err := s.Scan(&rec.SysMailId, &rec.SenderKind, &rec.SenderName, &rec.Title, &rec.Attachments, &rec.SentMs, &rec.ExpireMs, &rec.UpdatedMs,
		&rec.Body)
	return rec, err
}

// duplicateKey 识别 1062(MySQL 与 TiDB 同号),并报告撞的是不是主键。
//
// 撞唯一键 = 并发首发(或兜底路径),按唯一键读回比摘要;撞**主键** = 号段把一个已经发出去的 id 又发了一次
// (id_segment 水位回退,比如全局库从备份恢复),读回唯一键什么也找不到 —— 必须单独报出来,
// 而不是被误报成"行被 sweep 删了"。两种库的报错文本都是 "... for key '<表>.PRIMARY'"。
func duplicateKey(err error) (dup, primary bool) {
	var me *drivermysql.MySQLError
	if !errors.As(err, &me) || me.Number != 1062 {
		return false, false
	}
	return true, strings.Contains(me.Message, "PRIMARY")
}

// reissuedIDError 是"号段重发了已用过的 id"的存储故障(运维要按 deploy/k8s/README.md 的恢复手册抬水位)。
func reissuedIDError(column string, id uint64) error {
	return fmt.Errorf("%s %d already exists: id_segment (biz_tag=mail) re-issued an id that is already used — "+
		"the watermark went backwards (global DB restored?); raise id_segment.max_id above MAX(mail_personal.mail_id, mail_system.sys_mail_id)",
		column, id)
}

// nonNilBytes 把 nil 换成空切片:驱动把 nil []byte 当 NULL 写。proto2mysql 把 bytes 生成为**可空**的 MEDIUMBLOB,
// 本层一律写空串而不写 NULL;SQL 里按附件判定的地方也都写成 COALESCE(LENGTH(attachments), 0),
// 万一有人绕过本层写进 NULL,那封信按"无附件"处理,而不是 LENGTH(NULL) 让条件恒为 NULL、永远删不掉。
func nonNilBytes(b []byte) []byte {
	if b == nil {
		return []byte{}
	}
	return b
}

// placeholders 返回 n 个以逗号分隔的 "?"(n ≥ 1;调用方已挡掉空列表)。
func placeholders(n int) string {
	return strings.TrimSuffix(strings.Repeat("?,", n), ",")
}

func uint64Args(ids []uint64) []any {
	out := make([]any, len(ids))
	for i, id := range ids {
		out[i] = id
	}
	return out
}
