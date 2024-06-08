package loginservicelogic

import (
	"context"
	"github.com/golang/protobuf/proto"
	"login_server/client/dbservice/accountdbservice"
	"login_server/data"
	"strconv"

	"login_server/internal/svc"
	"login_server/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type LoginLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *LoginLogic {
	return &LoginLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *LoginLogic) Login(in *game.LoginC2LRequest) (*game.LoginResponse, error) {
	//todo 测试用例连接不登录马上断线，
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 登录的时候马上断开连接换了个gate应该可以登录成功
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	//todo gate异步同时登陆情况,老gate晚于新gate登录到controller会不会导致登录不成功了?这时候怎么处理

	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	_, ok := data.SessionList.Get(sessionId)
	if ok {
		return &game.LoginResponse{Error: &game.Tips{Id: 1005}}, nil
	}
	data.SessionList.Set(sessionId, &data.Player{})

	resp := &game.LoginResponse{}

	rdKey := "account" + in.Account
	cmd := l.svcCtx.Rdb.Get(l.ctx, rdKey)
	if cmd == nil {
		as := accountdbservice.NewAccountDBService(*l.svcCtx.DBCli)
		_, err := as.Load2Redis(l.ctx, &game.LoadAccountRequest{Account: in.Account})
		if err != nil {
			return resp, err
		}
		cmd = l.svcCtx.Rdb.Get(l.ctx, rdKey)
		if cmd == nil {
			logx.Error("cannot find account:" + in.Account)
			return resp, nil
		}
	}

	err := proto.Unmarshal([]byte(cmd.Val()), resp)
	return resp, err
}
