package session

import (
	"context"
	"time"

	base "proto/common/base"
	dspb "proto/data_service"
	"shared/loginsession"
	"shared/roleidentity"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

func Details(ctx context.Context) *base.SessionDetails {
	detail, _ := ctx.Value(contextKey{}).(*base.SessionDetails)
	if detail == nil {
		return nil
	}
	return proto.Clone(detail).(*base.SessionDetails)
}

// 原账号只读显式全局 login 实例；修订只读原 gate 会话字段，不从当前 SQL 补造。
func OriginalIdentityInterceptor(login loginsession.Reader, budget time.Duration, check func(context.Context) error) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, next grpc.UnaryHandler) (any, error) {
		detail := Details(ctx)
		if detail == nil {
			return next(ctx, req)
		}
		if detail.GetOwnerRevision() == 0 || check == nil || budget <= 0 {
			return nil, status.Error(codes.Unauthenticated, "好友原登录修订或验证依赖未就绪")
		}
		ctx, cancel := context.WithTimeout(ctx, budget)
		defer cancel()
		account, err := loginsession.Account(ctx, login, detail)
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "好友原登录态不可用")
		}
		ctx, err = roleidentity.Capture(ctx, &dspb.AccountRoleOwnership{Account: account, OwnerRevision: detail.OwnerRevision,
			Role: &base.AccountSimplePlayer{PlayerId: detail.PlayerId}})
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "好友原登录身份非法")
		}
		if err := check(ctx); err != nil {
			return nil, status.Error(codes.PermissionDenied, "好友角色归属已变化或被冻结")
		}
		return next(ctx, req)
	}
}

// 保留原会话/方法准入这一层位置；原件验证在解码后、业务之前，不改变热关停顺序。
func ClientIdentityInterceptor(clientMethods map[string]struct{}, original grpc.UnaryServerInterceptor) grpc.UnaryServerInterceptor {
	metadataGuard := UnaryServerInterceptor(clientMethods)
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, next grpc.UnaryHandler) (any, error) {
		return metadataGuard(ctx, req, info, func(bound context.Context, body any) (any, error) {
			if original == nil {
				return nil, status.Error(codes.Unauthenticated, "好友原身份验证未装配")
			}
			return original(bound, body, info, next)
		})
	}
}
