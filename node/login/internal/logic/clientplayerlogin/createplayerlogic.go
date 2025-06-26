package clientplayerloginlogic

import (
	"context"
	"errors"
	"github.com/looplab/fsm"
	"github.com/redis/go-redis/v9"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
	"time"

	"login/internal/svc"
	"login/pb/game"

	"github.com/golang/protobuf/proto"
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

	session, ok := ctxkeys.GetSession(l.ctx)
	if !ok || nil == session {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginSessionIdNotFound),
		}
		logx.Error("session not found in context")
		return resp, nil
	}

	accountKey := "account" + session.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, accountKey)
	if err := cmd.Err(); err != nil {
		if errors.Is(err, redis.Nil) {
			resp.ErrorMessage = &game.TipInfoMessage{
				Id: uint32(game.LoginError_kLoginAccountNotFound),
			}
			logx.Infof("account not found in redis: %s", session.Account)
			return resp, nil
		}
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginRedisError),
		}
		logx.Errorf("redis get failed, account: %s, err: %v", session.Account, err)
		return resp, err
	}

	// FSM 登录流程事件（延迟触发）
	defer func(fsmIns *fsm.FSM, ctx context.Context, event string, args ...interface{}) {
		if err := fsmIns.Event(ctx, event, args); err != nil {
			logx.Errorf("fsm login event failed: %v", err)
		}
	}(session.Fsm, context.Background(), data.EventProcessLogin)

	// FSM 进入创建角色状态
	if err := session.Fsm.Event(context.Background(), data.EventCreateChar); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginFsmFailed),
		}
		logx.Errorf("fsm create-char event failed, account: %s, err: %v", session.Account, err)
		return resp, nil
	}

	accountData := session.UserAccount
	if err := proto.Unmarshal([]byte(cmd.Val()), accountData); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginDataParseFailed),
		}
		logx.Errorf("failed to unmarshal user account data, account: %s, err: %v", session.Account, err)
		return resp, nil
	}

	if accountData.SimplePlayers == nil {
		accountData.SimplePlayers = &game.AccountSimplePlayerList{
			Players: make([]*game.AccountSimplePlayer, 0),
		}
	}

	if len(accountData.SimplePlayers.Players) >= 5 {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginAccountPlayerFull),
		}
		logx.Infof("account player limit reached, account: %s", session.Account)
		return resp, nil
	}

	newPlayerId := uint64(l.svcCtx.SnowFlake.Generate())
	newPlayer := &game.AccountSimplePlayer{
		PlayerId: newPlayerId,
	}

	accountData.SimplePlayers.Players = append(accountData.SimplePlayers.Players, newPlayer)

	for _, p := range accountData.SimplePlayers.Players {
		resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: p})
	}

	dataBytes, err := proto.Marshal(accountData)
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginDataSerializeFailed),
		}
		logx.Errorf("failed to marshal account data, account: %s, err: %v", session.Account, err)
		return resp, nil
	}

	err = l.svcCtx.Redis.Set(l.ctx, accountKey, dataBytes, 12*time.Hour).Err()
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginRedisSetFailed),
		}
		logx.Errorf("failed to set redis account data, account: %s, err: %v", session.Account, err)
		return resp, nil
	}

	logx.Infof("player created successfully, account: %s, player_id: %d", session.Account, newPlayerId)
	return resp, nil
}
