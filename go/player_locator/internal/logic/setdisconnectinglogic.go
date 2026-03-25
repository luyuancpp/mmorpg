package logic

import (
	"context"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	common "proto/common/base"
	pb "proto/player_locator"
)

type SetDisconnectingLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewSetDisconnectingLogic(ctx context.Context, svcCtx *svc.ServiceContext) *SetDisconnectingLogic {
	return &SetDisconnectingLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *SetDisconnectingLogic) SetDisconnecting(in *pb.SetDisconnectingRequest) (*common.Empty, error) {
	key := sessionKey(in.PlayerId)

	// Get current session
	data, err := l.svcCtx.RedisClient.Get(l.ctx, key).Bytes()
	if err == redis.Nil {
		return &common.Empty{}, nil // No session, nothing to disconnect
	}
	if err != nil {
		return nil, err
	}

	session := &pb.PlayerSession{}
	if err := proto.Unmarshal(data, session); err != nil {
		return nil, err
	}

	// Only the current session can start disconnect
	if session.SessionId != in.SessionId {
		l.Infof("SetDisconnecting: session mismatch player=%d current=%d requested=%d, ignoring",
			in.PlayerId, session.SessionId, in.SessionId)
		return &common.Empty{}, nil
	}

	// Calculate TTL
	ttl := time.Duration(in.LeaseTtlSeconds) * time.Second
	if ttl == 0 {
		ttl = time.Duration(l.svcCtx.Config.Lease.DefaultTTLSeconds) * time.Second
	}
	if ttl == 0 {
		ttl = 30 * time.Second
	}

	// Update state to disconnecting
	session.State = pb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	updated, err := proto.Marshal(session)
	if err != nil {
		return nil, err
	}

	// Set with TTL — Redis auto-deletes when lease expires
	if err := l.svcCtx.RedisClient.Set(l.ctx, key, updated, ttl).Err(); err != nil {
		return nil, err
	}

	// Add to lease tracking ZSET for the monitor to detect expiry
	expiry := float64(time.Now().Add(ttl).Unix())
	l.svcCtx.RedisClient.ZAdd(l.ctx, LeaseZSetKey, redis.Z{
		Score:  expiry,
		Member: in.PlayerId,
	})

	l.Infof("SetDisconnecting: player=%d session=%d ttl=%v", in.PlayerId, in.SessionId, ttl)
	return &common.Empty{}, nil
}
