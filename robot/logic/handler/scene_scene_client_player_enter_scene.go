package handler

import (
	"go.uber.org/zap"

	"robot/logic/gameobject"
	"robot/proto/scene"
)

func SceneSceneClientPlayerEnterSceneHandler(player *gameobject.Player, response *scene.EnterSceneC2SResponse) {
	zap.L().Info("entered scene", zap.Uint64("player", player.PlayerId))
}
