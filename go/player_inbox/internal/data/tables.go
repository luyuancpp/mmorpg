// Package data 是 player_inbox 独占库 mmorpg_player_inbox 的表清单(D-14)。
//
// 表结构的唯一事实源是 proto/player_inbox/player_inbox_table.proto:建表 / 加列只经 go/schemamigrate,
// 本包不写任何 DDL;业务 SQL 手写在 internal/inbox,行直接扫进生成的 message,不另写 Go struct(AGENTS §3)。
package data

import (
	inboxpb "proto/player_inbox"

	"google.golang.org/protobuf/proto"
)

// DatabaseName 是收件箱独占的逻辑库名,本地与 K8s 同名(D-14 第 1 条;设计 §1.4)。
// config.Validate 断言 MySQL.DBName 等于它;schemamigrate 连接后再断言 SELECT DATABASE() 等于它 ——
// 两道防线都是为了不把收件箱的表建进别的服务的库(port-decisions D-14:「不借 data_service 的 mmorpg_global」)。
const DatabaseName = "mmorpg_player_inbox"

// Tables 返回本库全部表对应的 message。每个 message 都带 OptionTableName;
// 新增表 = 在 player_inbox_table.proto 加 message 并在这里追加一项。
func Tables() []proto.Message {
	return []proto.Message{
		&inboxpb.PlayerInboxStream{},
		&inboxpb.PlayerInboxItemRow{},
		&inboxpb.PlayerInboxAudit{},
	}
}
