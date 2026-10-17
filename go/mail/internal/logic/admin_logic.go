package logic

// admin_logic.go —— 内部发件入口 MailAdmin 的两个方法(设计 §2.2–§2.5、§4.2)。
//
// 令牌校验不在这里:它在 server 层(internal/server/admin_server.go),回的是 gRPC PermissionDenied ——
// 走到本文件的调用一定已经带对了令牌。
//
// # 顺序是正确性的一部分(§2.4 / M-18)
//
//	① 纯形状校验(长度、request_id ≠ 0、收件人 ≠ 0、附件条目规则含货币白名单)
//	② Mode 闸门(M-9):Mode ∉ {dev, test} 时附件非空回 MailAttachmentNotOpen、sender_kind = ROBOT 回 kInvalidParameter
//	   —— 都在取号之前,被拒的请求不消耗号段
//	③ 交给存储层:**先按唯一键判重**(命中即比摘要回 duplicated / MailRequestConflict),
//	   未命中才做依赖"现在"的校验(expire_ms 窗口、邮箱现数 / 有效系统邮件数)
//
// 已经成功的请求在重放时,邮箱可能已满、expire_ms 可能已落到 now 之前;先做这些校验的话,重放拿到的是
// BoxFull / kInvalidParameter,调用方会把"已发出"误判成"没发出"而补发。
//
// # 结果语义
//
// 业务结果一律 in-band;存储 / 号段故障 = ErrStorage(唯一 fault)。调用方超时 / 断连 = 结果未知,
// 用同一个 (sender_kind, request_id) 重发即可;mail 对 MySQL 不做自动重试(§2.5)。
// 每次调用计 mail_admin_send_total{method,sender_kind,outcome}(denied 由 server 层计)。

import (
	"context"
	"errors"
	"fmt"
	"time"
	"unicode/utf8"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/peer"

	"mail/internal/config"
	"mail/internal/constants"
	"mail/internal/data"
	"mail/internal/metrics"

	base "proto/common/base"
	mailpb "proto/mail"
)

// 文本长度上限(Unicode 码点),mail_admin.proto 的字段注释是对外说法,这里是唯一的数值来源。
const (
	maxSenderNameRunes = 16
	maxTitleRunes      = 32
	maxBodyRunes       = 1000
)

const dayMs = uint64(24 * time.Hour / time.Millisecond)

// AdminLogic 实现 MailAdmin 的两个方法。
type AdminLogic struct {
	deps *Deps
}

func NewAdminLogic(deps *Deps) *AdminLogic {
	return &AdminLogic{deps: deps}
}

// SenderKindLabel 把 sender_kind 映射成指标 label(有限枚举;未知值一律 unknown,不拿数字当 label)。
// server 层记 denied 时也用它,所以导出。
func SenderKindLabel(kind mailpb.MailSenderKind) string {
	switch kind {
	case mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL:
		return metrics.SenderOpsTool
	case mailpb.MailSenderKind_MAIL_SENDER_KIND_ROBOT:
		return metrics.SenderRobot
	default:
		return metrics.SenderUnknown
	}
}

// CallerIdentity 拼一条"谁在调"的日志片段:对端地址是唯一不可伪造的部分,user-agent / x-operator 只是给人看的线索,
// 不能当鉴权依据。发件成功与鉴权失败的日志都带它 —— 系统邮件不可撤回,事后必须查得到是谁发的。
func CallerIdentity(ctx context.Context) string {
	addr := "unknown"
	if p, ok := peer.FromContext(ctx); ok && p.Addr != nil {
		addr = p.Addr.String()
	}
	agent, operator := "", ""
	if md, ok := metadata.FromIncomingContext(ctx); ok {
		if v := md.Get("user-agent"); len(v) > 0 {
			agent = v[0]
		}
		if v := md.Get("x-operator"); len(v) > 0 {
			operator = v[0]
		}
	}
	return fmt.Sprintf("peer=%s user-agent=%q operator=%q", addr, agent, operator)
}

