package logic

import (
	"context"
	"database/sql"
	"sa-auth/model"
	"time"

	"sa-auth/internal/svc"
	"sa-auth/internal/token"
	"sa-auth/internal/types"
)

type OAuthLoginLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewOAuthLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *OAuthLoginLogic {
	return &OAuthLoginLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *OAuthLoginLogic) OAuthLogin(req *types.OAuthLoginReq) (*types.OAuthLoginResp, error) {
	r, err := l.svcCtx.UserOauthModel.FindOneByProviderProviderId(
		l.ctx, req.Provider, req.ProviderId,
	)

	var uid uint64

	if err != nil {
		// 新用户
		newUid, err := l.svcCtx.UserModel.Insert(l.ctx, &model.User{
			Id:         1,
			CreateTime: time.Now().Unix(),
			LastLogin:  sql.NullInt64{Int64: time.Now().Unix(), Valid: true},
			IsGuest:    1,
		})
		if err != nil {
			return nil, err
		}
		uid, err := newUid.LastInsertId()
		if err != nil {
			return nil, err
		}

		l.svcCtx.UserOauthModel.Insert(l.ctx,
			&model.UserOauth{
				Id:         0,
				UserId:     uint64(uid),
				Provider:   req.Provider,
				ProviderId: req.ProviderId,
			})
	} else {
		uid = uint64(r.UserId)
	}

	tok, err := token.Login(uid, req.DeviceId)
	if err != nil {
		return nil, err
	}

	return &types.OAuthLoginResp{
		Uid:   uid,
		Token: tok,
	}, nil
}
