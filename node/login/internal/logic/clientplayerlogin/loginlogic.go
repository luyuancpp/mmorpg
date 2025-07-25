package clientplayerloginlogic

import (
	"context"
	"errors"
	"github.com/golang/protobuf/proto"
	"github.com/redis/go-redis/v9"
	"login/client/accountdbservice"
	"login/data"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/svc"
	"login/pb/game"
	"strconv"
	"time"

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
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	//todo gate异步同时登陆情况,老gate晚于新gate登录到controller会不会导致登录不成功了?这时候怎么处理
	resp := &game.LoginResponse{}

	// 1. 分布式锁，重试机制
	locker := locker.NewAccountLocker(l.svcCtx.RedisClient, time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second)

	ok, err := locker.AcquireLogin(l.ctx, in.Account)
	if err != nil || !ok {
		logx.Errorf("Login lock acquire failed for account=%s, err=%v", in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer locker.ReleaseLogin(l.ctx, in.Account)

	// 2. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	sessionId := strconv.FormatUint(sessionDetails.SessionId, 10)
	logx.Infof("Start processing login for account=%s with sessionId=%s", in.Account, sessionId)

	// 3. FSM 加载 + 执行 + 保存，出错立即返回
	f := data.InitPlayerFSM()

	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionId, ""); err != nil {
		logx.Errorf("FSM state load failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}

	// 执行 FSM 事件
	logx.Infof("Processing FSM event for sessionId=%s, account=%s, event=process_login", sessionId, in.Account)
	if err := f.Event(l.ctx, data.EventProcessLogin); err != nil {
		logx.Errorf("FSM transition error for sessionId=%s, account=%s, event=process_login, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFSMEventFailed)}
		return resp, nil
	}

	// 保存 FSM 状态
	logx.Infof("Attempting to save FSM state for sessionId=%s", sessionId)
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionId, ""); err != nil {
		logx.Errorf("FSM save failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		// 不阻断，但记录错误
		//todo 让客户端重新登录
		return resp, nil
	}

	// 4. 限制设备数量

	// 添加 SessionId 到设备集合
	sessionKey := constants.GenerateSessionKey(in.Account)
	if err := l.svcCtx.RedisClient.SAdd(l.ctx, sessionKey, sessionDetails.SessionId).Err(); err != nil {
		logx.Errorf("RedisClient SAdd error: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}
	expire := time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute
	_ = l.svcCtx.RedisClient.Expire(l.ctx, sessionKey, expire)

	// 限制最多 N 个设备
	count, err := l.svcCtx.RedisClient.SCard(l.ctx, sessionKey).Result()
	if err != nil {
		logx.Errorf("RedisClient SCard error: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	if count > config.AppConfig.Account.MaxDevicesPerAccount {
		logx.Infof("Account %s exceeds device limit: %d > %d", in.Account, count, config.AppConfig.Account.MaxDevicesPerAccount)

		// 可选：删除旧的 session（基于 TTL 或先入先出策略）
		// 或通知客户端“已有设备登录，是否强制顶号”——这需要客户端支持。

		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kTooManyDevices)}
		return resp, nil
	}

	// 5. 构造并保存登录会话信息
	sessionInfo := &game.LoginSessionInfo{
		Account:   in.Account,
		RoleId:    0,
		SessionId: sessionDetails.SessionId,
		GateId:    0,
		DeviceId:  "",
		LoginTime: time.Now().Unix(),
		Fsm:       f.Current(),
	}
	if err := loginsessionstore.SaveLoginSession(l.ctx, l.svcCtx.RedisClient, sessionInfo, expire); err != nil {
		logx.Errorf("Failed to save login session for account=%s: %v", in.Account, err)
		// 不终止流程
	}

	// 6. 加载账户数据（改进 RedisClient 获取判断方式）
	rdKey := constants.GetAccountDataKey(in.Account)
	cmd := l.svcCtx.RedisClient.Get(l.ctx, rdKey)
	valueBytes, err := cmd.Bytes()
	if errors.Is(err, redis.Nil) {
		logx.Infof("Account data not found in RedisClient for %s, loading from DB...", in.Account)
		service := accountdbservice.NewAccountDBService(*l.svcCtx.DbClient)
		_, loadErr := service.Load2Redis(l.ctx, &game.LoadAccountRequest{Account: in.Account})
		if loadErr != nil {
			logx.Errorf("Load account data from DB failed for %s: %v", in.Account, loadErr)
			resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountDataLoadFailed)}
			return resp, loadErr
		}
		cmd = l.svcCtx.RedisClient.Get(l.ctx, rdKey)
		valueBytes, err = cmd.Bytes()
		if err != nil {
			logx.Errorf("RedisClient re-get account data failed after DB load: %v", err)
			return nil, err
		}
	}

	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal(valueBytes, userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed for %s: %v", in.Account, err)
		return nil, err
	}

	// 7. 返回角色列表
	if userAccount.SimplePlayers != nil {
		for _, v := range userAccount.SimplePlayers.Players {
			resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: v})
		}
	}

	return resp, nil
}
