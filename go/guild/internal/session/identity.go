package session

import (
	"context"

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

// 原账号只读部署显式的全局login实例；修订只读gate发出时的会话原件，不从当前归属补造。
func OriginalIdentityInterceptor(login loginsession.Reader, check func(context.Context) error) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, next grpc.UnaryHandler) (any, error) {
		detail := Details(ctx)
		if detail == nil {
			return next(ctx, req)
		}
		if detail.GetOwnerRevision() == 0 || check == nil {
			return nil, status.Error(codes.Unauthenticated, "原登录修订未就绪")
		}
		account, err := loginsession.Account(ctx, login, detail)
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "原登录态不可用")
		}
		ctx, err = roleidentity.Capture(ctx, &dspb.AccountRoleOwnership{Account: account, OwnerRevision: detail.OwnerRevision, Role: &base.AccountSimplePlayer{PlayerId: detail.PlayerId}})
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "原登录身份非法")
		}
		if err = check(ctx); err != nil {
			return nil, status.Error(codes.PermissionDenied, "角色归属已变化或被冻结")
		}
		return next(ctx, req)
	}
}
