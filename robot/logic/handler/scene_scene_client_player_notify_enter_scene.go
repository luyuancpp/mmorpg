package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerNotifyEnterSceneHandler(player *gameobject.Player, response *scene.EnterSceneS2C) {
	if response.GetSceneInfo() == nil {
		return
	}
	player.SetSceneInfo(response.GetSceneInfo().GetSceneId(), response.GetSceneInfo().GetSceneConfigId())
	player.SignalSceneReady()
	zap.L().Info("notify enter scene",
		zap.Uint64("player", player.ID),
		zap.Uint64("scene_id", response.GetSceneInfo().GetSceneId()),
		zap.Uint32("scene_config_id", response.GetSceneInfo().GetSceneConfigId()),
	)
}
