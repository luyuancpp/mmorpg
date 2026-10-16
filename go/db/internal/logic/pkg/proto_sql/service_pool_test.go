package proto_sql

// 业务连接池的连接参数(launch-todo §2.3 第 9 条)与探活绕过连接池(第 10 条)。
//
// 改 config.AppConfig 的用例在 t.Cleanup 里还原,不能 t.Parallel。

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"sync"
	"testing"
	"time"

	"db/internal/config"
	"db/internal/storeerr"
)

// withDatabaseConfig 临时改写 AppConfig 的数据库段,用例结束时整份还原。
func withDatabaseConfig(t *testing.T, mutate func(d *config.DatabaseConfig)) {
	t.Helper()
	saved := config.AppConfig
	t.Cleanup(func() { config.AppConfig = saved })
	mutate(&config.AppConfig.ServerConfig.Database)
}

func TestNewServiceMysqlConfig_SetsTimeoutsAndSessionWaits(t *testing.T) {
	withDatabaseConfig(t, func(d *config.DatabaseConfig) {
		d.DialTimeoutSeconds = config.DefaultDatabaseDialTimeoutSeconds
		d.ReadTimeoutSeconds = config.DefaultDatabaseReadTimeoutSeconds
		d.WriteTimeoutSeconds = config.DefaultDatabaseWriteTimeoutSeconds
		d.SessionLockWaitSeconds = config.DefaultDatabaseSessionLockWaitSeconds
		d.SessionInnodbLockWaitSeconds = config.DefaultDatabaseSessionInnodbLockWaitSeconds
	})

	cfg := newServiceMysqlConfig()

	if cfg.Timeout != 5*time.Second || cfg.ReadTimeout != 30*time.Second || cfg.WriteTimeout != 30*time.Second {
		t.Fatalf("timeouts = dial %v read %v write %v, want 5s / 30s / 30s", cfg.Timeout, cfg.ReadTimeout, cfg.WriteTimeout)
	}
	if got := cfg.Params["lock_wait_timeout"]; got != "10" {
		t.Fatalf("lock_wait_timeout = %q, want 10", got)
	}
	if got := cfg.Params["innodb_lock_wait_timeout"]; got != "10" {
		t.Fatalf("innodb_lock_wait_timeout = %q, want 10", got)
	}
	if got := cfg.Params["sql_mode"]; got != "'STRICT_TRANS_TABLES'" {
		t.Fatalf("sql_mode = %q, the strict session mode must be kept", got)
	}
}

// 负数表示不设:驱动不加超时,也不发对应的会话变量(例如 TiDB 不接受 lock_wait_timeout 时写 -1)。
func TestNewServiceMysqlConfig_NegativeDisables(t *testing.T) {
	withDatabaseConfig(t, func(d *config.DatabaseConfig) {
		d.DialTimeoutSeconds = -1
		d.ReadTimeoutSeconds = -1
		d.WriteTimeoutSeconds = -1
		d.SessionLockWaitSeconds = -1
		d.SessionInnodbLockWaitSeconds = -1
	})

	cfg := newServiceMysqlConfig()

	if cfg.Timeout != 0 || cfg.ReadTimeout != 0 || cfg.WriteTimeout != 0 {
		t.Fatalf("negative seconds must disable the timeouts, got dial %v read %v write %v", cfg.Timeout, cfg.ReadTimeout, cfg.WriteTimeout)
	}
	for _, name := range []string{"lock_wait_timeout", "innodb_lock_wait_timeout"} {
		if v, ok := cfg.Params[name]; ok {
			t.Fatalf("%s = %q must not be sent when disabled", name, v)
		}
	}
}

// 迁移入口用的 NewMysqlConfig 不带业务池的超时与会话锁等待:DDL 可能合法地跑几分钟。
func TestNewMysqlConfig_HasNoServiceTimeouts(t *testing.T) {
	withDatabaseConfig(t, func(d *config.DatabaseConfig) {
		d.DialTimeoutSeconds = 5
		d.ReadTimeoutSeconds = 30
		d.WriteTimeoutSeconds = 30
		d.SessionLockWaitSeconds = 10
		d.SessionInnodbLockWaitSeconds = 10
	})

	cfg := NewMysqlConfig()

	if cfg.Timeout != 0 || cfg.ReadTimeout != 0 || cfg.WriteTimeout != 0 {
		t.Fatalf("migration config must not carry service timeouts, got dial %v read %v write %v", cfg.Timeout, cfg.ReadTimeout, cfg.WriteTimeout)
	}
	for _, name := range []string{"lock_wait_timeout", "innodb_lock_wait_timeout"} {
		if _, ok := cfg.Params[name]; ok {
			t.Fatalf("migration config must not set %s", name)
		}
	}
}

// ── 探活替身:database/sql/driver 的 Connector / Conn / Pinger ──

