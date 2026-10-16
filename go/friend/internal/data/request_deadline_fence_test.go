package data

// 请求截止之后不再提交(docs/design/client-request-ordering.md §6.1 / O11 / §14 第 4 条)的回归测试。
//
// gate 对同一会话的路由类请求单飞:路由服在 ForwardTimeoutMs 回超时,gate 就放行下一条。friend 的 handler
// 若在截止之后仍把写发出去,这条写就会晚于下一条请求落地,例如「同意好友 → 删好友」执行成「删好友 → 同意好友」,
// 两条都回成功、最终仍是好友(§1.3)。
//
// 本文件不连库、不受 FRIEND_TEST_MYSQL_DSN 门控:用录制驱动把「有没有语句到达驱动」钉死。
// 这是纯 SQL 层的事实 —— database/sql 在取连接(db.conn)与 Tx.Commit 时先看 ctx,ctx 已结束就不发任何语句 ——
// 所以它能抓住的回归正是「写路径上某一处改用了 context.Background() / WithoutCancel」。
// 真库上的锁语义与提交结果仍由同目录的 *_mysql_test.go 证明;这里只证明截止之后一条语句都不会发出。

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"io"
	"strings"
	"sync"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// ── 录制驱动 ────────────────────────────────────────────────
//
// 每条到达驱动的语句(含 BEGIN)都录下来。respond 为 nil 或对该语句返回 nil 时一律报错:
// 截止之后本不该有任何语句到达驱动,真到了要一眼看见,而不是悄悄"成功"。

type deadlineRecorder struct {
	mu      sync.Mutex
	stmts   []string
	respond func(query string) driver.Rows
}

func (r *deadlineRecorder) record(stmt string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.stmts = append(r.stmts, strings.Join(strings.Fields(stmt), " "))
}

func (r *deadlineRecorder) recorded() []string {
	r.mu.Lock()
	defer r.mu.Unlock()
	return append([]string(nil), r.stmts...)
}

func (r *deadlineRecorder) openDB(t *testing.T) *sql.DB {
	t.Helper()
	db := sql.OpenDB(deadlineConnector{r: r})
	t.Cleanup(func() { _ = db.Close() })
	return db
}

var errDeadlineDriver = errors.New("截止栅栏录制驱动:这条语句不该到达驱动")

type deadlineConnector struct{ r *deadlineRecorder }

func (c deadlineConnector) Connect(context.Context) (driver.Conn, error) {
	return deadlineConn{r: c.r}, nil
}
func (c deadlineConnector) Driver() driver.Driver { return deadlineDriver{r: c.r} }

type deadlineDriver struct{ r *deadlineRecorder }

func (d deadlineDriver) Open(string) (driver.Conn, error) { return deadlineConn{r: d.r}, nil }

type deadlineConn struct{ r *deadlineRecorder }

func (c deadlineConn) Prepare(query string) (driver.Stmt, error) {
	c.r.record(query)
	return nil, errDeadlineDriver
}
func (c deadlineConn) Close() error { return nil }
func (c deadlineConn) Begin() (driver.Tx, error) {
	c.r.record("BEGIN")
	return nil, errDeadlineDriver
}
func (c deadlineConn) BeginTx(context.Context, driver.TxOptions) (driver.Tx, error) {
	c.r.record("BEGIN")
	return nil, errDeadlineDriver
}
func (c deadlineConn) ExecContext(_ context.Context, query string, _ []driver.NamedValue) (driver.Result, error) {
	c.r.record(query)
	return nil, errDeadlineDriver
}
func (c deadlineConn) QueryContext(_ context.Context, query string, _ []driver.NamedValue) (driver.Rows, error) {
	c.r.record(query)
	if c.r.respond != nil {
		if rows := c.r.respond(strings.Join(strings.Fields(query), " ")); rows != nil {
			return rows, nil
		}
	}
	return nil, errDeadlineDriver
}

// oneRowThenCancel 回一行结果,读到末尾(io.EOF)时调用 cancel:模拟请求恰好在这次预读完成之后截止。
// 取消放在 EOF 而不是首行:首行之前取消会让 database/sql 直接以 ctx 错误关掉结果集,预读本身就失败了,
// 用例就测不到「预读成功之后、写之前」这一段。
type oneRowThenCancel struct {
	cols   []string
	row    []driver.Value
	sent   bool
	cancel context.CancelFunc
}

