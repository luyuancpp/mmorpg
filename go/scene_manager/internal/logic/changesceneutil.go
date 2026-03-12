package logic

import (
	"context"
	"fmt"
	"time"

	"scene_manager/internal/storage"
	"scene_manager/internal/svc"

	"google.golang.org/protobuf/proto"
)

func getPlayerLocationKey(playerId uint64) string {
	return fmt.Sprintf("player:%d:location", playerId)
}


// GetPlayerLocation retrieves the current scene and node for a player
func GetPlayerLocation(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64) (*storage.PlayerLocation, error) {
	key := getPlayerLocationKey(playerId)
	val, err := svcCtx.Redis.Get(key)
	if err != nil {
		return nil, err
	}
	if val == "" {
		return nil, nil
	}
	
	loc := &storage.PlayerLocation{}
	if err := proto.Unmarshal([]byte(val), loc); err != nil {
		return nil, err
	}
	return loc, nil
}

// UpdatePlayerLocation updates the player's location using protobuf
func UpdatePlayerLocation(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, sceneId uint64, nodeId string) error {
	key := getPlayerLocationKey(playerId)
	
	loc := &storage.PlayerLocation{
		SceneId:    sceneId,
		NodeId:     nodeId,
		UpdateTime: uint64(time.Now().Unix()),
	}

	data, err := proto.Marshal(loc)
	if err != nil {
		return err
	}

	return svcCtx.Redis.Set(key, string(data))
}

// DeletePlayerLocation removes the player's location from Redis
func DeletePlayerLocation(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64) error {
	key := getPlayerLocationKey(playerId)
	_, err := svcCtx.Redis.Del(key)
	return err
}
