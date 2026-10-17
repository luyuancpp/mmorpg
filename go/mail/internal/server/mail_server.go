// Package server 是 mail 的 gRPC 入口薄包装(照 friend / trade 的 internal/server):每个 RPC 委托给 logic,不写业务。
// 唯一的例外是 MailAdmin 的令牌校验(admin_server.go):它只对这一个 service 有意义,不进公共拦截器(设计 §5.4)。
package server

import (
	"context"

	"mail/internal/logic"

	mailpb "proto/mail"
)

// MailServer 实现 mailpb.ClientPlayerMailServer(标了 OptionIsClientProtocolService,只接 gate → 路由服转来的客户端请求)。
//
// 内嵌 UnimplementedClientPlayerMailServer:① 满足生成代码的 mustEmbed 约束;② 让 S2C 的 NotifyMailEvent 回 Unimplemented。
// 后者**不是**安全保证 —— 客户端伪造调用在会话层就被白名单挡住(internal/session,session_test.go 守住),这里是第二道防线。
// 东西向 / 运维方法不许加到这个 service 上:加进来就等于对客户端开放了一个消息号。
type MailServer struct {
	mailpb.UnimplementedClientPlayerMailServer
	logic *logic.MailLogic
}

// NewMailServer 返回接口类型:调用方(main)只需要能注册进 gRPC。
func NewMailServer(deps *logic.Deps) mailpb.ClientPlayerMailServer {
	return &MailServer{logic: logic.NewMailLogic(deps)}
}

// ListMails 合并定向邮件一页与系统邮件有效集,按 mail_id 降序。
func (s *MailServer) ListMails(ctx context.Context, in *mailpb.ListMailsRequest) (*mailpb.ListMailsResponse, error) {
	return s.logic.ListMails(ctx, in)
}

// ReadMail 读一封信并标已读。
func (s *MailServer) ReadMail(ctx context.Context, in *mailpb.ReadMailRequest) (*mailpb.ReadMailResponse, error) {
	return s.logic.ReadMail(ctx, in)
}

// DeleteMails 删一批信;带未领有效附件的不删,回在 protected_mail_ids。
func (s *MailServer) DeleteMails(ctx context.Context, in *mailpb.DeleteMailsRequest) (*mailpb.DeleteMailsResponse, error) {
	return s.logic.DeleteMails(ctx, in)
}

// DeleteReadMails 删掉全部已读且允许删的信。
func (s *MailServer) DeleteReadMails(ctx context.Context, in *mailpb.DeleteReadMailsRequest) (*mailpb.DeleteReadMailsResponse, error) {
	return s.logic.DeleteReadMails(ctx, in)
}

// ClaimMailAttachments 在 M1 恒回 MailClaimNotOpen(形状非法回 kInvalidParameter),不触库。
func (s *MailServer) ClaimMailAttachments(ctx context.Context, in *mailpb.ClaimMailAttachmentsRequest) (*mailpb.ClaimMailAttachmentsResponse, error) {
	return s.logic.ClaimMailAttachments(ctx, in)
}
