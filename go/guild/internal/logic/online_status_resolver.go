package logic

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	plpb "proto/player_locator"
)

const playerSessionKeyPrefix = "player:session:"

// OnlineStatusResolver resolves player online state from player_locator session store.
type OnlineStatusResolver struct {
	rdb *redis.Client
}

func NewOnlineStatusResolver(rdb *redis.Client) *OnlineStatusResolver {
	return &OnlineStatusResolver{rdb: rdb}
}

func (r *OnlineStatusResolver) BatchResolve(ctx context.Context, playerIDs []uint64) map[uint64]bool {
	onlineMap := make(map[uint64]bool, len(playerIDs))
	if r == nil || r.rdb == nil || len(playerIDs) == 0 {
		return onlineMap
	}

	keys := make([]string, 0, len(playerIDs))
	ids := make([]uint64, 0, len(playerIDs))
	seen := make(map[uint64]struct{}, len(playerIDs))
	for _, playerID := range playerIDs {
		if _, ok := seen[playerID]; ok {
			continue
		}
		seen[playerID] = struct{}{}
		ids = append(ids, playerID)
		keys = append(keys, fmt.Sprintf("%s%d", playerSessionKeyPrefix, playerID))
	}

	values, err := r.rdb.MGet(ctx, keys...).Result()
	if err != nil {
		logx.Errorf("batch get player sessions failed: %v", err)
		return onlineMap
	}

	for i, value := range values {
		if value == nil {
			continue
		}

		var raw []byte
		switch v := value.(type) {
		case string:
			raw = []byte(v)
		case []byte:
			raw = v
		default:
			continue
		}

		session := &plpb.PlayerSession{}
		if err := proto.Unmarshal(raw, session); err != nil {
			continue
		}
		if session.State == plpb.PlayerSessionState_SESSION_STATE_ONLINE {
			onlineMap[ids[i]] = true
		}
	}

	return onlineMap
}
