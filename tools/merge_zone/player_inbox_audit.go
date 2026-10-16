package main

import (
	"context"
	"errors"
	"strings"

	"github.com/go-sql-driver/mysql"
)

// ── player_inbox(独占库 mmorpg_player_inbox)──────────────────────────────
//
// 玩家持久收件箱(docs/design/player-inbox-service.md §8 #17)对合服的影响只有两点,本审计只读地各查一项:
//
//  1. 消费端账本在 zone 库:player_database.inbox_ledger(proto/common/database/mysql_database_table.proto 字段 20)。
//     合服按**列名**拷贝玩家行(player_rows.go copyPlayerRows / alignedPlayerColumns)。源区或目标区库里缺这一列,
//     拷贝步骤会在写第一张表之前以 SCHEMA MISMATCH 停下 —— 那已经是维护窗口里了。这里提前在合服前审计里点名
//     缺列的库(block),让运维先对那个 zone 跑 go/db 的 `cmd/migrate up`(设计 §11.7 第 1 步)。
//     这一列缺失还意味着该 zone 的 go/db 版本低于回滚下限(设计 §8 #21):账本停在旧值、已应用未确认的处置会被重复执行。
//  2. 收件箱条目在全局库:mmorpg_player_inbox.player_inbox_item 按全局唯一的 player_id 存,没有 zone 列,
//     合服后照常投递,**不需要任何重映射**。这里只报 PENDING 条数(info),给维护单一个量级。
//     库 / 表不存在(1049 / 1146)报 warn「NOT VERIFIED」:多半是这个环境还没部署收件箱,合服本身不受影响,
//     但不能看起来像「查过了、是 0」;其余错误一律 INFRA(exit 2),理由同 auditFriend。
//
// 只登记在合服前门禁组(runAuditMode 的 !verify 分支):合服后没有收件箱特有的断言 —— 条目不随 zone 走,
// 账本随玩家行一起被拷贝,由 verify:manifest_rows 覆盖。

// defaultPlayerInboxSchema 镜像 go/player_inbox 的库名常量(config.Validate 强制 MySQL.DBName 等于它)。
// merge_zone 是独立 module,不能 import go/player_inbox,字面量靠评审守住。
const defaultPlayerInboxSchema = "mmorpg_player_inbox"

// playerInboxLedgerColumn 是 player_database 上账本列的列名(proto 字段名 inbox_ledger)。
const playerInboxLedgerColumn = "inbox_ledger"

// playerInboxItemStatePending 是 PlayerInboxItemState.PLAYER_INBOX_ITEM_PENDING(proto/player_inbox/player_inbox.proto)。
const playerInboxItemStatePending = 1

// MySQL 错误号:库不存在 / 表不存在。只用来把「没部署收件箱」与真故障分开。
const (
	mysqlErrBadDB         = 1049 // ER_BAD_DB_ERROR
	mysqlErrNoSuchTable   = 1146 // ER_NO_SUCH_TABLE
	playerDatabaseTableNm = "player_database"
)

// auditPlayerInbox 见文件头。缺列 → block;查询失败 → INFRA;收件箱库 / 表不在 → warn(NOT VERIFIED);否则 info。
func auditPlayerInbox(ctx context.Context, cfg auditConfig) ResourceAudit {
	r := ResourceAudit{Name: "player_inbox", UniqueScope: "global"}
	if cfg.db == nil {
		return infraAudit(r.Name, "no MySQL handle")
	}

	// 1) 两个 zone 库都要有账本列。information_schema 查询对不存在的库返回 0 行,不报错 ——
	//    所以「zone 库本身不存在」同样落进 missing,由运维对照 -mysql-dsn 复核,不会被当成通过。
	var missing []string
	for _, zone := range []uint32{cfg.src, cfg.dst} {
		schema := zoneDBName(zone)
		var n int
		err := cfg.db.QueryRowContext(ctx,
			`SELECT COUNT(*) FROM information_schema.COLUMNS
			  WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? AND COLUMN_NAME = ?`,
			schema, playerDatabaseTableNm, playerInboxLedgerColumn).Scan(&n)
		if err != nil {
			return infraAudit(r.Name, "check %s.%s.%s failed: %v", schema, playerDatabaseTableNm, playerInboxLedgerColumn, err)
		}
		if n == 0 {
			missing = append(missing, schema)
		}
	}
	if len(missing) > 0 {
		r.Severity = "block"
		r.Notes = playerDatabaseTableNm + "." + playerInboxLedgerColumn + " missing in " + strings.Join(missing, ", ") +
			": run go/db `cmd/migrate -command up` on that zone first (player-inbox-service.md §11.7 step 1). " +
			"Merge copies player rows by column name and will stop with SCHEMA MISMATCH otherwise; " +
			"a zone without this column is also below the inbox rollback floor (§8 #21)."
		return r
	}

	// 2) 收件箱 PENDING 量(全局表,不随 zone 走)。
	table := defaultPlayerInboxSchema + ".player_inbox_item"
	err := cfg.db.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM "+table+" WHERE state = ?", playerInboxItemStatePending).Scan(&r.SourceCount)
	if err != nil {
		var me *mysql.MySQLError
		if errors.As(err, &me) && (me.Number == mysqlErrBadDB || me.Number == mysqlErrNoSuchTable) {
			r.Severity = "warn"
			r.Notes = "NOT VERIFIED: " + table + " not found (" + me.Message + "); player_inbox not deployed here? " +
				"Ledger columns are present in both zone DBs. Inbox rows are keyed by player_id only, so the merge itself is unaffected."
			return r
		}
		return infraAudit(r.Name, "count %s failed: %v (库没建见 deploy/mysql-init/00_init_zone_dbs.sql,"+
			"表没建跑 player_inbox -f etc/player_inbox.yaml -migrate)", table, err)
	}
	r.TargetCount = r.SourceCount // 同一张全局表,不按 zone 细分
	r.Severity = "info"
	r.Notes = playerDatabaseTableNm + "." + playerInboxLedgerColumn + " present in " + zoneDBName(cfg.src) + " and " +
		zoneDBName(cfg.dst) + "; " + table + " PENDING rows are keyed by player_id only — survive merge automatically. No action."
	return r
}
