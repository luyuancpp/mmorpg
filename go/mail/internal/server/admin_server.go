package server

import (
	"context"
	"crypto/subtle"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"mail/internal/config"
	"mail/internal/logic"
	"mail/internal/metrics"

	mailpb "proto/mail"
)

// AdminServer 实现 mailpb.MailAdminServer(内部服务,不标客户端协议,设计 §2.2 的四重隔离之一)。
//
// 鉴权(设计 §2.3):metadata x-mail-admin-token 必须等于 mail 进程环境变量 MMORPG_MAIL_ADMIN_TOKEN
// (去首尾空白后 ≥ 32 字节,config.ResolveAdminToken)。环境变量没配 = 整个 service 停用,两个方法一律 PermissionDenied;
// 没有 dev 放行。令牌值不进日志、指标、错误文本。
//
// "无会话 = 内部调用"不是鉴权:集群 gRPC 是 Insecure,任意进程可连。会话拦截器只负责把客户端来源挡在外面,
// "谁是合法运维"只由这里的令牌判。
type AdminServer struct {
	mailpb.UnimplementedMailAdminServer
	logic *logic.AdminLogic
	token string
}

// NewAdminServer 构造。token 取自 deps.AdminToken(空 = 停用)。
func NewAdminServer(deps *logic.Deps) mailpb.MailAdminServer {
	return &AdminServer{logic: logic.NewAdminLogic(deps), token: deps.AdminToken}
}

// SendPersonalMail 给一个玩家发一封定向邮件。
func (s *AdminServer) SendPersonalMail(ctx context.Context, in *mailpb.SendPersonalMailRequest) (*mailpb.SendPersonalMailResponse, error) {
	if err := s.authorize(ctx, "SendPersonalMail"); err != nil {
		metrics.ObserveAdminSend(metrics.MethodSendPersonal, logic.SenderKindLabel(in.GetSenderKind()), metrics.OutcomeDenied)
		return nil, err
	}
	return s.logic.SendPersonalMail(ctx, in)
}

// SendSystemMail 发一封全服邮件。
func (s *AdminServer) SendSystemMail(ctx context.Context, in *mailpb.SendSystemMailRequest) (*mailpb.SendSystemMailResponse, error) {
	if err := s.authorize(ctx, "SendSystemMail"); err != nil {
		metrics.ObserveAdminSend(metrics.MethodSendSystem, logic.SenderKindLabel(in.GetSenderKind()), metrics.OutcomeDenied)
		return nil, err
	}
	return s.logic.SendSystemMail(ctx, in)
}

// authorize 校验令牌,返回的 error 已是 gRPC status。比较用 subtle.ConstantTimeCompare:朴素的 == 会随比较位置提前返回,
// 给逐字节爆破留下时间侧信道(与 data_service 的 authorizeAdmin 同一做法)。
func (s *AdminServer) authorize(ctx context.Context, method string) error {
	if s.token == "" {
		logx.WithContext(ctx).Errorf("[mail] MailAdmin.%s refused: MailAdmin is disabled (environment variable not set or shorter than the minimum) caller=%s",
			method, logic.CallerIdentity(ctx))
		return status.Errorf(codes.PermissionDenied, "MailAdmin.%s is disabled on this mail instance", method)
	}
	var got string
	if md, ok := metadata.FromIncomingContext(ctx); ok {
		if vals := md.Get(config.AdminTokenMetadataKey); len(vals) > 0 {
			got = vals[0]
		}
	}
	if subtle.ConstantTimeCompare([]byte(got), []byte(s.token)) != 1 {
		// 只记"有没有带",绝不记令牌本身 —— 日志会进 Loki。
		logx.WithContext(ctx).Errorf("[mail] MailAdmin.%s refused: bad or missing %s (present=%t) caller=%s",
			method, config.AdminTokenMetadataKey, got != "", logic.CallerIdentity(ctx))
		return status.Errorf(codes.PermissionDenied, "MailAdmin.%s requires a valid %s", method, config.AdminTokenMetadataKey)
	}
	return nil
}
