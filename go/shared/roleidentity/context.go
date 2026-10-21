// Package roleidentity 将原登录会话捕获的角色归属带到业务SQL提交点。
package roleidentity

import (
	"context"
	"errors"

	"google.golang.org/protobuf/proto"
	dspb "proto/data_service"
	"shared/accountdirectory"
)

type key struct{}

// Capture 保存原件副本；禁止在事务开始后用当前owner重新制造调用者身份。
func Capture(ctx context.Context, owned *dspb.AccountRoleOwnership) (context.Context, error) {
	if owned == nil || owned.Role.GetPlayerId() == 0 || !accountdirectory.ValidAccount(owned.Account) || owned.OwnerRevision == 0 {
		return nil, errors.New("原会话角色归属不完整")
	}
	return context.WithValue(ctx, key{}, proto.Clone(owned).(*dspb.AccountRoleOwnership)), nil
}
func Original(ctx context.Context) (*dspb.AccountRoleOwnership, bool) {
	owned, ok := ctx.Value(key{}).(*dspb.AccountRoleOwnership)
	if !ok {
		return nil, false
	}
	return proto.Clone(owned).(*dspb.AccountRoleOwnership), true
}
