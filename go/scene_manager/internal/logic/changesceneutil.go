package logic

import (
	"context"
	"fmt"
	"time"

	"scene_manager/internal/storage"
	"scene_manager/internal/svc"

	"google.golang.org/protobuf/proto"
)

const (
	ChangeSceneState_None         = int32(storage.ChangeSceneState_NONE)
	ChangeSceneState_PendingLeave = int32(storage.ChangeSceneState_PENDING_LEAVE)
	ChangeSceneState_WaitingEnter = int32(storage.ChangeSceneState_WAITING_ENTER)
	ChangeSceneState_EnterSucceed = int32(storage.ChangeSceneState_ENTER_SUCCEED)

	ChangeSceneType_SameGs      = 1
	ChangeSceneType_DifferentGs = 2
)

type ChangeSceneInfo struct {
	SceneId uint64
	State   int32
	Type    int32
}

func getChangeSceneKey(playerId uint64) string {
	return fmt.Sprintf("player:%d:change_scene", playerId)
}

func getPlayerLocationKey(playerId uint64) string {
	return fmt.Sprintf("player:%d:location", playerId)
}

// PushChangeSceneInfo sets the change scene state.
// It uses protobuf for storage.
func PushChangeSceneInfo(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, sceneId uint64, changeType int) error {
	key := getChangeSceneKey(playerId)

	state := &storage.ChangeSceneState{
		TargetSceneId: sceneId,
		State:         storage.ChangeSceneState_PENDING_LEAVE,
		ChangeType:    int32(changeType),
		RequestTime:   time.Now().Unix(),
	}

	data, err := proto.Marshal(state)
	if err != nil {
		return err
	}

	return svcCtx.Redis.Set(key, string(data))
}

func GetChangeSceneInfo(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64) (*ChangeSceneInfo, error) {
	key := getChangeSceneKey(playerId)
	val, err := svcCtx.Redis.Get(key)
	if err != nil {
		return nil, err
	}
	if val == "" {
		return nil, nil
	}

	state := &storage.ChangeSceneState{}
	if err := proto.Unmarshal([]byte(val), state); err != nil {
		return nil, err
	}

	return &ChangeSceneInfo{
		SceneId: state.TargetSceneId,
		State:   int32(state.State),
		Type:    state.ChangeType,
	}, nil
}

func UpdateChangeSceneState(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, newState int32) error {
	key := getChangeSceneKey(playerId)
	val, err := svcCtx.Redis.Get(key)
	if err != nil || val == "" {
		return fmt.Errorf("change scene info not found")
	}

	state := &storage.ChangeSceneState{}
	if err := proto.Unmarshal([]byte(val), state); err != nil {
		return err
	}

	state.State = storage.ChangeSceneState_State(newState)
	data, err := proto.Marshal(state)
	if err != nil {
		return err
	}
	
	return svcCtx.Redis.Set(key, string(data))
}

func ClearChangeSceneInfo(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64) error {
	key := getChangeSceneKey(playerId)
	_, err := svcCtx.Redis.Del(key)
	return err
}

func IsSceneOnLocalNode(ctx context.Context, svcCtx *svc.ServiceContext, sceneId uint64) (bool, error) {
	key := fmt.Sprintf("scene:%d:node", sceneId)
	nodeId, err := svcCtx.Redis.Get(key)
	if err != nil {
		return false, err
	}
	return nodeId == svcCtx.Config.NodeID, nil
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
