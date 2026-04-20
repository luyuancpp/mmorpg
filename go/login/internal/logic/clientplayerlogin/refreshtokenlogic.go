package clientplayerloginlogic

import (
	"context"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_proto "proto/login"
	"shared/generated/pb/table"

	"github.com/zeromicro/go-zero/core/logx"
)

type RefreshTokenLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewRefreshTokenLogic(ctx context.Context, svcCtx *svc.ServiceContext) *RefreshTokenLogic {
	return &RefreshTokenLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *RefreshTokenLogic) RefreshToken(in *login_proto.RefreshTokenRequest) (*login_proto.RefreshTokenResponse, error) {
	resp := &login_proto.RefreshTokenResponse{}

	if in.RefreshToken == "" {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountNotFound)}
		return resp, nil
	}

	tokenPair, err := l.svcCtx.TokenManager.Refresh(l.ctx, in.RefreshToken)
	if err != nil {
		logx.Errorf("RefreshToken failed: %v", err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountNotFound)}
		return resp, nil
	}

	resp.AccessToken = tokenPair.AccessToken
	resp.RefreshToken = tokenPair.RefreshToken
	resp.AccessTokenExpire = tokenPair.AccessTokenExpire
	resp.RefreshTokenExpire = tokenPair.RefreshTokenExpire

	logx.Infof("RefreshToken success: new access_token expires=%d", tokenPair.AccessTokenExpire)
	return resp, nil
}
