package sessioncleaner

import (
	"context"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"login/internal/constants"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/loginsessionstore"
)

type CentreClient interface {
	Send(msg interface{}, msgId uint32) error
}

func CleanupSession(
	ctx context.Context,
	redisClient *redis.Client,
	sessionId uint64,
	logicTag string,
) error {
	sessionInfo, err := loginsessionstore.GetLoginSession(ctx, redisClient, sessionId)
	if err != nil {
		logx.Errorf("[%s] Failed to load login session for sessionId=%d: %v", logicTag, sessionId, err)
	} else {
		account := sessionInfo.Account

		if err := fsmstore.DeleteFSMState(ctx, redisClient, account, ""); err != nil {
			logx.Errorf("[%s] Delete FSM state failed: account=%s, err=%v", logicTag, account, err)
		}

		// 删除设备集合
		deviceKey := constants.GenerateSessionKey(account)
		_ = redisClient.SRem(ctx, deviceKey, sessionId).Err()
	}

	// 删除 RedisClient session
	if err := loginsessionstore.DeleteLoginSession(ctx, redisClient, sessionId); err != nil {
		logx.Errorf("[%s] Delete session from RedisClient failed: sessionId=%d, err=%v", logicTag, sessionId, err)
	}

	logx.Infof("[%s] Session cleanup completed for sessionId=%d", logicTag, sessionId)
	return nil
}
