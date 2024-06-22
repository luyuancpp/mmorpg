package loginservicelogic

import (
	"context"
	"github.com/golang/protobuf/proto"
	"login/client/dbservice/accountdbservice"
	"login/data"
	"strconv"

	"login/internal/svc"
	"login/pb/game"

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

func (l *LoginLogic) Login(in *game.LoginC2LRequest) (*game.LoginC2LResponse, error) {
	//todo 测试用例连接不登录马上断线，
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 登录的时候马上断开连接换了个gate应该可以登录成功
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	//todo gate异步同时登陆情况,老gate晚于新gate登录到controller会不会导致登录不成功了?这时候怎么处理

	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	_, ok := data.SessionList.Get(sessionId)
	resp := &game.LoginC2LResponse{ClientMsgBody: &game.LoginResponse{}}
	resp.SessionInfo = in.SessionInfo
	if ok {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1005}
		return resp, nil
	}
	data.SessionList.Set(sessionId, &data.Player{})

	rdKey := "account" + in.ClientMsgBody.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, rdKey)
	if len(cmd.Val()) <= 0 {
		as := accountdbservice.NewAccountDBService(*l.svcCtx.DBClient)
		_, err := as.Load2Redis(l.ctx, &game.LoadAccountRequest{Account: in.ClientMsgBody.Account})
		if err != nil {
			resp.ClientMsgBody.Error = &game.Tip{Id: 1005}
			return resp, err
		}
	}

	valueBytes, err := cmd.Bytes()
	if err != nil {
		logx.Error(err)
		return nil, err
	}

	accountDatabase := &game.AccountDatabase{}
	err = proto.Unmarshal(valueBytes, accountDatabase)
	if err != nil {
		logx.Error(err)
		return nil, err
	}
	if nil != accountDatabase.SimplePlayers {
		for _, v := range accountDatabase.SimplePlayers.Players {
			cPlayer := &game.CAccountSimplePlayer{Player: v}
			resp.ClientMsgBody.Players = append(resp.ClientMsgBody.Players, cPlayer)
		}
	}

	return resp, err
}
