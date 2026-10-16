package inbox

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"

	inboxpb "proto/player_inbox"

	"shared/assetop"

	"github.com/go-sql-driver/mysql"
	"github.com/zeromicro/go-zero/core/logx"
)

// Store 是收件箱的存储与协议核心(设计 §6.2、§7.8、§7.9、§9)。
//
// 并发与锁(设计 §10):
//   - 所有事务都是 READ COMMITTED,经 assetop.WithTxRetryConfig 执行,并**显式注入**可重试判定
//     (1213 / 1205 / 9007,IsRetryableTxError)—— IsRetryable 为 nil 时一次都不重试(assetop/seq.go)。
//     RC 的理由同 assetop/seq.go:TiDB 悲观事务没有间隙锁;锁内的普通读必须看得见前一个持锁者已提交的行。
//   - 锁顺序固定:流行(主键 FOR UPDATE)→ 条目行(只按主键,seq 升序)→ 审计行插入。
//     条目行一律先普通读、再按主键条件写,**不做**二级索引上的加锁读(二级索引加锁读与主键 CAS 加锁顺序相反会成环)。
//   - 每个事务只碰一个 (player, domain),同一条流内天然串行,不同流之间不存在环。
//   - Redis 读只在事务外进行,不在持有行锁时等待 Redis。
//
// Store 构造后只读,可被多个 RPC 与后台循环并发使用。
type Store struct {
	db       *sql.DB
	registry *Registry
	allowed  map[inboxpb.PlayerInboxMessageType]struct{}
	players  PlayerDirectory
	epochs   EpochReader
	notifier Notifier
	now      func() time.Time
	txCfg    assetop.TxRetryConfig
}

// PlayerDirectory 是入箱前的玩家存在校验(设计 §6.2 第 2 步,查归属区映射)。
// (false, nil) = 映射里没有这名玩家;err 非 nil = 查询失败(故障,fail-closed 不入箱)。
type PlayerDirectory interface {
	PlayerExists(ctx context.Context, playerID uint64) (bool, error)
}

// EpochReader 读核心 Redis 里 player:{id}:owner_epoch 的当前值 R(围栏上界,IB-8)。键不存在返回 0。
type EpochReader interface {
	OwnerEpoch(ctx context.Context, playerID uint64) (uint64, error)
}

// StoreDeps 是 Store 的显式依赖(AGENTS §11.2)。Notifier 可为 nil:不发入箱后的那次提醒,
// 全部交给补发循环(单测与 -migrate 之外的形态不应为 nil)。
type StoreDeps struct {
	DB       *sql.DB
	Registry *Registry
	// Allowed 是 PlayerInbox.EnqueueTypes 解析后的白名单;nil / 空 = 全部停用。
	Allowed  map[inboxpb.PlayerInboxMessageType]struct{}
	Players  PlayerDirectory
	Epochs   EpochReader
	Notifier Notifier
	// Now 是时间源,生产为 time.Now;集成测试注入固定时钟验证「同一毫秒并发入箱 seq 连续」。
	Now func() time.Time
}

// NewStore 校验依赖并构造 Store。缺任何一项必需依赖都返回错误(启动致命)。
func NewStore(d StoreDeps) (*Store, error) {
	if d.DB == nil || d.Registry == nil || d.Players == nil || d.Epochs == nil {
		return nil, errors.New("inbox: NewStore requires DB, Registry, Players and Epochs")
	}
	if err := d.Registry.CheckAllowlist(d.Allowed); err != nil {
		return nil, err
	}
	now := d.Now
	if now == nil {
		now = time.Now
	}
	allowed := make(map[inboxpb.PlayerInboxMessageType]struct{}, len(d.Allowed))
	for t := range d.Allowed {
		allowed[t] = struct{}{}
	}
	return &Store{
		db:       d.DB,
		registry: d.Registry,
		allowed:  allowed,
		players:  d.Players,
		epochs:   d.Epochs,
		notifier: d.Notifier,
		now:      now,
		txCfg:    TxRetryConfig(),
	}, nil
}

// Registry 返回 Store 使用的类型注册表。
func (s *Store) Registry() *Registry { return s.registry }

// TxRetryConfig 是本服务全部事务的参数:assetop 默认值(RC、3 次、10ms 起退避 + 抖动),再**注入**
// 可重试判定 IsRetryableTxError。单测钉住注入不为 nil 且覆盖 1213 / 1205 / 9007(设计 §10)。
func TxRetryConfig() assetop.TxRetryConfig {
	cfg := assetop.DefaultTxRetryConfig()
	cfg.IsRetryable = IsRetryableTxError
	return cfg
}