func (r *oneRowThenCancel) Columns() []string { return r.cols }
func (r *oneRowThenCancel) Close() error      { return nil }
func (r *oneRowThenCancel) Next(dest []driver.Value) error {
	if r.sent {
		r.cancel()
		return io.EOF
	}
	r.sent = true
	copy(dest, r.row)
	return nil
}

func newDeadlineRepo(t *testing.T, rec *deadlineRecorder) (*FriendRepo, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rdb := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	return NewFriendRepo(rdb, rec.openDB(t), testFriendRepoOptions(testListReadHardLimit)), mr
}

// canceledRequestCtx 是一个已经截止的请求 ctx(go-zero 超时拦截器到期、或 friend 自己的整请求预算到期后,
// handler 手里拿到的就是这种 ctx)。
func canceledRequestCtx() context.Context {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	return ctx
}

// 客户端请求路径上的全部写入口(logic 层逐个把请求 ctx 原样传进来,见 logic/friend_logic.go)。
func TestWritesAfterRequestDeadlineSendNoStatement(t *testing.T) {
	const me, target uint64 = 41001, 41002
	cases := []struct {
		name string
		call func(ctx context.Context, repo *FriendRepo) error
	}{
		{"AddFriend(CreateRequest)", func(ctx context.Context, repo *FriendRepo) error {
			_, err := repo.CreateRequest(ctx, me, target, AddFriendLimits{MaxFriends: 100, MaxPendingRequests: 100, MaxIncomingRequests: 100})
			return err
		}},
		{"AcceptFriend(AcceptRequest)", func(ctx context.Context, repo *FriendRepo) error {
			_, err := repo.AcceptRequest(ctx, 777, me, 100)
			return err
		}},
		{"RejectFriend(RejectRequest)", func(ctx context.Context, repo *FriendRepo) error {
			_, err := repo.RejectRequest(ctx, 777, me)
			return err
		}},
		{"RemoveFriend", func(ctx context.Context, repo *FriendRepo) error {
			_, err := repo.RemoveFriend(ctx, me, target)
			return err
		}},
		{"Block", func(ctx context.Context, repo *FriendRepo) error {
			_, err := repo.Block(ctx, me, target, 100)
			return err
		}},
		{"Unblock", func(ctx context.Context, repo *FriendRepo) error {
			return repo.Unblock(ctx, me, target)
		}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			rec := &deadlineRecorder{}
			repo, mr := newDeadlineRepo(t, rec)

			err := tc.call(canceledRequestCtx(), repo)

			require.ErrorIs(t, err, context.Canceled, "截止之后必须如实报错(logic 定性 ErrStorage),不能回成功")
			require.Empty(t, rec.recorded(), "截止之后不得有任何语句到达驱动(含容量行补齐与 BEGIN)")
			require.Empty(t, mr.Keys(), "没有提交就不得失效缓存")
		})
	}
}

// 截止恰好落在「按申请代次定位」之后:AcceptRequest 已经读到申请行,下一步是补齐容量行、开写事务建边。
// 这一刻之后不得再发任何语句,否则建边会晚于 gate 放行的下一条 RemoveFriend 落地。
func TestAcceptRequestDeadlineAfterLookupOpensNoWriteTransaction(t *testing.T) {
	const from, accepter, requestID uint64 = 42001, 42002, 4242
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	rec := &deadlineRecorder{}
	rec.respond = func(query string) driver.Rows {
		if !strings.HasPrefix(query, "SELECT from_player_id, to_player_id FROM friend_request WHERE request_id=") {
			return nil
		}
		return &oneRowThenCancel{
			cols:   []string{"from_player_id", "to_player_id"},
			row:    []driver.Value{int64(from), int64(accepter)},
			cancel: cancel,
		}
	}
	repo, mr := newDeadlineRepo(t, rec)

	_, err := repo.AcceptRequest(ctx, requestID, accepter, 100)

	require.ErrorIs(t, err, context.Canceled)
	stmts := rec.recorded()
	require.Len(t, stmts, 1, "预读之后截止:不得补齐容量行、不得开写事务,实际发出 %v", stmts)
	require.True(t, strings.HasPrefix(stmts[0], "SELECT from_player_id, to_player_id FROM friend_request"), stmts[0])
	require.Empty(t, mr.Keys(), "没有提交就不得失效缓存")
}
