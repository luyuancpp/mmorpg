package gameobject

import (
	"robot/generated/pb/game"
)

func (player *Player) SetupPlayerSkillsOnLogin() {
	rq := &game.GetSkillListRequest{}

	player.Send(rq, game.SceneSkillClientPlayerGetSkillListMessageId)
}
