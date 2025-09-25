package pkg

import (
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient-new/muduo"
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
)

// GameClient represents a client for interacting with the game server.
type GameClient struct {
	Client            *muduo.TcpClient
	PlayerId          uint64
	BehaviorTree      map[string]*BehaviorTree
	CurrentTree       *BehaviorTree
	Blackboard        *Blackboard
	MessageSequenceID uint64
	Account           string
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
	RegisterLoginNodes(maps)

	// Initialize behavior trees
	behaviorTree, currentTree := initializeBehaviorTrees(projectConfig, maps)

	if currentTree == nil {
		zap.L().Error("No default behavior tree found")
		return nil
	}

	blackboard := NewBlackboard()

	clientInstance := &GameClient{
		Client:            client,
		BehaviorTree:      behaviorTree,
		CurrentTree:       currentTree,
		Blackboard:        blackboard,
		MessageSequenceID: 1,
	}

	clientInstance.InitializeBehaviorTreeBlackboard()

	return clientInstance
}

// RegisterLoginNodes registers custom behavior tree nodes.
func RegisterLoginNodes(maps *b3.RegisterStructMaps) {
	maps.Register("CreatePlayer", new(behaviortree.CreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.IsRoleListEmpty))
	maps.Register("PlayerEnterGame", new(behaviortree.PlayerEnterGame))
	maps.Register("AlreadyLoggedIn", new(behaviortree.AlreadyLoggedIn))
	maps.Register("CheckTestCount", new(behaviortree.CheckTestCount))
	maps.Register("IncrementTestCount", new(behaviortree.IncrementTestCount))
	maps.Register("ResetTestCount", new(behaviortree.ResetTestCount))
	maps.Register("SetSubTree", new(behaviortree.SetSubTree))
	maps.Register("InitTree", new(behaviortree.InitTree))
	maps.Register("BoardEqualConst", new(behaviortree.BoardEqualConst))
	maps.Register("PlayerEnterGame", new(behaviortree.PlayerEnterGame))
	maps.Register("AlreadyLoggedIn", new(behaviortree.AlreadyLoggedIn))
	maps.Register("RandomEnterScene", new(behaviortree.RandomEnterScene))
	maps.Register("CheckTestCount", new(behaviortree.CheckTestCount))
	maps.Register("IncrementTestCount", new(behaviortree.IncrementTestCount))
	maps.Register("ResetTestCount", new(behaviortree.ResetTestCount))
	maps.Register("SetSubTree", new(behaviortree.SetSubTree))
	maps.Register("InitTree", new(behaviortree.InitTree))
	maps.Register("BoardEqualConst", new(behaviortree.BoardEqualConst))
	maps.Register("GetHatredTarget", new(behaviortree.GetHatredTarget))
	maps.Register("SetBoardTargetPos", new(behaviortree.SetBoardTargetPos))
	maps.Register("MoveToEntity", new(behaviortree.MoveToEntity))
	maps.Register("GetTargetDistance", new(behaviortree.GetTargetDistance))
	maps.Register("GetReleasableSkillList", new(behaviortree.GetReleasableSkillList))
	maps.Register("CheckSkillByDistance", new(behaviortree.CheckSkillByDistance))
	maps.Register("GetSkillID", new(behaviortree.GetSkillID))
	maps.Register("ReleaseSkill", new(behaviortree.ReleaseSkill))
	maps.Register("SleepNode", new(behaviortree.SleepNode))
	maps.Register("Login", new(behaviortree.Login))
	maps.Register("IsInGame", new(behaviortree.IsInGame))
}

// initializeBehaviorTrees initializes behavior trees from configuration.
func initializeBehaviorTrees(config *RawProjectCfg, maps *b3.RegisterStructMaps) (map[string]*BehaviorTree, *BehaviorTree) {
	behaviorTree := make(map[string]*BehaviorTree)
	var currentTree *BehaviorTree

	for i, v := range config.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		if tree == nil {
			zap.L().Error("Failed to create behavior tree", zap.Int("index", i))
			continue
		}

		if tree.GetTitile() == behaviortree.LoginSubTree {
			currentTree = tree
		}
		behaviorTree[tree.GetTitile()] = tree
	}

	return behaviorTree, currentTree
}

// InitializeBehaviorTreeBlackboard initializes or resets the behavior tree's blackboard.
func (client *GameClient) InitializeBehaviorTreeBlackboard() {
	client.Blackboard = NewBlackboard()
	client.Blackboard.SetMem(behaviortree.ClientBoardKey, client)
	client.Blackboard.SetMem(behaviortree.ActorListBoardKey, gameobject.NewActorList())
	client.Blackboard.SetMem(behaviortree.SkillListBoardKey, &game.PlayerSkillListPBComponent{})
	zap.L().Info("Behavior tree blackboard initialized")
}

// Send sends a message to the server.
func (client *GameClient) Send(message proto.Message, messageId uint32) {
	rq := &game.ClientRequest{
		Id:        client.MessageSequenceID,
		MessageId: messageId,
	}
	client.MessageSequenceID++

	var err error
	rq.Body, err = proto.Marshal(message)
	if err != nil {
		zap.L().Error("Failed to marshal message", zap.Error(err))
		return
	}

	client.Client.Send(rq)
}

// Close closes the client connection.
func (client *GameClient) Close() {
	client.Client.Close()
	zap.L().Info("Client closed successfully")
}

// TickBehaviorTree updates the state of the current behavior tree.
func (client *GameClient) TickBehaviorTree() {
	if client.CurrentTree != nil {
		client.CurrentTree.Tick(0, client.Blackboard)
	} else {
		zap.L().Warn("No behavior tree to tick")
	}
}

// SetPlayerId sets the player ID for the client.
func (client *GameClient) SetPlayerId(playerId uint64) {
	client.PlayerId = playerId
}

func (client *GameClient) GetPlayerId() uint64 {
	return client.PlayerId
}

func (client *GameClient) GetAccount() string {
	return client.Account
}

// SetSubTree sets the current behavior tree to the specified subtree.
func (client *GameClient) SetSubTree(treeTitle string) {
	tree, ok := client.BehaviorTree[treeTitle]
	if ok {
		client.CurrentTree = tree
	} else {
		zap.L().Warn("Behavior tree not found", zap.String("title", treeTitle))
	}
}
