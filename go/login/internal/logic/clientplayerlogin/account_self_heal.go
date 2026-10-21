package clientplayerloginlogic

import (
	"context"
	base "proto/common/base"
)

// 旧入口仅强读 SQL，绝不凭 Redis 反向键重建角色。归属缺失或变化即失败。
func (l *EnterGameLogic) healAccountRole(ctx context.Context, state enterGameSessionState, name string) *base.AccountSimplePlayer {
	if l.svcCtx.AccountDirectory == nil {
		return nil
	}
	owned, err := l.svcCtx.AccountDirectory.Lookup(ctx, state.playerID)
	if err != nil || owned.Account != state.account || state.ownerRevision == 0 || owned.OwnerRevision != state.ownerRevision || owned.TradeListingId != 0 {
		return nil
	}
	return owned.Role
}
