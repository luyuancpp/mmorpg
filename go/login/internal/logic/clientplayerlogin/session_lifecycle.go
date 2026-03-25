package clientplayerloginlogic

import (
	"context"

	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/sessionmanager"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	login_proto_common "proto/common/base"

	"github.com/zeromicro/go-zero/core/logx"
)

func getSessionDetailsForAction(ctx context.Context, action string) (*login_proto_common.SessionDetails, bool) {
	sessionDetails, ok := ctxkeys.GetSessionDetails(ctx)
	if !ok {
		logx.Errorf("Session not found in context during %s", action)
		return nil, false
	}

	return sessionDetails, true
}

func cleanupLoginSessionState(ctx context.Context, svcCtx *svc.ServiceContext, sessionID uint64, logicTag string) {
	if err := sessioncleaner.CleanupSession(ctx, svcCtx.RedisClient, sessionID, logicTag); err != nil {
		logx.Errorf("%s cleanup failed: %v", logicTag, err)
	}
}

func deletePlayerSession(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) {
	if playerID == 0 {
		return
	}

	if err := sessionmanager.DeleteSession(ctx, svcCtx.PlayerLocatorClient, playerID); err != nil {
		logx.Errorf("Failed to delete session for player %d: %v", playerID, err)
	}
}

func markPlayerSessionDisconnecting(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64, sessionID uint64) {
	if playerID == 0 {
		return
	}

	if err := sessionmanager.SetSessionDisconnecting(ctx, svcCtx.PlayerLocatorClient, playerID, sessionID); err != nil {
		logx.Errorf("Failed to set session disconnecting for player %d: %v", playerID, err)
		return
	}

	logx.Infof("Session disconnecting with 30s lease: player=%d session=%d", playerID, sessionID)
}
