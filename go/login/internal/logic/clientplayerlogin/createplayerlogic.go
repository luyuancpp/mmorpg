package clientplayerloginlogic

import (
	"context"
	"errors"
	"generated/pb/game/"
	"game/data"
	"game/internal/config"
	"game/internal/constants"
	"game/internal/logic/pkg/ctxkeys"
	"game/internal/logic/pkg/fsmstore"
	"game/internal/logic/pkg/locker"
	"game/internal/logic/pkg/loginsessionstore"
	"game/internal/svc"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"strconv"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

type CreatePlayerLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreatePlayerLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreatePlayerLogic {
	return &CreatePlayerLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *CreatePlayerLogic) CreatePlayer(in *game.CreatePlayerRequest) (*game.CreatePlayerResponse, error) {
	resp := &game.CreatePlayerResponse{
		Players: make([]*game.AccountSimplePlayerWrapper, 0),
	}

	// 1. 获取 Session 详情
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or invalid during player creation")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. 获取 LoginSessionInfo（含账号）
	session, err := loginsessionstore.GetLoginSession(l.ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionNotFound)}
		return resp, nil
	}
	account := session.Account

	// 3. 加锁（防止并发创建角色）
	locker := locker.NewAccountLocker(l.svcCtx.RedisClient, time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second)
	ok, err = locker.AcquireCreate(l.ctx, account)
	if err != nil || !ok {
		logx.Errorf("CreatePlayer lock acquire failed for account=%s: %v", account, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer locker.ReleaseCreate(l.ctx, account)

	// 4. 加载账户数据
	accountDataKey := constants.GetAccountDataKey(account)
	cmd := l.svcCtx.RedisClient.Get(l.ctx, accountDataKey)
	if err := cmd.Err(); err != nil {
		if errors.Is(err, redis.Nil) {
			logx.Infof("Account not found in redis: %s", account)
			resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountNotFound)}
			return resp, nil
		}
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisError)}
		logx.Errorf("RedisClient get failed, account: %s, err: %v", account, err)
		return resp, err
	}

	// 5. FSM 状态管理（基于 sessionId）
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionIdStr, ""); err != nil {
		logx.Errorf("Failed to load FSM state for session %s: %v", sessionIdStr, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}
	if err := f.Event(l.ctx, data.EventCreateChar); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFsmFailed)}
		logx.Errorf("FSM create_char failed, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 6. 解码账户数据
	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal([]byte(cmd.Val()), userAccount); err != nil {
		logx.Errorf("Failed to unmarshal user account, err: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginDataParseFailed)}
		return resp, nil
	}
	if userAccount.SimplePlayers == nil {
		userAccount.SimplePlayers = &game.AccountSimplePlayerList{Players: make([]*game.AccountSimplePlayer, 0)}
	}

	// 7. 创建角色
	if len(userAccount.SimplePlayers.Players) >= 5 {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountPlayerFull)}
		logx.Infof("Account player limit reached: %s", account)
		return resp, nil
	}
	newPlayerId := uint64(l.svcCtx.SnowFlake.Generate())
	newPlayer := &game.AccountSimplePlayer{PlayerId: newPlayerId}
	userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players, newPlayer)

	// 8. 回写 RedisClient
	dataBytes, err := proto.Marshal(userAccount)
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginDataSerializeFailed)}
		logx.Errorf("Failed to marshal user account, err: %v", err)
		return resp, nil
	}
	if err := l.svcCtx.RedisClient.Set(l.ctx, accountDataKey, dataBytes, time.Hour*time.Duration(config.AppConfig.Account.CacheExpireHours)).Err(); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		logx.Errorf("Failed to set user account in RedisClient, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 9. 保存 FSM 状态（用 sessionId）
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionIdStr, ""); err != nil {
		logx.Errorf("Failed to save FSM state, sessionId: %s, err: %v", sessionIdStr, err)
	}

	// 10. 返回角色信息
	for _, p := range userAccount.SimplePlayers.Players {
		resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: p})
	}

	logx.Infof("Player created successfully, account: %s, playerId: %d", account, newPlayerId)
	return resp, nil
}
