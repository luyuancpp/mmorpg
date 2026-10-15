package logic

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	pb "proto/player_locator"
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
	if session.State == pb.PlayerSessionState_SESSION_STATE_ONLINE &&
		session.SessionId == in.NewSessionId && session.RequestId == in.RequestId {
		// RPC response 丢失后的同请求重放。
		return &pb.ReconnectResponse{Success: true, Session: session}, nil
	}
	if session.State != pb.PlayerSessionState_SESSION_STATE_DISCONNECTING {
		return &pb.ReconnectResponse{
			Success:      false,
			ErrorMessage: "session is no longer disconnecting",
		}, nil
	}
	if in.Account != "" && session.Account != "" && in.Account != session.Account {
		return &pb.ReconnectResponse{
			Success:      false,
			ErrorMessage: "session account mismatch",
		}, nil
	}
	// 代次栅栏排在同请求重放之后:重放的代次与记录相等,先判会把丢包重试误拒。
	// 判定基于本次读到的 data,随后的整字节 CAS 以同一份 data 为 expected,判与写之间没有窗口。
	if !loginGenerationAdvances(session.LoginGeneration, in.LoginGeneration) {
		return &pb.ReconnectResponse{
			Success: false,
			ErrorMessage: fmt.Sprintf("stale login generation: current=%d incoming=%d",
				session.LoginGeneration, in.LoginGeneration),
		}, nil
	}

	// Update session with new connection info
	session.SessionId = in.NewSessionId
	session.GateId = in.GateId
	session.GateInstanceId = in.GateInstanceId
	session.TokenId = in.TokenId
	session.TokenExpiryMs = in.TokenExpiryMs
	session.RequestId = in.RequestId
	// 请求为 0(旧版 login)也照写 0:记录如实表示这条会话没有代次,不保留上一会话的值。
	session.LoginGeneration = in.LoginGeneration
	session.SessionVersion++
	session.State = pb.PlayerSessionState_SESSION_STATE_ONLINE
	session.LastActiveTs = time.Now().UnixMilli()

	updated, err := proto.Marshal(session)
	if err != nil {
		return nil, err
	}

	// ONLINE 写入、ready ZREM、processing claim 撤销在同一 Lua 里完成。
	// monitor 若已 claim 旧会话，其 token/payload 会在这里一起失效。
	result, err := replaceSessionAndCancelLease(l.ctx, l.svcCtx, in.PlayerId, data, updated, 0)
	if err != nil {
		return nil, err
	}
	if result != replaceSessionApplied {
		// 不自动重放读改写；调用方重新走登录决策，避免基于陈旧 session 生成新会话。
		return &pb.ReconnectResponse{
			Success:      false,
			ErrorMessage: "session changed concurrently",
		}, nil
	}

	l.Infof("Reconnect: player=%d new_session=%d gate=%s version=%d login_generation=%d",
		in.PlayerId, in.NewSessionId, in.GateId, session.SessionVersion, session.LoginGeneration)

	return &pb.ReconnectResponse{
		Success: true,
		Session: session,
	}, nil
}
