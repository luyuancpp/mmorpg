package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pkg"
	"robot/proto/service/grpc/login"
)

func ClientPlayerLoginLoginClientHandler(client *pkg.GameClient, response *login.LoginResponse) {
	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)
}

func ClientPlayerLoginLoginHandler(player *gameobject.Player, response *login.LoginResponse) {
}
