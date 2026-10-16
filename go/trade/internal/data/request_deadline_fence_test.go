package data

// 请求截止之后不再提交(docs/design/client-request-ordering.md §6.1 / O11 / §14 第 4 条)的回归测试(不连库)。
//
// 客户端请求路径上 trade 只有收藏两处写(SetFavorite → InsertFavorite / DeleteFavorite)。gate 对同一会话的
// 路由类请求单飞,路由服回超时就放行下一条:截止之后才落库的「收藏」会晚于紧随其后的「取消收藏」,
// 玩家看到取消成功、商品却仍在收藏里。
//
// 用录制驱动把「有没有语句到达驱动」钉死:database/sql 取连接前先看 ctx,InsertFavorite 的
// assetop.WithTxRetry 每次尝试前也先看 ctx —— 两者任一被改成不受请求 ctx 约束,这里就判红。
// 带 integration 标签的真库用例不在默认 go test 里跑,所以这条必须不依赖 MySQL。

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"strings"
	"sync"
	"testing"
	"time"

	tradepb "proto/trade"
)

type deadlineRecorder struct {
	mu    sync.Mutex
	stmts []string
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

var errDeadlineDriver = errors.New("截止栅栏录制驱动:这条语句不该到达驱动")

type deadlineConnector struct{ r *deadlineRecorder }

func (c deadlineConnector) Connect(context.Context) (driver.Conn, error) {
	return deadlineConn{r: c.r}, nil
}
func (c deadlineConnector) Driver() driver.Driver { return deadlineDriver{r: c.r} }

type deadlineDriver struct{ r *deadlineRecorder }

func (d deadlineDriver) Open(string) (driver.Conn, error) { return deadlineConn{r: d.r}, nil }

// deadlineConn 录下每条到达驱动的语句(含 BEGIN)并一律报错:截止之后本不该有任何语句到达这里。
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
	return nil, errDeadlineDriver
}

func TestFavoriteWritesAfterRequestDeadlineSendNoStatement(t *testing.T) {
	cases := []struct {
		name string
		call func(ctx context.Context, repo *ListingRepo) error
	}{
		{"InsertFavorite", func(ctx context.Context, repo *ListingRepo) error {
			return repo.InsertFavorite(ctx, &tradepb.TradeFavoriteRecord{PlayerId: 1002, ListingId: 11, CreatedMs: 1})
		}},
		{"DeleteFavorite", func(ctx context.Context, repo *ListingRepo) error {
			return repo.DeleteFavorite(ctx, 1002, 11)
		}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			rec := &deadlineRecorder{}
			db := sql.OpenDB(deadlineConnector{r: rec})
			t.Cleanup(func() { _ = db.Close() })
			// 单次上限给足:用例要证明的是「请求 ctx 已截止」挡住了写,不是 opTimeout。
			repo := NewListingRepo(db, time.Minute)
			ctx, cancel := context.WithCancel(context.Background())
			cancel() // 已经截止的请求 ctx(go-zero 超时拦截器到期后 handler 拿到的就是这种 ctx)

			err := tc.call(ctx, repo)

			if !errors.Is(err, context.Canceled) {
				t.Fatalf("截止之后必须如实报错(logic 定性 kServiceUnavailable),实际 err=%v", err)
			}
			if got := rec.recorded(); len(got) != 0 {
				t.Fatalf("截止之后不得有任何语句到达驱动(含 BEGIN),实际发出 %v", got)
			}
		})
	}
}