// sendEnvelope 是两种发件共有的字段。
type sendEnvelope struct {
	senderKind  mailpb.MailSenderKind
	requestID   uint64
	senderName  string
	title       string
	body        string
	attachments *mailpb.MailAttachments
	expireMs    uint64
}

// validateEnvelope 做 ① 形状校验与 ② Mode 闸门,返回 normalize 过的附件(nil = 没有附件)。
// tip 非 nil 即拒绝;outcome 一律是 rejected。
func (l *AdminLogic) validateEnvelope(ctx context.Context, method string, env sendEnvelope) (*mailpb.MailAttachments, *base.TipInfoMessage) {
	switch env.senderKind {
	case mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL, mailpb.MailSenderKind_MAIL_SENDER_KIND_ROBOT:
	default:
		return nil, tipErr(constants.ErrInvalidParameter, "sender_kind must be OPS_TOOL or ROBOT")
	}
	if env.requestID == 0 {
		return nil, tipErr(constants.ErrInvalidParameter, "request_id required")
	}
	if tip := validateText("sender_name", env.senderName, maxSenderNameRunes, true); tip != nil {
		return nil, tip
	}
	if tip := validateText("title", env.title, maxTitleRunes, true); tip != nil {
		return nil, tip
	}
	if tip := validateText("body", env.body, maxBodyRunes, false); tip != nil {
		return nil, tip
	}
	if env.expireMs == 0 {
		return nil, tipErr(constants.ErrInvalidParameter, "expire_ms required")
	}
	attachments, err := normalizeAttachments(env.attachments, l.deps.Config.Mail.AllowsCurrency)
	if err != nil {
		logx.WithContext(ctx).Infof("[mail] %s rejected request_id=%d: %v", method, env.requestID, err)
		return nil, tipErr(constants.ErrAttachmentInvalid, "attachments invalid")
	}

	// ② Mode 闸门(M-9):M1 唯一的防资产损失闸门 —— 领取未开放,生产发出去的附件只会到期作废。
	if !config.IsRelaxedMode(l.deps.Config.Mode) {
		if attachments != nil {
			return nil, tipErr(constants.ErrAttachmentNotOpen, "attachments are only allowed in dev / test mode")
		}
		if env.senderKind == mailpb.MailSenderKind_MAIL_SENDER_KIND_ROBOT {
			return nil, tipErr(constants.ErrInvalidParameter, "sender_kind ROBOT is only accepted in dev / test mode")
		}
	}
	return attachments, nil
}

// validateText:按 Unicode 码点限长(proto3 string 在反序列化时已保证是合法 UTF-8);required 时拒空串。
func validateText(field, s string, maxRunes int, required bool) *base.TipInfoMessage {
	if required && s == "" {
		return tipErr(constants.ErrInvalidParameter, field+" required")
	}
	if utf8.RuneCountInString(s) > maxRunes {
		return tipErr(constants.ErrInvalidParameter, fmt.Sprintf("%s longer than %d characters", field, maxRunes))
	}
	return nil
}

// maxExpireMs 是发件时刻 + 最长有效期(天)。
func maxExpireMs(nowMs uint64, ttlDays uint32) uint64 {
	return nowMs + uint64(ttlDays)*dayMs
}

// ════════════════════════════════════════════════════════════════════════════
// SendPersonalMail
// ════════════════════════════════════════════════════════════════════════════

