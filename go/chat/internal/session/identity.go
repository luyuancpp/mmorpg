package session

import (
	"context"
	"time"

	base "proto/common/base"
	dspb "proto/data_service"
	"shared/loginsession"
	"shared/roleidentity"

	"google.golang.org/grpc"
)

// 解码后捕获原gate连接账号与原wire修订；失败不补当前目录，由业务入口按缺原件拒绝。
func OriginalIdentityInterceptor(reader loginsession.Reader) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req any, info *grpc.UnaryServerInfo, next grpc.UnaryHandler) (any, error) {
		detail, ok := GetSessionDetails(ctx)
		if !ok || detail.PlayerId == 0 || detail.OwnerRevision == 0 {
			return next(ctx, req)
		}
		lookup, cancel := context.WithTimeout(ctx, time.Second)
		account, err := loginsession.Account(lookup, reader, detail)
		cancel()
		if err != nil {
			return next(ctx, req)
		}
		bound, err := roleidentity.Capture(ctx, &dspb.AccountRoleOwnership{Account: account, OwnerRevision: detail.OwnerRevision, Role: &base.AccountSimplePlayer{PlayerId: detail.PlayerId}})
		if err != nil {
			return next(ctx, req)
		}
		return next(bound, req)
	}
}