type poolTestConn struct {
	pingErr error
	pings   *int
	closes  *int
	mu      *sync.Mutex
}

func (c *poolTestConn) Prepare(string) (driver.Stmt, error) { return nil, errors.New("not supported") }
func (c *poolTestConn) Begin() (driver.Tx, error)           { return nil, errors.New("not supported") }

func (c *poolTestConn) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()
	*c.closes++
	return nil
}

func (c *poolTestConn) Ping(context.Context) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	*c.pings++
	return c.pingErr
}

var _ driver.Pinger = (*poolTestConn)(nil)

type poolTestConnector struct {
	mu         sync.Mutex
	connectErr error
	pingErr    error
	connects   int
	pings      int
	closes     int
}

func (c *poolTestConnector) Connect(context.Context) (driver.Conn, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.connects++
	if c.connectErr != nil {
		return nil, c.connectErr
	}
	return &poolTestConn{pingErr: c.pingErr, pings: &c.pings, closes: &c.closes, mu: &c.mu}, nil
}

func (c *poolTestConnector) Driver() driver.Driver { return poolTestDriver{} }

func (c *poolTestConnector) counts() (connects, pings, closes int) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.connects, c.pings, c.closes
}

type poolTestDriver struct{}

func (poolTestDriver) Open(string) (driver.Conn, error) { return nil, errors.New("use the connector") }

// newProbeStore 建一个连接池走 pool 连接器、探活走 probe 连接器的库。
func newProbeStore(t *testing.T, pool, probe *poolTestConnector) *GameDB {
	t.Helper()
	db := sql.OpenDB(pool)
	t.Cleanup(func() { _ = db.Close() })
	return &GameDB{DB: db, Connector: probe}
}

// 第 10 条:有连接器时探活新建一条连接、Ping、关掉,不碰连接池。
func TestPingStore_UsesConnectorNotPool(t *testing.T) {
	pool, probe := &poolTestConnector{}, &poolTestConnector{}
	store := newProbeStore(t, pool, probe)

	if err := pingStore(context.Background(), store); err != nil {
		t.Fatalf("pingStore: %v", err)
	}
	connects, pings, closes := probe.counts()
	if connects != 1 || pings != 1 || closes != 1 {
		t.Fatalf("probe connector connects/pings/closes = %d/%d/%d, want 1/1/1", connects, pings, closes)
	}
	if poolConnects, _, _ := pool.counts(); poolConnects != 0 {
		t.Fatalf("the probe must not borrow from the pool, pool connects = %d", poolConnects)
	}
}

// 池满(唯一一条连接被借走)时探活不排队:在 1 秒预算内返回可用。
func TestPingStore_FullPoolDoesNotBlock(t *testing.T) {
	pool, probe := &poolTestConnector{}, &poolTestConnector{}
	store := newProbeStore(t, pool, probe)
	store.DB.SetMaxOpenConns(1)
	held, err := store.DB.Conn(context.Background())
	if err != nil {
		t.Fatalf("hold the only pool connection: %v", err)
	}
	t.Cleanup(func() { _ = held.Close() })

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	if err := pingStore(ctx, store); err != nil {
		t.Fatalf("a full pool must not make a live database look unreachable: %v", err)
	}
	if ctx.Err() != nil {
		t.Fatal("pingStore waited for the full pool")
	}
}

// 池里只剩死连接(Ping 返回 ErrBadConn)不影响探活结论。
func TestPingStore_DeadPoolConnsDoNotMatter(t *testing.T) {
	pool := &poolTestConnector{pingErr: driver.ErrBadConn}
	probe := &poolTestConnector{}
	store := newProbeStore(t, pool, probe)
	idle, err := store.DB.Conn(context.Background())
	if err != nil {
		t.Fatalf("open a pool connection: %v", err)
	}
	_ = idle.Close() // 还回池里成为空闲连接

	if err := pingStore(context.Background(), store); err != nil {
		t.Fatalf("dead idle pool connections must not matter: %v", err)
	}
	if _, poolPings, _ := pool.counts(); poolPings != 0 {
		t.Fatalf("the probe must not ping pool connections, got %d pings", poolPings)
	}
}

// 连接器拨号失败:原样返回,storeerr 归为 conn_lost(由注册表决定是否冷却)。
func TestPingStore_ConnectErrorIsConnLost(t *testing.T) {
	pool := &poolTestConnector{}
	probe := &poolTestConnector{connectErr: errDialRefused}
	store := newProbeStore(t, pool, probe)

	err := pingStore(context.Background(), store)
	if err == nil {
		t.Fatal("a refused dial must be reported")
	}
	if kind := storeerr.Classify(err); kind != storeerr.KindConnLost {
		t.Fatalf("storeerr.Classify = %v, want KindConnLost", kind)
	}
}
