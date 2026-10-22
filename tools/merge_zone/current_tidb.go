package main

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"github.com/go-sql-driver/mysql"
)

const (
	currentTiDBVersionSQL = "SELECT VERSION(),@@version_comment"
	currentTiDBSessionSQL = "SELECT @@tidb_txn_mode,@@transaction_isolation,@@tidb_replica_read,@@tidb_read_consistency,@@tidb_snapshot,@@tidb_read_staleness,@@tidb_low_resolution_tso"
)

var errCurrentTiDB = errors.New("全局SQL连接必须使用真实TiDB当前强读会话")

// 玩家行、Team原件、路由收集与审计共用同一规则。按大小写无关删除旧参数，避免驱动SET时同名别名覆盖安全值。
// 每条新连接都绑定，不只修改探针碰到的那一条连接；调用者仍须在交付句柄前读回策略。
func pinCurrentTiDB(cfg *mysql.Config) {
	if cfg.Params == nil {
		cfg.Params = map[string]string{}
	}
	for key := range cfg.Params {
		switch strings.ToLower(key) {
		case mysqlIsolationParam, mysqlLegacyIsolationParam, "tidb_txn_mode", "tidb_replica_read", "tidb_read_consistency", "tidb_snapshot", "tidb_read_staleness", "tidb_low_resolution_tso":
			delete(cfg.Params, key)
		}
	}
	for key, value := range map[string]string{
		mysqlIsolationParam: mysqlReadCommitted, "tidb_txn_mode": "'pessimistic'", "tidb_replica_read": "'leader'", "tidb_read_consistency": "'strict'", "tidb_snapshot": "''", "tidb_read_staleness": "0", "tidb_low_resolution_tso": "OFF",
	} {
		cfg.Params[key] = value
	}
}

func requireCurrentTiDB(ctx context.Context, db *sql.DB) error {
	var version, comment string
	if err := db.QueryRowContext(ctx, currentTiDBVersionSQL).Scan(&version, &comment); err != nil {
		return fmt.Errorf("%w: 无法读取引擎: %w", errCurrentTiDB, err)
	}
	if !strings.Contains(strings.ToLower(version+comment), "tidb") {
		return fmt.Errorf("%w: 拒绝MySQL历史写入域", errCurrentTiDB)
	}
	var mode, isolation, replica, consistency, snapshot string
	var stale, low int64
	if err := db.QueryRowContext(ctx, currentTiDBSessionSQL).Scan(&mode, &isolation, &replica, &consistency, &snapshot, &stale, &low); err != nil {
		return fmt.Errorf("%w: 无法读回强读策略: %w", errCurrentTiDB, err)
	}
	if mode != "pessimistic" || isolation != "READ-COMMITTED" || replica != "leader" || consistency != "strict" || snapshot != "" || stale != 0 || low != 0 {
		return fmt.Errorf("%w: 读回策略不符，拒绝历史快照/旧副本/低精度TSO", errCurrentTiDB)
	}
	return nil
}

// 不使用log.Fatal，审计入口必须将连接失败保留为INFRA/exit2，不能把未执行误报为无阻塞。
func openCurrentTiDB(ctx context.Context, raw string) (*sql.DB, error) {
	dsn, err := readCommittedDSN(raw)
	if err != nil {
		return nil, err
	}
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		return nil, err
	}
	if err := requireCurrentTiDB(ctx, db); err != nil {
		_ = db.Close()
		return nil, err
	}
	return db, nil
}
