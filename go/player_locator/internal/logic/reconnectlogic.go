package logic

import (
	"context"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	pb "player_locator/proto/player_locator"
)

type ReconnectLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewReconnectLogic(ctx context.Context, svcCtx *svc.ServiceContext) *ReconnectLogic {
	return &ReconnectLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *ReconnectLogic) Reconnect(in *pb.ReconnectRequest) (*pb.ReconnectResponse, error) {
	key := sessionKey(in.PlayerId)

	data, err := l.svcCtx.RedisClient.Get(l.ctx, key).Bytes()
	if err == redis.Nil {
		return &pb.ReconnectResponse{
			Success:      false,
			ErrorMessage: "no session found for player",
		}, nil
	}
	if err != nil {
		return nil, err
	}

	session := &pb.PlayerSession{}
	if err := proto.Unmarshal(data, session); err != nil {
		return nil, err
	}

	// Update session with new connection info
	session.SessionId = in.NewSessionId
	session.GateId = in.GateId
	session.GateInstanceId = in.GateInstanceId
	session.TokenId = in.TokenId
	session.TokenExpiryMs = in.TokenExpiryMs
	session.RequestId = in.RequestId
	session.SessionVersion++
	session.State = pb.PlayerSessionState_SESSION_STATE_ONLINE
	session.LastActiveTs = time.Now().UnixMilli()

	updated, err := proto.Marshal(session)
	if err != nil {
		return nil, err
	}

	// Persist without TTL — cancels disconnect lease effectively
	if err := l.svcCtx.RedisClient.Set(l.ctx, key, updated, 0).Err(); err != nil {
		return nil, err
	}

	// Remove from lease tracking
	l.svcCtx.RedisClient.ZRem(l.ctx, LeaseZSetKey, in.PlayerId)

	l.Infof("Reconnect: player=%d new_session=%d gate=%s version=%d",
		in.PlayerId, in.NewSessionId, in.GateId, session.SessionVersion)

	return &pb.ReconnectResponse{
		Success: true,
		Session: session,
	}, nil
}
