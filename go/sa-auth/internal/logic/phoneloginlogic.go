package logic

import (
	"context"
	"errors"
	"time"

	"sa-auth/internal/svc"
	"sa-auth/internal/token"
	"sa-auth/internal/types"
)

type PhoneLoginLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewPhoneLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *PhoneLoginLogic {
	return &PhoneLoginLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *PhoneLoginLogic) PhoneLogin(req *types.PhoneLoginReq) (*types.PhoneLoginResp, error) {
	if req.Code != "1234" {
		return nil, errors.New("invalid sms code")
	}

	p, err := l.svcCtx.UserPhoneModel.FindOneByPhone(l.ctx, req.Phone)
	var uid uint64

	if err != nil {
		// 新用户
		uid64, err := l.svcCtx.UserModel.InsertGuest(l.ctx, time.Now().Unix())
		if err != nil {
			return nil, err
		}
		uid = uid64
		_ = l.svcCtx.UserPhoneModel.Insert(l.ctx, uid, req.Phone)
	} else {
		uid = uint64(p.UserId)
	}

	tok, err := token.Login(uid, req.DeviceId)
	if err != nil {
		return nil, err
	}

	return &types.PhoneLoginResp{
		Uid:   uid,
		Token: tok,
	}, nil
}
