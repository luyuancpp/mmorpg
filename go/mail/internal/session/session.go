// Package session 解码 gate(经 client_rpc_router 原样透传)附带的会话元数据
// x-session-detail-bin,并决定哪些 RPC 允许客户端来源调用(设计 docs/design/mail-system.md §5.4)。
//
// 口径与 go/friend/internal/session 相同(port-decisions D-9),三条规则:
//
//  1. 没有会话元数据 = 内部调用(运维 CLI / robot 直连 MailAdmin),放行。客户端无法靠"不带会话"绕过本层:
//     路由服对缺会话的客户端消息直接拒绝,而 mail 只承诺路由服模式、没有 gate 直连白名单(D-12)。
//     **放行不等于鉴权**:MailAdmin 的"谁是合法运维"由令牌拦截器判,本层只负责把客户端来源挡在外面。
//  2. 带了会话元数据却解不开 = 拒绝(Unauthenticated)。坏头只可能是篡改或版本错配,绝不能退化成"当内部调用"。
//  3. 客户端来源只放行 ClientMethods 白名单;其余一律 PermissionDenied。
//
// # 白名单里刻意不放的方法
//
//   - NotifyMailEvent:S2C 推送方法。它必须写在名字含 ClientPlayer 的服务里,生成器才给 Unity / robot 出下行
//     handler,代价是它也出现在服务端 ServiceDesc 上、成了一个"客户端可拨"的方法名。服务端不提供它的 C2S 语义,
//     "今天返回 Unimplemented"不是安全保证,所以在这里按方法名挡住。
//   - MailAdmin 的两个发件方法:客户端来源调用 = 冒充运维发信,回 PermissionDenied(设计 §2.2 的四重隔离之一)。
//
// session_test.go 遍历两个 ServiceDesc 守住"新增 RPC 必须表态"。
package session

import (
	"context"
	"encoding/base64"
	"fmt"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	base "proto/common/base"
	mailpb "proto/mail"
)

// MetadataKey 是 gate 写入、路由服透传的会话元数据键,值 = base64(proto SessionDetails)。
const MetadataKey = "x-session-detail-bin"

type contextKey struct{}

// ClientMethods 是允许客户端来源调用的方法全集:ClientPlayerMail 的 5 个 C2S 方法。
// 白名单而不是黑名单:以后新增的 RPC 不写进来就不会意外对客户端开放。
var ClientMethods = map[string]struct{}{
	mailpb.ClientPlayerMail_ListMails_FullMethodName:            {},
	mailpb.ClientPlayerMail_ReadMail_FullMethodName:             {},
	mailpb.ClientPlayerMail_DeleteMails_FullMethodName:          {},
	mailpb.ClientPlayerMail_DeleteReadMails_FullMethodName:      {},
	mailpb.ClientPlayerMail_ClaimMailAttachments_FullMethodName: {},
}

// WithDetails 把已校验的会话放进 ctx(拦截器与单测共用)。
func WithDetails(ctx context.Context, detail *base.SessionDetails) context.Context {
	return context.WithValue(ctx, contextKey{}, detail)
}

// ClientPlayerID 返回客户端来源请求的权威 player_id(D-9:"我"只从会话取;mail 的请求体里根本没有 player_id)。
//
// ok=false 表示这次调用没带会话。逻辑层据此回 constants.ErrInvalidParameter 的 in-band 响应
// (刻意不用 fault=1 的 kPlayerNotFoundInSession,理由见 constants.ErrInvalidParameter)。
func ClientPlayerID(ctx context.Context) (playerID uint64, ok bool) {
	detail, _ := ctx.Value(contextKey{}).(*base.SessionDetails)
	if detail == nil || detail.GetPlayerId() == 0 {
		return 0, false
	}
	return detail.GetPlayerId(), true
}

// UnaryServerInterceptor 校验会话元数据并按 clientMethods 做方法级准入。
// 在一元链里放在 killswitch 之后、serverbase 之前:被关停的方法不必解码会话,被本层拒绝的调用也不会污染业务耗时与
// in-band 码指标。MailAdmin 的令牌不在链上验,在 internal/server/admin_server.go(只对那一个 service 有意义,设计 §5.4)。
func UnaryServerInterceptor(clientMethods map[string]struct{}) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (any, error) {
		md, _ := metadata.FromIncomingContext(ctx)
		values := md.Get(MetadataKey)
		if len(values) == 0 {
			return handler(ctx, req)
		}

		detail, err := decode(values[0])
		if err != nil {
			logx.WithContext(ctx).Errorf("[mail] Rejecting %s: cannot decode session metadata: %v", info.FullMethod, err)
			return nil, status.Error(codes.Unauthenticated, "invalid session metadata")
		}
		if _, allowed := clientMethods[info.FullMethod]; !allowed {
			logx.WithContext(ctx).Errorf("[mail] Rejecting client call to non-client method %s (session_id=%d)",
				info.FullMethod, detail.GetSessionId())
			return nil, status.Errorf(codes.PermissionDenied, "%s is not callable by clients", info.FullMethod)
		}
		return handler(WithDetails(ctx, detail), req)
	}
}

// decode 解出会话并做最低限度的完整性检查。player_id == 0 也算坏头:后续逻辑拿它当权威身份。
func decode(value string) (*base.SessionDetails, error) {
	raw, err := base64.StdEncoding.DecodeString(value)
	if err != nil {
		return nil, fmt.Errorf("base64: %w", err)
	}
	detail := &base.SessionDetails{}
	if err := proto.Unmarshal(raw, detail); err != nil {
		return nil, fmt.Errorf("unmarshal SessionDetails: %w", err)
	}
	if detail.GetPlayerId() == 0 {
		return nil, fmt.Errorf("SessionDetails without player_id (session_id=%d)", detail.GetSessionId())
	}
	return detail, nil
}
