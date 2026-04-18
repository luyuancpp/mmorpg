package handler

import (
	"fmt"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSkillClientPlayerNotifySkillInterruptedHandler(player *gameobject.Player, response *scene.SkillInterruptedS2C) {
	player.NoteSkillResponse(fmt.Sprintf("interrupted reason=%d", response.GetReasonCode()))
}
