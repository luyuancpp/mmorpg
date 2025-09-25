package handler

import (
	"robot/logic/behaviortree"
	"robot/pkg"
	"robot/proto/service/go/grpc/login"
)

func ClientPlayerLoginLoginHandler(client *pkg.GameClient, response *login.LoginResponse) {
	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)
}
