package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSkillClientPlayerReleaseSkillHandler(player *gameobject.Player, response *scene.ReleaseSkillResponse) {
	if response.GetErrorMessage() != nil {
		errText := response.GetErrorMessage().String()
		player.NoteSkillResponse(errText)
		zap.L().Warn("release skill rejected", zap.Uint64("player", player.ID), zap.String("error", errText))
		return
	}
	player.NoteSkillResponse("")
}
