package gameobject

import (
	"robot/generated/pb/game"
	"robot/proto/service/rpc/scene"
)

func (player *Player) SetupPlayerSkillsOnLogin() {
	rq := &scene.GetSkillListRequest{}

	player.Send(rq, game.SceneSkillClientPlayerGetSkillListMessageId)
}
