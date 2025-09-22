package clientplayerloginlogic

import (
	"context"
	"errors"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"login/data"
	"login/generated/pb/game"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/svc"
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
	accountLocker := locker.NewAccountLocker(l.svcCtx.RedisClient, time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second)

	ok, err := accountLocker.AcquireLogin(l.ctx, in.Account)
	if err != nil || !ok {
		logx.Errorf("Login lock acquire failed for account=%s, err=%v", in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer accountLocker.ReleaseLogin(l.ctx, in.Account)

	// 2. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	sessionId := strconv.FormatUint(sessionDetails.SessionId, 10)
	logx.Infof("Start processing login for account=%s with sessionId=%s", in.Account, sessionId)

	// 3. FSM 加载 + 执行 + 保存，出错立即返回
	f := data.InitPlayerFSM()

	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionId, ""); err != nil {
		logx.Errorf("FSM state load failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}

	// 执行 FSM 事件
	if err := f.Event(l.ctx, data.EventProcessLogin); err != nil {
		logx.Errorf("FSM transition error for sessionId=%s, account=%s, event=process_login, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginFSMEventFailed)}
		return resp, nil
	}

	// 保存 FSM 状态
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionId, ""); err != nil {
		logx.Errorf("FSM save failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		// 不阻断，但记录错误
		//todo 让客户端重新登录
		return resp, nil
	}

	// 4. 限制设备数量

	// 添加 SessionId 到设备集合
	sessionKey := constants.GenerateSessionKey(in.Account)
	expire := time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute

	_, err = l.svcCtx.RedisClient.TxPipelined(l.ctx, func(pipe redis.Pipeliner) error {
		pipe.SAdd(l.ctx, sessionKey, sessionDetails.SessionId)
		pipe.Expire(l.ctx, sessionKey, expire)
		return nil
	})
	if err != nil {
		logx.Errorf("Failed to SAdd + Expire in pipeline for sessionKey=%s: %v", sessionKey, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	// 限制最多 N 个设备
	count, err := l.svcCtx.RedisClient.SCard(l.ctx, sessionKey).Result()
	if err != nil {
		logx.Errorf("RedisClient SCard error: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	if count > config.AppConfig.Account.MaxDevicesPerAccount {
		logx.Infof("Account %s exceeds device limit: %d > %d", in.Account, count, config.AppConfig.Account.MaxDevicesPerAccount)

		// 可选：删除旧的 session（基于 TTL 或先入先出策略）
		// 或通知客户端“已有设备登录，是否强制顶号”——这需要客户端支持。

		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(table.LoginError_kTooManyDevices)}
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
	userAccount, err := GetOrInitUserAccount(l.ctx, l.svcCtx.RedisClient, in.Account, time.Duration(config.AppConfig.Timeouts.AccountCacheExpireHours)*time.Second)
	if err != nil {
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

func GetOrInitUserAccount(ctx context.Context, rdb *redis.Client, account string, ttl time.Duration) (*game.UserAccounts, error) {
	key := constants.GetAccountDataKey(account)

	// 优先尝试从 Redis 获取
	cmd := rdb.Get(ctx, key)
	valueBytes, err := cmd.Bytes()

	if errors.Is(err, redis.Nil) {
		// Redis 无数据，创建空对象
		logx.Infof("UserAccounts not found for account=%s, initializing default", account)
		userAccount := &game.UserAccounts{}

		valueBytes, err = proto.Marshal(userAccount)
		if err != nil {
			logx.Errorf("Marshal default UserAccounts failed: %v", err)
			return nil, err
		}

		// 保存到 Redis
		err = rdb.Set(ctx, key, valueBytes, ttl).Err()
		if err != nil {
			logx.Errorf("Failed to save default UserAccounts to Redis for account=%s: %v", account, err)
			return nil, err
		}

		return userAccount, nil
	}

	if err != nil {
		logx.Errorf("Failed to get UserAccounts from Redis: %v", err)
		return nil, err
	}

	// 反序列化返回
	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal(valueBytes, userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed for account=%s: %v", account, err)
		return nil, err
	}

	return userAccount, nil
}
