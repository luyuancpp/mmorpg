package behaviortree

import (
	"client/interfaces"
	"client/logic"
	"client/pb/game"
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type CreatePlayer struct {
	Action
}

func (this *CreatePlayer) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *CreatePlayer) OnTick(tick *Tick) b3.Status {
	clientI := tick.Blackboard.GetMem(ClientIdentifier)

	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	rq := &game.CreatePlayerRequest{}
	client.Send(rq, game.LoginServiceCreatePlayerMessageId)
	return b3.SUCCESS
}

type IsRoleListEmpty struct {
	Action
}

func (this *IsRoleListEmpty) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *IsRoleListEmpty) OnTick(tick *Tick) b3.Status {
	playerListI := tick.Blackboard.GetMem(PlayerListIdentifier)
	if nil == playerListI {
		return b3.FAILURE
	}

	playerList, ok := playerListI.([]*game.AccountSimplePlayerWrapper)
	if !ok {
		return b3.FAILURE
	}

	if len(playerList) > 0 {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type PlayerEnterGame struct {
	Action
}

func (this *PlayerEnterGame) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *PlayerEnterGame) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	clientI := tick.Blackboard.GetMem(ClientIdentifier)
	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	// 从黑板中获取玩家列表
	playerListI := tick.Blackboard.GetMem(PlayerListIdentifier)
	if playerListI == nil {
		return b3.FAILURE
	}

	playerList, ok := playerListI.([]*game.AccountSimplePlayerWrapper)
	if !ok {
		return b3.FAILURE
	}

	// 处理玩家数据
	playerId := playerList[0].Player.PlayerId
	zap.L().Info("Player login", zap.Uint64("player id", playerId))
	logic.PlayerList.Set(playerId, logic.NewMainPlayer(playerId, client))

	// 发送请求
	rq := &game.EnterGameRequest{PlayerId: playerId}
	client.Send(rq, game.LoginServiceEnterGameMessageId)

	return b3.SUCCESS
}

type AlreadyLoggedIn struct {
	Action
}

func (this *AlreadyLoggedIn) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *AlreadyLoggedIn) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	clientI := tick.Blackboard.GetMem(ClientIdentifier)
	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	if client.GetPlayerId() <= 0 {
		return b3.FAILURE
	}

	return b3.SUCCESS
}