// IsRetryableTxError 判定「事务已整体回滚,重跑是安全的」:1213 死锁、1205 锁等待超时、9007 TiDB 写冲突。
// 9007 必须在内:迁到 TiDB 之后它会取代死锁成为最常见的一类,漏判就等于不重试(assetop/seq.go 的注释)。
func IsRetryableTxError(err error) bool {
	var myErr *mysql.MySQLError
	if !errors.As(err, &myErr) {
		return false
	}
	switch myErr.Number {
	case 1213, 1205, 9007:
		return true
	default:
		return false
	}
}

// isDuplicateKeyError 判定 1062(唯一键 / 主键冲突)。
func isDuplicateKeyError(err error) bool {
	var myErr *mysql.MySQLError
	return errors.As(err, &myErr) && myErr.Number == 1062
}

// nowMs 取一次时间转成 Unix 毫秒;早于 1970 时返回 0(seq 分配会据此拒绝)。
func (s *Store) nowMs() uint64 {
	ms := s.now().UnixMilli()
	if ms < 0 {
		return 0
	}
	return uint64(ms)
}

// ── SQL 片段 ─────────────────────────────────────────────────────────────
// 列名一律加反引号:operator / action / state 等与 SQL 关键字同名(player_inbox_table.proto 头注)。

const (
	streamTable = "player_inbox_stream"
	itemTable   = "player_inbox_item"
	auditTable  = "player_inbox_audit"

	streamColumns = "`shard`,`player_id`,`domain`,`next_seq`,`fence_epoch`,`open_count`,`notify_due_ms`,`created_ms`,`updated_ms`"
	itemColumns   = "`shard`,`player_id`,`domain`,`seq`,`msg_type`,`producer_kind`,`request_id`,`content_sha256`,`payload`," +
		"`state`,`directive`,`void_reason`,`delivery_count`,`last_delivered_epoch`,`last_delivered_ms`,`outcome_reason`," +
		"`finalized_by`,`decided_epoch`,`issued_at_ms`,`created_ms`,`updated_ms`,`operator_name`,`note`"

	streamPK = "`shard` = ? AND `player_id` = ? AND `domain` = ?"
	itemPK   = "`shard` = ? AND `player_id` = ? AND `domain` = ? AND `seq` = ?"
)

// terminalStatesSQL 是终态集合的 SQL 字面量(APPLIED / REJECTED / VOIDED / SETTLED_UNKNOWN)。
// 由枚举值拼出而不是手写数字,改枚举时跟着变。
var terminalStatesSQL = fmt.Sprintf("(%d,%d,%d,%d)",
	int32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_APPLIED),
	int32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_REJECTED),
	int32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_VOIDED),
	int32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_SETTLED_UNKNOWN))

// sqlRunner 是 *sql.DB 与 *sql.Tx 的公共子集:同一条读在事务内外复用。
type sqlRunner interface {
	ExecContext(ctx context.Context, query string, args ...any) (sql.Result, error)
	QueryContext(ctx context.Context, query string, args ...any) (*sql.Rows, error)
	QueryRowContext(ctx context.Context, query string, args ...any) *sql.Row
}

type rowScanner interface {
	Scan(dest ...any) error
}

func scanStream(sc rowScanner) (*inboxpb.PlayerInboxStream, error) {
	r := &inboxpb.PlayerInboxStream{}
	if err := sc.Scan(&r.Shard, &r.PlayerId, &r.Domain, &r.NextSeq, &r.FenceEpoch, &r.OpenCount,
		&r.NotifyDueMs, &r.CreatedMs, &r.UpdatedMs); err != nil {
		return nil, err
	}
	if err := checkRowShard(r.GetShard(), r.GetPlayerId()); err != nil {
		return nil, err
	}
	return r, nil
}

func scanItem(sc rowScanner) (*inboxpb.PlayerInboxItemRow, error) {
	r := &inboxpb.PlayerInboxItemRow{}
	if err := sc.Scan(&r.Shard, &r.PlayerId, &r.Domain, &r.Seq, &r.MsgType, &r.ProducerKind, &r.RequestId,
		&r.ContentSha256, &r.Payload, &r.State, &r.Directive, &r.VoidReason, &r.DeliveryCount,
		&r.LastDeliveredEpoch, &r.LastDeliveredMs, &r.OutcomeReason, &r.FinalizedBy, &r.DecidedEpoch,
		&r.IssuedAtMs, &r.CreatedMs, &r.UpdatedMs, &r.OperatorName, &r.Note); err != nil {
		return nil, err
	}
	if err := checkRowShard(r.GetShard(), r.GetPlayerId()); err != nil {
		return nil, err
	}
	return r, nil
}

// checkRowShard 是读出时的断言(设计 §4.2:TiDB 不执行 CHECK,shard 一致性靠唯一计算函数 + 读出断言 + 单测)。
func checkRowShard(shard uint32, playerID uint64) error {
	if want := RowShard(playerID); shard != want {
		return fmt.Errorf("inbox: corrupt row shard=%d for player_id=%d (want %d)", shard, playerID, want)
	}
	return nil
}

