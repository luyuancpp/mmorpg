package clientplayerloginlogic

import (
	"context"
	"login/internal/svc"
	dbpb "proto/common/database"
	"shared/accountdirectory"
)

// 两次强读限制旧异步链，但不声称闭合最后重读到原生入场提交之间的竞态；
// 冻结完成仍必须由 SQL verifier 验证原生卸载与入场排空证据。
func (l *EnterGameLogic) recheckAccountRoleEntry(ctx context.Context, state enterGameSessionState) error {
	if l.svcCtx.AccountDirectory == nil {
		return accountdirectory.ErrUnavailable
	}
	owned, err := l.svcCtx.AccountDirectory.Lookup(ctx, state.playerID)
	if err != nil {
		return err
	}
	if owned.Account != state.account || state.ownerRevision == 0 || owned.OwnerRevision != state.ownerRevision || owned.TradeListingId != 0 {
		return accountdirectory.ErrConflict
	}
	return nil
}

func readAccountDirectory(ctx context.Context, sc *svc.ServiceContext, account string, create bool) (*dbpb.UserAccounts, error) {
	if sc.AccountDirectory == nil {
		return nil, accountdirectory.ErrUnavailable
	}
	roles, err := sc.AccountDirectory.Read(ctx, account, create)
	if err != nil {
		return nil, err
	}
	return &dbpb.UserAccounts{Account: account, SimplePlayers: roles}, nil
}
