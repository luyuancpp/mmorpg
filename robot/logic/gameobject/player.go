package gameobject

import (
	"github.com/golang/protobuf/proto"
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	cmap "github.com/orcaman/concurrent-map/v2"
	"go.uber.org/zap"
	"hash/fnv"
	"robot/interfaces"
	"robot/logic/behaviortree"
	"strconv"
)

type Player struct {
	Client       interfaces.GameClientInterface
	BehaviorTree map[string]*BehaviorTree
	CurrentTree  *BehaviorTree
	Blackboard   *Blackboard
	SceneID      uint32
}

var PlayerList cmap.ConcurrentMap[uint64, *Player]

// fnv32 计算 64 位整数的 FNV-1a 哈希值，并返回 32 位无符号整数
func fnv32(key uint64) uint32 {
	hash32 := fnv.New32()
	strKey := strconv.FormatUint(key, 10)
	_, err := hash32.Write([]byte(strKey))
	if err != nil {
		zap.L().Error("Error generating hash", zap.Error(err))
		return 0
	}

	return hash32.Sum32()
}

func init() {
	PlayerList = cmap.NewWithCustomShardingFunction[uint64, *Player](fnv32)
}

// RegisterLoginNodes registers custom behavior tree nodes.
func RegisterGameNodes(maps *b3.RegisterStructMaps) {
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

// NewMainPlayer 创建一个新的玩家实例，初始化其行为树和黑板
func NewMainPlayer(playerId uint64, clientI interface{}) *Player {
	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		return nil
	}
	client.SetPlayerId(playerId)

	// Load behavior tree configuration file
	projectConfig, result := LoadRawProjectCfg("etc/robot.b3")
	if !result {
		zap.L().Error("Failed to load behavior tree configuration", zap.String("path", "etc/robot.b3"))
		return nil
	}

	// Register custom behavior tree nodes
	maps := b3.NewRegisterStructMaps()
	RegisterGameNodes(maps)

	// Initialize behavior trees
	behaviorTree, currentTree := initializeBehaviorTrees(projectConfig, maps)

	if currentTree == nil {
		zap.L().Error("No default behavior tree found")
		return nil
	}

	blackboard := NewBlackboard()

	player := &Player{Client: client,
		BehaviorTree: behaviorTree,
		CurrentTree:  currentTree,
		Blackboard:   blackboard}

	PlayerList.Set(playerId, player)
	zap.L().Info("Player created successfully", zap.Uint64("player id", playerId))
	return player
}

// Send 向服务器发送消息
func (player *Player) Send(message proto.Message, messageId uint32) {
	player.Client.Send(message, messageId)
}

func (player *Player) GetClient() interfaces.GameClientInterface {
	return player.Client
}
