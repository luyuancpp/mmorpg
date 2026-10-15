package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/sessionfence"
	"login/internal/svc"
	login_proto "proto/login"

	"github.com/zeromicro/go-zero/core/logx"
)

type DisconnectLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewDisconnectLogic(ctx context.Context, svcCtx *svc.ServiceContext) *DisconnectLogic {
	return &DisconnectLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *DisconnectLogic) Disconnect(in *login_proto.LoginNodeDisconnectRequest) (*login_proto.LoginEmptyResponse, error) {
	sessionDetails, ok := getSessionDetailsForAction(l.ctx, "disconnect")
	if !ok {
		return &login_proto.LoginEmptyResponse{}, nil
	}

	// gate 连接会话栅栏(docs/design/client-request-ordering.md §6.2):先标记这条连接已关闭,再清理登录态。
	// 之后晚到的 Login 不再写 login_session;EnterGame 后台链在绑定点看到关闭会自己补挂断线租约。
	// 反过来,后台链已经绑定了玩家、而 gate 会话还没收到 BindSession(这里的 player_id 为 0)时,
	// 从栅栏读回绑定的玩家补挂租约 —— 原先在 playerID == 0 处早退,玩家会永久停在 ONLINE。
	playerID := sessionDetails.PlayerId
	if fenceKey, fenced := sessionfence.KeyOf(sessionDetails); fenced {
		bound, err := sessionfence.Close(l.ctx, l.svcCtx.RedisClient, fenceKey, sessionFenceTTL)
		if err != nil {
			// 栅栏写失败不阻断清理:照旧按会话里的身份清,只是失去「补挂租约」这一层兜底。
			logx.Errorf("Disconnect session fence close failed: sessionId=%d err=%v", sessionDetails.SessionId, err)
		} else if playerID == 0 {
			playerID = bound
		}
	}

	cleanupLoginSessionState(l.ctx, l.svcCtx, sessionDetails.SessionId, "disconnect")

	// Mark session as disconnecting with 30s lease in Redis (replaces Centre's DelayedCleanupTimer).
	// When the Redis key TTL expires, the session is effectively cleaned up.
	markPlayerSessionDisconnecting(l.ctx, l.svcCtx, playerID, in.SessionId)

	return &login_proto.LoginEmptyResponse{}, nil
}
