package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/redis/go-redis/v9"
	"login/client/accountdbservice"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/svc"
	"login/pb/game"
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
	//todo 测试用例连接不登录马上断线，
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 登录的时候马上断开连接换了个gate应该可以登录成功
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	//todo gate异步同时登陆情况,老gate晚于新gate登录到controller会不会导致登录不成功了?这时候怎么处理
	resp := &game.LoginResponse{}

	// 1. 分布式锁，重试机制
	locker := locker.NewLoginLocker(l.svcCtx.Redis, 10*time.Second) // 超时改为10秒
	retryCount := 3
	lockAcquired := false
	var lockErr error
	for i := 0; i < retryCount; i++ {
		var ok bool
		ok, lockErr = locker.Acquire(l.ctx, in.Account)
		if lockErr == nil && ok {
			lockAcquired = true
			break
		}
		time.Sleep(200 * time.Millisecond)
	}
	if !lockAcquired {
		logx.Errorf("Login lock acquire failed after retries for account=%s, err=%v", in.Account, lockErr)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer locker.Release(l.ctx, in.Account)

	// 2. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 3. FSM 加载 + 执行 + 保存，出错立即返回
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.Redis, f, in.Account, ""); err != nil {
		logx.Errorf("FSM state load failed for account=%s: %v", in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}
	if err := f.Event(l.ctx, "process_login"); err != nil {
		logx.Errorf("FSM transition error for account=%s: %v", in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFSMEventFailed)}
		return resp, nil
	}
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.Redis, f, in.Account, ""); err != nil {
		logx.Errorf("FSM save error for account=%s: %v", in.Account, err)
		// 不阻断，但记录
	}

	// 4. 限制设备数量
	const MaxDevicesPerAccount = 3

	// 添加 SessionId 到设备集合
	sessionKey := fmt.Sprintf("login_account_sessions:%s", in.Account)
	if err := l.svcCtx.Redis.SAdd(l.ctx, sessionKey, sessionDetails.SessionId).Err(); err != nil {
		logx.Errorf("Redis SAdd error: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}
	_ = l.svcCtx.Redis.Expire(l.ctx, sessionKey, 30*time.Minute)

	// 限制最多 N 个设备
	count, err := l.svcCtx.Redis.SCard(l.ctx, sessionKey).Result()
	if err != nil {
		logx.Errorf("Redis SCard error: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	if count > MaxDevicesPerAccount {
		logx.Infof("Account %s exceeds device limit: %d > %d", in.Account, count, MaxDevicesPerAccount)

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
	if err := loginsessionstore.SaveLoginSession(l.ctx, l.svcCtx.Redis, sessionInfo); err != nil {
		logx.Errorf("Failed to save login session for account=%s: %v", in.Account, err)
		// 不终止流程
	}

	// 6. 加载账户数据（改进 Redis 获取判断方式）
	rdKey := "account" + in.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, rdKey)
	valueBytes, err := cmd.Bytes()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			logx.Infof("Account data not found in Redis for %s, loading from DB...", in.Account)
			service := accountdbservice.NewAccountDBService(*l.svcCtx.DbClient)
			_, loadErr := service.Load2Redis(l.ctx, &game.LoadAccountRequest{Account: in.Account})
			if loadErr != nil {
				logx.Errorf("Load account data from DB failed for %s: %v", in.Account, loadErr)
				resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountDataLoadFailed)}
				return resp, loadErr
			}
			cmd = l.svcCtx.Redis.Get(l.ctx, rdKey)
			valueBytes, err = cmd.Bytes()
			if err != nil {
				logx.Errorf("Redis re-get account data failed after DB load: %v", err)
				return nil, err
			}
		} else {
			logx.Errorf("Redis Get account error for %s: %v", in.Account, err)
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