// readStream 普通读流行(不加锁);不存在返回 (nil, nil)。
func readStream(ctx context.Context, q sqlRunner, shard uint32, playerID uint64, domain inboxpb.PlayerInboxDomain) (*inboxpb.PlayerInboxStream, error) {
	row := q.QueryRowContext(ctx, "SELECT "+streamColumns+" FROM "+streamTable+" WHERE "+streamPK,
		shard, playerID, uint32(domain))
	st, err := scanStream(row)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("inbox: read stream player=%d domain=%d: %w", playerID, domain, err)
	}
	return st, nil
}

// lockStream 对流行加 FOR UPDATE(锁序第 1);不存在返回 (nil, nil)。
func lockStream(ctx context.Context, tx *sql.Tx, shard uint32, playerID uint64, domain inboxpb.PlayerInboxDomain) (*inboxpb.PlayerInboxStream, error) {
	row := tx.QueryRowContext(ctx, "SELECT "+streamColumns+" FROM "+streamTable+" WHERE "+streamPK+" FOR UPDATE",
		shard, playerID, uint32(domain))
	st, err := scanStream(row)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("inbox: lock stream player=%d domain=%d: %w", playerID, domain, err)
	}
	return st, nil
}

// readItemByRequest 按唯一键 (shard, player_id, producer_kind, request_id) 普通读;不存在返回 (nil, nil)。
func readItemByRequest(ctx context.Context, q sqlRunner, shard uint32, playerID uint64,
	producer inboxpb.PlayerInboxProducerKind, requestID uint64) (*inboxpb.PlayerInboxItemRow, error) {
	row := q.QueryRowContext(ctx, "SELECT "+itemColumns+" FROM "+itemTable+
		" WHERE `shard` = ? AND `player_id` = ? AND `producer_kind` = ? AND `request_id` = ?",
		shard, playerID, uint32(producer), requestID)
	it, err := scanItem(row)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("inbox: read item by request player=%d request_id=%d: %w", playerID, requestID, err)
	}
	return it, nil
}

// readItem 按主键普通读一行;不存在返回 (nil, nil)。
func readItem(ctx context.Context, q sqlRunner, shard uint32, playerID uint64, domain inboxpb.PlayerInboxDomain, seq uint64) (*inboxpb.PlayerInboxItemRow, error) {
	row := q.QueryRowContext(ctx, "SELECT "+itemColumns+" FROM "+itemTable+" WHERE "+itemPK,
		shard, playerID, uint32(domain), seq)
	it, err := scanItem(row)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("inbox: read item player=%d domain=%d seq=%d: %w", playerID, domain, seq, err)
	}
	return it, nil
}

// readItemsBySeq 按主键批量普通读(seq IN (...)),返回 seq → 行。
func readItemsBySeq(ctx context.Context, q sqlRunner, shard uint32, playerID uint64, domain inboxpb.PlayerInboxDomain,
	seqs []uint64) (map[uint64]*inboxpb.PlayerInboxItemRow, error) {
	out := make(map[uint64]*inboxpb.PlayerInboxItemRow, len(seqs))
	if len(seqs) == 0 {
		return out, nil
	}
	args := make([]any, 0, 3+len(seqs))
	args = append(args, shard, playerID, uint32(domain))
	for _, seq := range seqs {
		args = append(args, seq)
	}
	query := "SELECT " + itemColumns + " FROM " + itemTable +
		" WHERE `shard` = ? AND `player_id` = ? AND `domain` = ? AND `seq` IN (" + placeholders(len(seqs)) + ")"
	rows, err := q.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, fmt.Errorf("inbox: read items by seq player=%d domain=%d: %w", playerID, domain, err)
	}
	defer rows.Close()
	for rows.Next() {
		it, err := scanItem(rows)
		if err != nil {
			return nil, err
		}
		out[it.GetSeq()] = it
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return out, nil
}

// queryItems 跑一条返回完整条目行的查询。
func queryItems(ctx context.Context, q sqlRunner, query string, args ...any) ([]*inboxpb.PlayerInboxItemRow, error) {
	rows, err := q.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []*inboxpb.PlayerInboxItemRow
	for rows.Next() {
		it, err := scanItem(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, it)
	}
	return out, rows.Err()
}

func placeholders(n int) string {
	if n <= 0 {
		return ""
	}
	return strings.TrimSuffix(strings.Repeat("?,", n), ",")
}

