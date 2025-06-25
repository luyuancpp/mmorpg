package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"
	"robot/interfaces"
	"robot/pb/game"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type CreatePlayer struct {
	Action
}

func (c *CreatePlayer) Initialize(setting *BTNodeCfg) {
	c.Action.Initialize(setting)
}

func (c *CreatePlayer) OnTick(tick *Tick) b3.Status {
	clientI := tick.Blackboard.GetMem(ClientBoardKey)

	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	account := client.GetAccount()
	zap.L().Info("attempting to create player",
		zap.String("account_name", account),
	)

	rq := &game.CreatePlayerRequest{}
	client.Send(rq, game.ClientPlayerLoginCreatePlayerMessageId)

	return b3.SUCCESS
}

type IsRoleListEmpty struct {
	Action
}

func (i *IsRoleListEmpty) Initialize(setting *BTNodeCfg) {
	i.Action.Initialize(setting)
}

func (i *IsRoleListEmpty) OnTick(tick *Tick) b3.Status {
	playerListI := tick.Blackboard.GetMem(PlayerListBoardKey)
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

func (p *PlayerEnterGame) Initialize(setting *BTNodeCfg) {
	p.Action.Initialize(setting)
}

func (p *PlayerEnterGame) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	clientI := tick.Blackboard.GetMem(ClientBoardKey)
	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	// 从黑板中获取玩家列表
	playerListI := tick.Blackboard.GetMem(PlayerListBoardKey)
	if playerListI == nil {
		return b3.FAILURE
	}

	playerList, ok := playerListI.([]*game.AccountSimplePlayerWrapper)
	if !ok {
		return b3.FAILURE
	}

	// 处理玩家数据
	playerId := playerList[0].Player.PlayerId
	account := client.GetAccount()
	
	zap.L().Info("send Player login",
		zap.Uint64("player id", playerId), zap.String("account_name", account))

	// 发送请求
	rq := &game.EnterGameRequest{PlayerId: playerId}
	client.Send(rq, game.ClientPlayerLoginEnterGameMessageId)

	return b3.SUCCESS
}

type AlreadyLoggedIn struct {
	Action
}

func (a *AlreadyLoggedIn) Initialize(setting *BTNodeCfg) {
	a.Action.Initialize(setting)
}

func (a *AlreadyLoggedIn) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	clientI := tick.Blackboard.GetMem(PlayerBoardKey)
	if clientI == nil {
		return b3.FAILURE
	}

	return b3.SUCCESS
}
