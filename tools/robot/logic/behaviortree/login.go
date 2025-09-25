package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"
	"robot/generated/pb/game"
	"robot/interfaces"
	"robot/proto/service/go/grpc/login"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type Login struct {
	Action
}

func (c *Login) Initialize(setting *BTNodeCfg) {
	c.Action.Initialize(setting)
}

func (c *Login) OnTick(tick *Tick) b3.Status {

	clientI := tick.Blackboard.GetMem(ClientBoardKey)

	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	rq := &login.LoginRequest{
		Account:  client.GetAccount(),
		Password: client.GetAccount(),
	}
	client.Send(rq, game.ClientPlayerLoginLoginMessageId)
	zap.L().Info("send Player login",
		zap.String("account_name", client.GetAccount()))
	return b3.SUCCESS
}

type CreatePlayer struct {
	Action
}

func (c *CreatePlayer) Initialize(setting *BTNodeCfg) {
	c.Action.Initialize(setting)
}

func (c *CreatePlayer) OnTick(tick *Tick) b3.Status {
	sent := tick.Blackboard.GetMem(CreateCharacterSentBoardKey)
	if sent != nil {
		return b3.FAILURE
	}

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

	rq := &login.CreatePlayerRequest{}
	client.Send(rq, game.ClientPlayerLoginCreatePlayerMessageId)

	tick.Blackboard.SetMem(CreateCharacterSentBoardKey, true)
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

	playerList, ok := playerListI.([]*login.AccountSimplePlayerWrapper)
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
	sent := tick.Blackboard.GetMem(EnterGameSentBoardKey)
	if sent != nil {
		return b3.FAILURE
	}

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

	playerList, ok := playerListI.([]*login.AccountSimplePlayerWrapper)
	if !ok {
		return b3.FAILURE
	}

	// 处理玩家数据
	playerId := playerList[0].Player.PlayerId
	account := client.GetAccount()

	zap.L().Info("send Player enter game",
		zap.Uint64("player id", playerId), zap.String("account_name", account))

	// 发送请求
	rq := &login.EnterGameRequest{PlayerId: playerId}
	client.Send(rq, game.ClientPlayerLoginEnterGameMessageId)

	tick.Blackboard.SetMem(EnterGameSentBoardKey, true)
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
	clientI := tick.Blackboard.GetMem(PlayerListBoardKey)
	if clientI == nil {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type IsInGame struct {
	Action
}

func (a *IsInGame) Initialize(setting *BTNodeCfg) {
	a.Action.Initialize(setting)
}

func (a *IsInGame) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	clientI := tick.Blackboard.GetMem(PlayerBoardKey)
	if clientI == nil {
		return b3.FAILURE
	}

	return b3.SUCCESS
}
