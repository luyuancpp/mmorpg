// Package data 是 mail 的存储层:独占库 mmorpg_mail 的表清单与手写参数化 SQL(docs/design/mail-system.md §3、§4)。
//
// 表结构的唯一事实源是 proto/mail/mail_table.proto(D-14 第 2 条):建表 / 加列只经 go/schemamigrate,
// 本包不写任何 DDL;行直接扫进生成的 message,不另写与表重复的 Go struct(AGENTS §3)。
// 列的增删一律先改 proto 再重生成 —— 反过来先改 SQL 会让 schemamigrate 的 Plan 与真实库永久对不上,启动即拒。
package data

import (
	mailpb "proto/mail"

	"google.golang.org/protobuf/proto"
)

// DatabaseName 是 mail 独占的逻辑库名,本地与 K8s 同名(D-14 第 1 条)。
// config.Validate 断言 MySQL.DBName 等于它;schemamigrate 连接后再断言 SELECT DATABASE() 等于它 ——
// 两道防线都是为了不把邮件的表建进别的服务的库。
const DatabaseName = "mmorpg_mail"

// Tables 返回本库全部表对应的 message。每个 message 都带 OptionTableName;
// 新增表 = 在 mail_table.proto 加 message 并在这里追加一项。
//
// 顺序与 mail_table.proto 的 message 顺序一致(定向 → 系统 → 系统邮件的玩家状态 → 守卫行):
// schemamigrate 按本切片顺序建表,顺序稳定 = Plan 输出可逐行 diff。
func Tables() []proto.Message {
	return []proto.Message{
		&mailpb.MailPersonalRecord{},
		&mailpb.MailSystemRecord{},
		&mailpb.MailSystemStateRecord{},
		&mailpb.MailBoxRecord{},
	}
}
