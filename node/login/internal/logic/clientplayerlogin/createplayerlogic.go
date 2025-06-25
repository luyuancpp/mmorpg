package clientplayerloginlogic

import (
	"context"
	"github.com/looplab/fsm"
	"go.uber.org/zap"
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
		ErrorMessage: &game.TipInfoMessage{},
		Players:      make([]*game.AccountSimplePlayerWrapper, 0),
	}

	session, ok := ctxkeys.GetSession(l.ctx)
	if !ok {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginSessionIdNotFound),
		}
		zap.L().Error("session not found in context")
		return resp, nil
	}

	accountKey := "account" + session.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, accountKey)
	if err := cmd.Err(); err != nil {
		if err == redis.Nil {
			resp.ErrorMessage = &game.TipInfoMessage{
				Id: uint32(game.LoginError_kLoginAccountNotFound),
			}
			zap.L().Warn("account not found in redis", zap.String("account", session.Account))
			return resp, nil
		}
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginRedisError),
		}
		zap.L().Error("redis get failed", zap.String("account", session.Account), zap.Error(err))
		return resp, err
	}

	// FSM 登录流程事件（延迟触发）
	defer func(fsmIns *fsm.FSM, ctx context.Context, event string, args ...interface{}) {
		if err := fsmIns.Event(ctx, event, args); err != nil {
			zap.L().Error("fsm login event failed", zap.Error(err))
		}
	}(session.Fsm, context.Background(), data.EventProcessLogin)

	// FSM 进入创建角色状态
	if err := session.Fsm.Event(context.Background(), data.EventCreateChar); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginFsmFailed),
		}
		zap.L().Error("fsm create-char event failed", zap.String("account", session.Account), zap.Error(err))
		return resp, nil
	}

	accountData := &game.UserAccounts{}
	if err := proto.Unmarshal([]byte(cmd.Val()), accountData); err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginDataParseFailed),
		}
		zap.L().Error("failed to unmarshal user account data",
			zap.String("account", session.Account),
			zap.Error(err),
		)
		return resp, nil
	}

	// 初始化玩家列表
	if accountData.SimplePlayers == nil {
		accountData.SimplePlayers = &game.AccountSimplePlayerList{
			Players: make([]*game.AccountSimplePlayer, 0),
		}
	}

	// 玩家已满
	if len(accountData.SimplePlayers.Players) >= 5 {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginAccountPlayerFull),
		}
		zap.L().Info("player slot full", zap.String("account", session.Account))
		return resp, nil
	}

	// 创建新角色
	newPlayerId := uint64(l.svcCtx.SnowFlake.Generate())
	newPlayer := &game.AccountSimplePlayer{
		PlayerId: newPlayerId,
		// 可以加初始化字段，如名字、创建时间等
	}

	accountData.SimplePlayers.Players = append(accountData.SimplePlayers.Players, newPlayer)

	for _, p := range accountData.SimplePlayers.Players {
		resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: p})
	}

	// 保存数据到 Redis
	dataBytes, err := proto.Marshal(accountData)
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginDataSerializeFailed),
		}
		zap.L().Error("failed to marshal account data",
			zap.String("account", session.Account),
			zap.Error(err),
		)
		return resp, nil
	}

	err = l.svcCtx.Redis.Set(l.ctx, accountKey, dataBytes, 12*time.Hour).Err()
	if err != nil {
		resp.ErrorMessage = &game.TipInfoMessage{
			Id: uint32(game.LoginError_kLoginRedisSetFailed),
		}
		zap.L().Error("failed to save account data to redis",
			zap.String("account", session.Account),
			zap.Error(err),
		)
		return resp, nil
	}

	// 成功日志
	zap.L().Info("player created successfully",
		zap.String("account", session.Account),
		zap.Uint64("player_id", newPlayerId),
	)

	return resp, nil
}
