package pkg

import (
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/pb/game"
)

// GameClient represents a client for interacting with the game server.
type GameClient struct {
	Client            *muduo.Client
	PlayerId          uint64
	BehaviorTree      []*BehaviorTree
	Blackboard        *Blackboard
	MessageSequenceID uint64
}

// NewGameClient creates and initializes a new GameClient instance.
func NewGameClient(client *muduo.Client) *GameClient {
	// Load behavior tree configuration file
	projectConfig, result := LoadRawProjectCfg("etc/robot.b3")
	if !result {
		zap.L().Error("Failed to load behavior tree configuration", zap.String("path", "etc/robot.b3"))
		return nil
	}

	// Register custom behavior tree nodes
	maps := b3.NewRegisterStructMaps()
	maps.Register("CreatePlayer", new(behaviortree.CreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.IsRoleListEmpty))
	maps.Register("PlayerEnterGame", new(behaviortree.PlayerEnterGame))
	maps.Register("AlreadyLoggedIn", new(behaviortree.AlreadyLoggedIn))
	maps.Register("RandomEnterScene", new(behaviortree.RandomEnterScene))
	maps.Register("CheckSceneSwitchCount", new(behaviortree.CheckSceneSwitchCount))
	maps.Register("AddSceneSwitchCount", new(behaviortree.AddSceneSwitchCount))

	// Initialize behavior trees
	behaviorTree := make([]*BehaviorTree, len(projectConfig.Data.Trees))
	for i, v := range projectConfig.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		if tree == nil {
			zap.L().Error("Failed to create behavior tree", zap.Int("index", i))
			return nil
		}
		behaviorTree[i] = tree
	}

	blackboard := NewBlackboard()

	c := &GameClient{
		Client:            client,
		BehaviorTree:      behaviorTree,
		Blackboard:        blackboard,
		MessageSequenceID: 1,
	}

	// Set the client instance in the blackboard
	c.Blackboard.SetMem(behaviortree.ClientIdentifier, c)

	return c
}

// Send sends a message to the server.
func (gameClient *GameClient) Send(message proto.Message, messageId uint32) {
	rq := &game.ClientRequest{Id: gameClient.MessageSequenceID, MessageId: messageId}

	gameClient.MessageSequenceID++

	var err error
	rq.Body, err = proto.Marshal(message)

	if err != nil {
		zap.L().Error("Failed to marshal message", zap.Error(err))
		return
	}

	gameClient.Client.Send(rq)
}

// Close closes the client connection.
func (gameClient *GameClient) Close() {
	if err := gameClient.Client.Close(); err != nil {
		zap.L().Error("Failed to close client", zap.Error(err))
	} else {
		zap.L().Info("Client closed successfully")
	}
}

// TickBehaviorTree updates the state of all behavior trees.
func (gameClient *GameClient) TickBehaviorTree() {
	for i, tree := range gameClient.BehaviorTree {
		tree.Tick(i, gameClient.Blackboard)
	}
}

func (gameClient *GameClient) SetPlayerId(playerId uint64) {
	gameClient.PlayerId = playerId
}

func (gameClient *GameClient) GetPlayerId() uint64 {
	return gameClient.PlayerId
}
