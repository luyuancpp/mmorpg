package server

import (
	"context"
	"crypto/subtle"
	"fmt"
	"strings"

	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/peer"
	"google.golang.org/grpc/status"
)

// 运维面鉴权(设计 §1.6、§6.1 第 1 步)。写法照 data_service 的 authorizeAdmin(dataserviceserver.go)。
//
// 做成拦截器而不是每个方法开头各调一次:PlayerInboxAdmin 的**全部**方法都要求口令,新增方法不可能忘记挂。
// 只拦 /player_inbox.PlayerInboxAdmin/ 前缀;scene 调的 PlayerInbox(Fetch / Ack)不经口令(D6:v1 不签名)。

// AdminTokenMetadataKey 是运维口令所在的 gRPC metadata 键。必须全小写:gRPC 在传输层把 key 规范成小写。
const AdminTokenMetadataKey = "x-admin-token"

// adminMethodPrefix 是运维服务全部方法的 FullMethod 前缀(由生成的服务描述派生,不手写服务名)。
var adminMethodPrefix = "/" + inboxpb.PlayerInboxAdmin_ServiceDesc.ServiceName + "/"

// AdminAuthInterceptor 返回运维面鉴权拦截器。token 为空 = 运维面整体停用(fail-closed):每个运维方法都回
// PermissionDenied,不是「免鉴权」。比较用常数时间,口令值绝不进日志。
func AdminAuthInterceptor(token string) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (any, error) {
		if !strings.HasPrefix(info.FullMethod, adminMethodPrefix) {
			return handler(ctx, req)
		}
		if err := authorizeAdmin(ctx, token, info.FullMethod); err != nil {
			return nil, err
		}
		return handler(ctx, req)
	}
}

// authorizeAdmin 校验运维凭据。返回的 error 已经是 gRPC status。
// subtle.ConstantTimeCompare:朴素的 == 会随比较位置提前返回,给逐字节爆破留下时间侧信道。
func authorizeAdmin(ctx context.Context, want, method string) error {
	if want == "" {
		logx.WithContext(ctx).Errorf("[player_inbox] %s refused: no admin token configured via PlayerInbox.AdminToken / AdminTokenEnv (admin surface disabled) %s",
			method, callerIdentity(ctx))
		return status.Errorf(codes.PermissionDenied, "%s is disabled because no admin token is configured (PlayerInbox.AdminToken / AdminTokenEnv)", method)
	}
	var got string
	if md, ok := metadata.FromIncomingContext(ctx); ok {
		if vals := md.Get(AdminTokenMetadataKey); len(vals) > 0 {
			got = vals[0]
		}
	}
	if subtle.ConstantTimeCompare([]byte(got), []byte(want)) != 1 {
		// 只记「有没有带」,绝不记口令本身 —— 日志会进 Loki,写进去就等于泄露(AGENTS §11.3)。
		logx.WithContext(ctx).Errorf("[player_inbox] %s refused: bad or missing %s (present=%t) %s",
			method, AdminTokenMetadataKey, got != "", callerIdentity(ctx))
		return status.Errorf(codes.PermissionDenied, "%s requires a valid %s", method, AdminTokenMetadataKey)
	}
	return nil
}

// PeerAddr 是 gRPC 对端地址,审计里唯一不可伪造的部分(设计 §1.6);取不到时为 "unknown"。
func PeerAddr(ctx context.Context) string {
	if p, ok := peer.FromContext(ctx); ok && p.Addr != nil {
		return p.Addr.String()
	}
	return "unknown"
}

// callerIdentity 拼一条「谁在调」的日志片段:对端地址 + 调用方自报的 user-agent。
// 地址是唯一不可伪造的部分,user-agent 只是给人看的线索,不能当鉴权依据。
func callerIdentity(ctx context.Context) string {
	agent := ""
	if md, ok := metadata.FromIncomingContext(ctx); ok {
		if v := md.Get("user-agent"); len(v) > 0 {
			agent = v[0]
		}
	}
	return fmt.Sprintf("peer=%s user-agent=%q", PeerAddr(ctx), agent)
}
