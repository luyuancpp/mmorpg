package loginadmin

import (
	"context"

	"login/internal/logic/admin"
	"login/internal/svc"
	login "proto/login"
)

type LoginAdminServer struct {
	svcCtx *svc.ServiceContext
	login.UnimplementedLoginAdminServer
}

func NewLoginAdminServer(svcCtx *svc.ServiceContext) *LoginAdminServer {
	return &LoginAdminServer{svcCtx: svcCtx}
}

func (s *LoginAdminServer) RemovePlayersFromAccounts(ctx context.Context, req *login.RemovePlayersFromAccountsRequest) (*login.RemovePlayersFromAccountsResponse, error) {
	resp, err := admin.RemovePlayersFromAccounts(ctx, s.svcCtx, &admin.RemovePlayersFromAccountsReq{
		PlayerIDs: req.PlayerIds,
	})
	if err != nil {
		return &login.RemovePlayersFromAccountsResponse{}, err
	}
	return &login.RemovePlayersFromAccountsResponse{
		RemovedCount:  resp.RemovedCount,
		NotFoundCount: resp.NotFoundCount,
		FailedCount:   resp.FailedCount,
	}, nil
}
