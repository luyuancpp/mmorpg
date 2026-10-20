package debugutil

import (
	"context"
	"database/sql"
	"errors"
	"fmt"

	"github.com/luyuancpp/proto2mysql"
	"github.com/luyuancpp/proto2mysql/pbconv"
	"google.golang.org/protobuf/proto"
)

// 玩家行的读写一律经 proto2mysql 的同一套 SQL 生成与行 ↔ 消息转换(go/db 落库就是它),
// 只是跑在调用方自己的事务上:proto2mysql 的事务入口不收 TxOptions,而导出要可重复读快照、导入要 RC。

// Queryer 是 *sql.DB 与 *sql.Tx 共有的读接口。
type Queryer interface {
	QueryContext(ctx context.Context, query string, args ...any) (*sql.Rows, error)
}

// Execer 是 *sql.DB 与 *sql.Tx 共有的写接口。
type Execer interface {
	ExecContext(ctx context.Context, query string, args ...any) (sql.Result, error)
}

func messageTable(msg proto.Message) (*proto2mysql.MessageTable, error) {
	model := proto2mysql.NewDB()
	model.RegisterTable(msg)
	table, ok := model.Tables[proto2mysql.GetTableName(msg)]
	if !ok || table == nil {
		return nil, fmt.Errorf("register table for %s failed", proto2mysql.GetTableName(msg))
	}
	return table, nil
}

// ReadRowsWhere 按 whereClause(可带 ORDER BY / LIMIT,参数化)读多行,每行用 newMsg 新建一条消息。
func ReadRowsWhere(ctx context.Context, q Queryer, newMsg func() proto.Message, whereClause string, args ...any) ([]proto.Message, error) {
	table, err := messageTable(newMsg())
	if err != nil {
		return nil, err
	}
	statement := table.GetSelectSQLByWhereWithArgs(whereClause, args)
	rows, err := q.QueryContext(ctx, statement.Sql, statement.Args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return nil, err
	}
	var out []proto.Message
	for rows.Next() {
		raw := make([]sql.RawBytes, len(columns))
		scans := make([]any, len(columns))
		for i := range raw {
			scans[i] = &raw[i]
		}
		if err := rows.Scan(scans...); err != nil {
			return nil, err
		}
		// 与 proto2mysql 的 scanRowStrings 同义:NULL 读成空串,ParseFromString 按字段声明序还原。
		values := make([]string, len(raw))
		for i, v := range raw {
			values[i] = string(v)
		}
		msg := newMsg()
		if err := pbconv.ParseFromString(msg, values); err != nil {
			return nil, err
		}
		out = append(out, msg)
	}
	return out, rows.Err()
}

// ErrMultipleRows 表示按主键读到了多行(表结构与 proto 不一致)。
var ErrMultipleRows = errors.New("按 player_id 读到多行")

// ReadPlayerRow 按 player_id 读一行玩家表。没有行时 found=false。
func ReadPlayerRow(ctx context.Context, q Queryer, newMsg func() proto.Message, playerID uint64) (proto.Message, bool, error) {
	rows, err := ReadRowsWhere(ctx, q, newMsg, "`player_id` = ?", playerID)
	if err != nil {
		return nil, false, err
	}
	switch len(rows) {
	case 0:
		return nil, false, nil
	case 1:
		return rows[0], true, nil
	default:
		return nil, false, ErrMultipleRows
	}
}

// ReplacePlayerRow 在调用方的事务里按主键删掉旧行再插入 msg。
func ReplacePlayerRow(ctx context.Context, tx Execer, msg proto.Message) error {
	table, err := messageTable(msg)
	if err != nil {
		return err
	}
	del, err := table.GetDeleteSQLWithArgs(msg)
	if err != nil {
		return fmt.Errorf("build delete: %w", err)
	}
	if _, err := tx.ExecContext(ctx, del.Sql, del.Args...); err != nil {
		return fmt.Errorf("delete old row: %w", err)
	}
	ins, err := table.GetInsertSQLWithArgs(msg)
	if err != nil {
		return fmt.Errorf("build insert: %w", err)
	}
	if _, err := tx.ExecContext(ctx, ins.Sql, ins.Args...); err != nil {
		return fmt.Errorf("insert row: %w", err)
	}
	return nil
}
