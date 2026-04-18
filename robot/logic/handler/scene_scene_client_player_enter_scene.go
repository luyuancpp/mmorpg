package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerEnterSceneHandler(player *gameobject.Player, response *scene.EnterSceneC2SResponse) {
	if response.GetErrorMessage() != nil {
		zap.L().Warn("enter scene rejected",
			zap.Uint64("player", player.ID),
			zap.String("error", response.GetErrorMessage().String()),
		)
		return
	}
	zap.L().Info("enter scene accepted", zap.Uint64("player", player.ID))
}
