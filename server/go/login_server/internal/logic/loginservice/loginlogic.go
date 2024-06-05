package loginservicelogic

import (
	"context"
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

func (l *LoginLogic) Login(in *game.LoginRequest) (*game.LoginResponse, error) {
	// todo: add your logic here and delete this line

	//todo 测试用例连接不登录马上断线，
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 登录的时候马上断开连接换了个gate应该可以登录成功
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	//todo gate异步同时登陆情况,老gate晚于新gate登录到controller会不会导致登录不成功了?这时候怎么处理

	id := uint64(1)
	key := strconv.FormatUint(id, 10)

	_, ok := data.SessionList.Get(key)
	if ok {
		return &game.LoginResponse{Error: &game.Tips{Id: 1}}, nil
	}

	data.SessionList.Set("", &data.Player{})
	return &game.LoginResponse{}, nil
}