// requireOneRow 断言一条条件写恰好命中一行。所有条件写都在流行锁下进行,命中数不对只可能是缺陷或数据已矛盾:
// 返回错误让整个事务回滚(fail-closed),而不是带着半套状态提交。
func requireOneRow(res sql.Result, what string) error {
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("inbox: %s rows affected: %w", what, err)
	}
	if n != 1 {
		return fmt.Errorf("inbox: %s affected %d rows, want 1 (state changed under stream lock?)", what, n)
	}
	return nil
}

// InsertAudit 在调用方事务里写一行审计(INSERT IGNORE:每条目每种动作一行,重复调用不报错,设计 §4.1)。
// shard 由本函数按 player_id 计算,调用方传入的值被忽略。
func InsertAudit(ctx context.Context, tx *sql.Tx, a *inboxpb.PlayerInboxAudit) error {
	if a == nil {
		return errors.New("inbox: nil audit row")
	}
	_, err := tx.ExecContext(ctx, "INSERT IGNORE INTO "+auditTable+
		" (`shard`,`player_id`,`domain`,`seq`,`action`,`operator`,`peer_addr`,`reason`,`created_ms`) VALUES (?,?,?,?,?,?,?,?,?)",
		RowShard(a.GetPlayerId()), a.GetPlayerId(), a.GetDomain(), a.GetSeq(), a.GetAction(),
		a.GetOperator(), a.GetPeerAddr(), a.GetReason(), a.GetCreatedMs())
	if err != nil {
		return fmt.Errorf("inbox: insert audit player=%d seq=%d action=%d: %w", a.GetPlayerId(), a.GetSeq(), a.GetAction(), err)
	}
	return nil
}

// ensureStream 补建流行(设计 §6.2 第 6 步):INSERT IGNORE、next_seq=1,在**主事务之外**的独立短事务里执行,
// 遇 1213 / 1205 / 9007 有界重试。
//
// 为什么放在主事务外:照 assetop.EnsureSeqRow。两个并发请求在事务里对同一个不存在的行做
// 「加锁读、发现没有、插入」时,共享锁升级会互等成死锁;单独的插入短事务提交之后,主事务的 FOR UPDATE
// 总能锁到一行已提交的记录上。
func (s *Store) ensureStream(ctx context.Context, shard uint32, playerID uint64, domain inboxpb.PlayerInboxDomain, nowMs uint64) error {
	return assetop.WithTxRetryConfig(ctx, s.db, s.txCfg, func(tx *sql.Tx) error {
		_, err := tx.ExecContext(ctx, "INSERT IGNORE INTO "+streamTable+" ("+streamColumns+") VALUES (?,?,?,1,0,0,0,?,?)",
			shard, playerID, uint32(domain), nowMs, nowMs)
		if err != nil {
			return fmt.Errorf("inbox: ensure stream player=%d domain=%d: %w", playerID, domain, err)
		}
		return nil
	})
}

// ClearNotifyDue 在提醒成功后把 notify_due_ms 置 0,**以 next_seq 做 CAS 令牌**(设计 §6.3):
// 这期间只要又有新的入箱,next_seq 就变了,CAS 失败,notify_due_ms 留给补发循环 —— 最多多发一次提醒,不会丢。
// 不用毫秒值当令牌:同一毫秒两次入箱会让旧令牌碰巧相等(ABA)。返回是否真的清掉了。
func (s *Store) ClearNotifyDue(ctx context.Context, playerID uint64, domain inboxpb.PlayerInboxDomain, expectNextSeq uint64) (bool, error) {
	res, err := s.db.ExecContext(ctx, "UPDATE "+streamTable+" SET `notify_due_ms` = 0 WHERE "+streamPK+
		" AND `next_seq` = ? AND `notify_due_ms` <> 0", RowShard(playerID), playerID, uint32(domain), expectNextSeq)
	if err != nil {
		return false, fmt.Errorf("inbox: clear notify_due player=%d domain=%d: %w", playerID, domain, err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, err
	}
	return n == 1, nil
}

// notifyAndClear 发一次在线提醒;提醒已落定(送达 / 离线 / 等待落点)时按 next_seq 做 CAS 清 notify_due_ms。
// 失败只记日志,不影响调用方(入箱的返回值、补发循环的下一行)。
func (s *Store) notifyAndClear(ctx context.Context, playerID uint64, domain inboxpb.PlayerInboxDomain, expectNextSeq uint64) NotifyResult {
	if s.notifier == nil {
		return NotifyError
	}
	result := s.notifier.Notify(ctx, playerID, domain)
	if !result.Settled() {
		return result
	}
	if _, err := s.ClearNotifyDue(ctx, playerID, domain, expectNextSeq); err != nil {
		logx.WithContext(ctx).Errorf("[player_inbox] Notify settled but clearing notify_due failed player=%d domain=%d: %v "+
			"(resend loop will notify again; harmless)", playerID, domain, err)
	}
	return result
}