// SendPersonalMail 给一个玩家发一封定向邮件。首次发出(非重放)才在提交之后推送 NotifyMailEvent;推送失败不影响结果。
// 不校验收件人是否存在(mail 没有玩家名册,§4.2 末段)。
func (l *AdminLogic) SendPersonalMail(ctx context.Context, req *mailpb.SendPersonalMailRequest) (*mailpb.SendPersonalMailResponse, error) {
	const method = "SendPersonalMail"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	sender := SenderKindLabel(req.GetSenderKind())
	reply := func(outcome string, resp *mailpb.SendPersonalMailResponse) (*mailpb.SendPersonalMailResponse, error) {
		metrics.ObserveAdminSend(metrics.MethodSendPersonal, sender, outcome)
		return resp, nil
	}
	reject := func(tip *base.TipInfoMessage) (*mailpb.SendPersonalMailResponse, error) {
		return reply(metrics.OutcomeRejected, &mailpb.SendPersonalMailResponse{ErrorMessage: tip})
	}
	fault := func(err error) (*mailpb.SendPersonalMailResponse, error) {
		return reply(metrics.OutcomeFault, &mailpb.SendPersonalMailResponse{ErrorMessage: storageFailure(ctx, method, err)})
	}

	if req.GetRecipientPlayerId() == 0 {
		return reject(tipErr(constants.ErrInvalidParameter, "recipient_player_id required"))
	}
	env := sendEnvelope{
		senderKind: req.GetSenderKind(), requestID: req.GetRequestId(),
		senderName: req.GetSenderName(), title: req.GetTitle(), body: req.GetBody(),
		attachments: req.GetAttachments(), expireMs: req.GetExpireMs(),
	}
	attachments, tip := l.validateEnvelope(ctx, method, env)
	if tip != nil {
		return reject(tip)
	}
	if l.deps.NextID == nil {
		return fault(errors.New("mail_id allocator not wired"))
	}
	encoded, err := encodeAttachments(attachments)
	if err != nil {
		return fault(err)
	}
	now := l.deps.nowMs()
	result, err := l.deps.Repo.SendPersonal(ctx, data.PersonalSend{
		Record: &mailpb.MailPersonalRecord{
			OwnerPlayerId: req.GetRecipientPlayerId(),
			SenderKind:    uint32(env.senderKind),
			RequestId:     env.requestID,
			RequestDigest: requestDigest(sendDigestInput{
				kind: digestKindPersonal, senderKind: uint32(env.senderKind), requestID: env.requestID,
				recipientID: req.GetRecipientPlayerId(), senderName: env.senderName, title: env.title, body: env.body,
				attachments: attachments, expireMs: env.expireMs,
			}),
			SenderName:  env.senderName,
			Title:       env.title,
			Body:        env.body,
			Attachments: encoded,
			SentMs:      now,
			ExpireMs:    env.expireMs,
		},
		MaxExpireMs: maxExpireMs(now, l.deps.Config.Mail.MaxPersonalMailTtlDays),
		BoxLimit:    l.deps.Config.Mail.MaxPersonalMailsPerPlayer,
		NextID:      l.deps.NextID,
	})
	if err != nil {
		return fault(err)
	}
	switch result.Outcome {
	case data.SendCreated:
		logx.WithContext(ctx).Infof("[mail] %s sent mail_id=%d to=%d sender_kind=%s request_id=%d attachments=%t caller=%s",
			method, result.MailID, req.GetRecipientPlayerId(), sender, env.requestID, attachments != nil, CallerIdentity(ctx))
		// 提交之后、事务之外推送;推送失败只记日志与指标(push.go)。
		l.pushNewMail(ctx, req.GetRecipientPlayerId())
		return reply(metrics.OutcomeOK, &mailpb.SendPersonalMailResponse{MailId: result.MailID})
	case data.SendDuplicated:
		return reply(metrics.OutcomeDuplicated, &mailpb.SendPersonalMailResponse{MailId: result.MailID, Duplicated: true})
	case data.SendConflict:
		return reject(tipErr(constants.ErrRequestConflict, "request_id reused with different content"))
	case data.SendExpireOutOfRange:
		return reject(tipErr(constants.ErrInvalidParameter, fmt.Sprintf("expire_ms must be in (now, now + %d days]", l.deps.Config.Mail.MaxPersonalMailTtlDays)))
	case data.SendBoxFull:
		return reject(tipErr(constants.ErrRecipientBoxFull, "recipient mailbox full"))
	default:
		return fault(fmt.Errorf("unexpected send outcome %d", result.Outcome))
	}
}

// ════════════════════════════════════════════════════════════════════════════
// SendSystemMail
// ════════════════════════════════════════════════════════════════════════════

