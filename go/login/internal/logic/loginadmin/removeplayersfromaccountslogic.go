package loginadminlogic

import (
	"context"

	"login/internal/svc"
	"proto/login"

	"github.com/zeromicro/go-zero/core/logx"
)

type RemovePlayersFromAccountsLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewRemovePlayersFromAccountsLogic(ctx context.Context, svcCtx *svc.ServiceContext) *RemovePlayersFromAccountsLogic {
	return &RemovePlayersFromAccountsLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Remove orphan characters from their accounts' SimplePlayers lists.
func (l *RemovePlayersFromAccountsLogic) RemovePlayersFromAccounts(in *login.RemovePlayersFromAccountsRequest) (*login.RemovePlayersFromAccountsResponse, error) {
	// todo: add your logic here and delete this line

	return &login.RemovePlayersFromAccountsResponse{}, nil
}
