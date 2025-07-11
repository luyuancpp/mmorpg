package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/redis/go-redis/v9"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/svc"
	"login/pb/game"
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

	// 1. 获取 SessionId 并从 Redis 获取 account
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx) // 获取当前 session 详情
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or invalid during player creation")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 假设 sessionKey 是 "login_account_sessions:{session_id}"，可以通过 sessionId 获取到 account
	accountKey := fmt.Sprintf("login_account_sessions:%s", sessionDetails.SessionId)
	accountCmd := l.svcCtx.Redis.Get(l.ctx, accountKey)
	account, err := accountCmd.Result()
	if err != nil {
		logx.Errorf("Failed to retrieve account by sessionId, err: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountNotFound)}
		return resp, err
	}

	logx.Infof("Retrieved account from session: %s", account)

	// 2. 加载账户数据
	accountDataKey := "account" + account
	cmd := l.svcCtx.Redis.Get(l.ctx, accountDataKey)
	if err := cmd.Err(); err != nil {
		if errors.Is(err, redis.Nil) {
			logx.Infof("Account not found in redis: %s", account)
			resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountNotFound)}
			return resp, nil
		}
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisError)}
		logx.Errorf("Redis get failed, account: %s, err: %v", account, err)
		return resp, err
	}

	// 3. FSM 状态管理
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.Redis, f, account, ""); err != nil {
		logx.Errorf("Failed to load FSM state: %v", err)
	}
	if err := f.Event(context.Background(), "create_char"); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginFsmFailed)}
		logx.Errorf("FSM create_char failed, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 4. 解码账户数据
	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal([]byte(cmd.Val()), userAccount); err != nil {
		logx.Errorf("Failed to unmarshal user account, err: %v", err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginDataParseFailed)}
		return resp, nil
	}
	if userAccount.SimplePlayers == nil {
		userAccount.SimplePlayers = &game.AccountSimplePlayerList{
			Players: make([]*game.AccountSimplePlayer, 0),
		}
	}

	// 5. 创建角色
	if len(userAccount.SimplePlayers.Players) >= 5 {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountPlayerFull)}
		logx.Infof("Account player limit reached: %s", account)
		return resp, nil
	}

	// 新角色 ID
	newPlayerId := uint64(l.svcCtx.SnowFlake.Generate())
	newPlayer := &game.AccountSimplePlayer{
		PlayerId: newPlayerId,
	}
	userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players, newPlayer)

	// 6. 回写 Redis
	dataBytes, err := proto.Marshal(userAccount)
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginDataSerializeFailed)}
		logx.Errorf("Failed to marshal user account, err: %v", err)
		return resp, nil
	}

	if err := l.svcCtx.Redis.Set(l.ctx, accountDataKey, dataBytes, 12*time.Hour).Err(); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginRedisSetFailed)}
		logx.Errorf("Failed to set user account in Redis, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 7. 保存 FSM 状态
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.Redis, f, account, ""); err != nil {
		logx.Errorf("Failed to save FSM state, account: %s, err: %v", account, err)
	}

	// 8. 返回角色信息
	for _, p := range userAccount.SimplePlayers.Players {
		resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: p})
	}

	logx.Infof("Player created successfully, account: %s, playerId: %d", account, newPlayerId)
	return resp, nil
}
