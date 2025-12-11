package logic

import (
	"context"
	"database/sql"
	"sa-auth/model"
	"time"

	"github.com/zeromicro/go-zero/core/syncx"
	"sa-auth/internal/svc"
	"sa-auth/internal/token"
	"sa-auth/internal/types"
)

type GuestLoginLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewGuestLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GuestLoginLogic {
	return &GuestLoginLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *GuestLoginLogic) GuestLogin(req *types.GuestLoginReq) (*types.GuestLoginResp, error) {

	result, err := l.svcCtx.UserModel.Insert(l.ctx, &model.User{
		Id:         1,
		CreateTime: time.Now().Unix(),
		LastLogin:  sql.NullInt64{Int64: time.Now().Unix(), Valid: true},
		IsGuest:    1,
	})
	if err != nil {
		return nil, err
	}

	newId, err := result.LastInsertId()
	if err != nil {
		return nil, err
	}

	tok, err := token.Login(uint64(newId), req.DeviceId)
	if err != nil {
		return nil, err
	}

	return &types.GuestLoginResp{
		Uid:   uint64(newId),
		Token: tok,
	}, nil
}
