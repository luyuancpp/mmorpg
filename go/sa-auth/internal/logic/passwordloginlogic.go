package logic

import (
	"context"
	"errors"
	"github.com/zeromicro/go-zero/core/logx"
	"time"

	"sa-auth/internal/svc"
	"sa-auth/internal/token"
	"sa-auth/internal/types"

	"golang.org/x/crypto/bcrypt"
)

type PasswordLoginLogic struct {
	logx.Logger
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewPasswordLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *PasswordLoginLogic {
	return &PasswordLoginLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
	}
}

func (l *PasswordLoginLogic) PasswordLogin(req *types.PasswordLoginReq) (*types.PasswordLoginResp, error) {
	// 查 user
	u, err := l.svcCtx.UserModel.FindOneByDisplayName(l.ctx, req.Account)
	if err != nil {
		return nil, errors.New("user not found")
	}

	// 查密码
	up, err := l.svcCtx.UserPasswordModel.FindOne(l.ctx, u.Id)
	if err != nil {
		return nil, errors.New("password not set")
	}

	if bcrypt.CompareHashAndPassword([]byte(up.Hash), []byte(req.Password)) != nil {
		return nil, errors.New("invalid password")
	}

	// update last_login
	_ = l.svcCtx.UserModel.UpdateLastLogin(l.ctx, u.Id, time.Now().Unix())

	// Sa-Token 登录
	tok, err := token.Login(uint64(u.Id), req.DeviceId)
	if err != nil {
		return nil, err
	}

	return &types.PasswordLoginResp{
		Uid:   uint64(u.Id),
		Token: tok,
	}, nil
}
