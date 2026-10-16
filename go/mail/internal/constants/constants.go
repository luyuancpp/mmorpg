// Package constants 是 mail 写进 TipInfoMessage.id 的 tip 码表(docs/design/mail-system.md §5.6)。
//
// mail 引用**两条**码轴前缀(constants_test.go 的 TestNoHandWrittenTipCodes 机械守住):
//   - MailError_:Tip.xlsx 的 //mail_error base=17000 段,邮件域专用业务码;
//   - CommonError_:参数非法 / 存储故障这类跨域语义,与 friend、trade、路由服同一口径。
//
// 段与码都由导表器发号(AGENTS §7 第 5 条),这里只引用生成的枚举,不写任何数字。
// 加一个新码 = 往 Tip.xlsx 的 //mail_error 组尾加一行(tools/scripts/mail_xlsx_patch.py 的写法)→ 重跑导表器 →
// 在下面加一行引用。
package constants

import (
	"shared/generated/pb/table"
	"shared/serverbase"
)

// 邮件段的 8 个码。**全部是业务拒绝,没有一个是故障**(Tip.xlsx 的 fault 列全空):发件方被拒是规则生效,
// 玩家读不到别人的信是权限,都不是"我方出错"。存储 / 号段故障复用 common 段的 ErrStorage。
const (
	// ErrClaimNotOpen:M1 的 ClaimMailAttachments 一律回它(设计 §4.3)。M2 解禁领取后**不删**(已上线的码不回收),
	// 只是不再返回。
	ErrClaimNotOpen = uint32(table.MailError_kMailClaimNotOpen)

	// ErrNotFound:读 / 删 / 领一封不存在、不是自己的、已被自己删掉、或系统邮件已不可见的信。
	// "不是自己的"与"不存在"刻意同码:分开回就是让人能探测别人的邮件 id 是否存在(§11.3 敏感信息最少暴露)。
	ErrNotFound = uint32(table.MailError_kMailNotFound)

	// ErrExpired:信存在、是自己的,但已过 expire_ms。与 ErrNotFound 分开,客户端文案才能说"已过期"而不是"不存在"。
	ErrExpired = uint32(table.MailError_kMailExpired)

	// ErrRecipientBoxFull:SendPersonalMail 时收件人的有效定向邮件已达 Mail.MaxPersonalMailsPerPlayer(拒收,§3.4)。
	// 回给发件方(运维 / 机器),由发件方决定重试或换通道;玩家永远看不到这个码。
	ErrRecipientBoxFull = uint32(table.MailError_kMailRecipientBoxFull)

	// ErrAttachmentNotOpen:Mode 不是 dev / test 时请求带了附件(M-9:M1 生产禁发附件)。在取号之前拒,不消耗号段。
	ErrAttachmentNotOpen = uint32(table.MailError_kMailAttachmentNotOpen)

	// ErrAttachmentInvalid:附件条目违反容量 / 去重 / 取值规则(§3.5:货币 ≤4 条且类型不重复且在白名单内、
	// 物品 ≤16 条且 config_id 不重复、count ≥ 1、1 ≤ amount ≤ INT64_MAX)。
	ErrAttachmentInvalid = uint32(table.MailError_kMailAttachmentInvalid)

	// ErrRequestConflict:同一个幂等键 (sender_kind, request_id[, owner]) 已经发过,但这次的请求摘要不同 ——
	// 调用方复用了键却改了内容,是调用方的 bug(§2.4)。同键同摘要是重放,回首次结果,不走这里。
	ErrRequestConflict = uint32(table.MailError_kMailRequestConflict)

	// ErrSystemMailLimit:当前有效系统邮件数已达 Mail.MaxActiveSystemMails(软上限,§3.1)。
	// 读时合并要求有效系统邮件有界,这个码就是那道界。
	ErrSystemMailLimit = uint32(table.MailError_kMailSystemMailLimit)
)

// mail 复用的 common 段码(理由同 friend:跨域语义不在每个服务各发一份)。
const (
	// ErrInvalidParameter:取不到会话身份、请求形状非法(id 列表超长 / 为空、文本超长、request_id = 0、
	// expire_ms 不在窗口内、ROBOT 发件方出现在 Mode=pro 等)。
	//
	// 身份缺失刻意**不用** kPlayerNotFoundInSession(fault=1):请求根本没带会话属于客户端 / 链路问题,
	// 用故障码会把它刷成告警(与 friend 同一理由)。
	ErrInvalidParameter = uint32(table.CommonError_kInvalidParameter)

	// ErrStorage:MySQL 真故障,或号段取不到(data_service 不可达)—— 后者确实是依赖故障(§4.4)。
	// **本包唯一的故障码**(Tip.xlsx fault 列为 1,serverbase 记成 rpc_inband_fault 并配告警),
	// 所以绝不能拿它当"查不到"或"功能未实现"的占位。
	ErrStorage = uint32(table.CommonError_kServiceUnavailable)
)

// TipClassifier 返回本服务的 in-band 业务码定性函数,供 serverbase.UnaryInterceptor 使用。
// 定性完全交给全局表(Tip.xlsx 的 fault 列 → tip.Faults):邮件段 8 个码都不是故障,唯一的故障是 ErrStorage。
// **不要**在这里包本地 map —— 那会把"码的属性"与"码的定义"分家(AGENTS §7 第 5 条)。
func TipClassifier() serverbase.Classifier {
	return serverbase.TipVerdict
}
