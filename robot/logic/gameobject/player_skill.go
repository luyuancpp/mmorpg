package gameobject

import "robot/pb/game"

func (player *Player) SetupPlayerSkillsOnLogin() {
	rq := &game.GetSkillListRequest{}

	player.Send(rq, game.ClientPlayerSkillServiceGetSkillListMessageId)
}
