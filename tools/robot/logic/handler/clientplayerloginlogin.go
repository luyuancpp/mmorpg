package handler

import (
	"robot/logic/behaviortree"
	"robot/pkg"
	"robot/proto/service/grpc/login"
)

func ClientPlayerLoginLoginHandler(client *pkg.GameClient, response *login.LoginResponse) {
	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)
}