// SendSystemMail 发一封全服邮件:发出即对所有人可见,不推送,M1 不可撤回(§11 第 8 / 9 条)。
func (l *AdminLogic) SendSystemMail(ctx context.Context, req *mailpb.SendSystemMailRequest) (*mailpb.SendSystemMailResponse, error) {
	const method = "SendSystemMail"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	sender := SenderKindLabel(req.GetSenderKind())
	reply := func(outcome string, resp *mailpb.SendSystemMailResponse) (*mailpb.SendSystemMailResponse, error) {
		metrics.ObserveAdminSend(metrics.MethodSendSystem, sender, outcome)
		return resp, nil
	}
	reject := func(tip *base.TipInfoMessage) (*mailpb.SendSystemMailResponse, error) {
		return reply(metrics.OutcomeRejected, &mailpb.SendSystemMailResponse{ErrorMessage: tip})
	}
	fault := func(err error) (*mailpb.SendSystemMailResponse, error) {
		return reply(metrics.OutcomeFault, &mailpb.SendSystemMailResponse{ErrorMessage: storageFailure(ctx, method, err)})
	}

	env := sendEnvelope{
		senderKind: req.GetSenderKind(), requestID: req.GetRequestId(),
		senderName: req.GetSenderName(), title: req.GetTitle(), body: req.GetBody(),
		attachments: req.GetAttachments(), expireMs: req.GetExpireMs(),
	}
	attachments, tip := l.validateEnvelope(ctx, method, env)
	if tip != nil {
		return reject(tip)
	}
	if l.deps.NextID == nil {
		return fault(errors.New("sys_mail_id allocator not wired"))
	}
	encoded, err := encodeAttachments(attachments)
	if err != nil {
		return fault(err)
	}
	now := l.deps.nowMs()
	result, err := l.deps.Repo.SendSystem(ctx, data.SystemSend{
		Record: &mailpb.MailSystemRecord{
			SenderKind: uint32(env.senderKind),
			RequestId:  env.requestID,
			RequestDigest: requestDigest(sendDigestInput{
				kind: digestKindSystem, senderKind: uint32(env.senderKind), requestID: env.requestID,
				senderName: env.senderName, title: env.title, body: env.body,
				attachments: attachments, expireMs: env.expireMs,
			}),
			SenderName:  env.senderName,
			Title:       env.title,
			Body:        env.body,
			Attachments: encoded,
			SentMs:      now,
			ExpireMs:    env.expireMs,
		},
		MaxExpireMs: maxExpireMs(now, l.deps.Config.Mail.MaxSystemMailTtlDays),
		ActiveLimit: l.deps.Config.Mail.MaxActiveSystemMails,
		NextID:      l.deps.NextID,
	})
	if err != nil {
		return fault(err)
	}
	switch result.Outcome {
	case data.SendCreated:
		logx.WithContext(ctx).Infof("[mail] %s sent sys_mail_id=%d sender_kind=%s request_id=%d expire_ms=%d attachments=%t caller=%s",
			method, result.MailID, sender, env.requestID, env.expireMs, attachments != nil, CallerIdentity(ctx))
		return reply(metrics.OutcomeOK, &mailpb.SendSystemMailResponse{SysMailId: result.MailID})
	case data.SendDuplicated:
		return reply(metrics.OutcomeDuplicated, &mailpb.SendSystemMailResponse{SysMailId: result.MailID, Duplicated: true})
	case data.SendConflict:
		return reject(tipErr(constants.ErrRequestConflict, "request_id reused with different content"))
	case data.SendExpireOutOfRange:
		return reject(tipErr(constants.ErrInvalidParameter, fmt.Sprintf("expire_ms must be in (now, now + %d days]", l.deps.Config.Mail.MaxSystemMailTtlDays)))
	case data.SendSystemLimit:
		return reject(tipErr(constants.ErrSystemMailLimit, "active system mail limit reached"))
	default:
		return fault(fmt.Errorf("unexpected send outcome %d", result.Outcome))
	}
}
