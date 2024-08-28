package logic

import (
	"client/logic/behaviortree"
	"client/pkg"
	"github.com/golang/protobuf/proto"
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	cmap "github.com/orcaman/concurrent-map/v2"
	"go.uber.org/zap"
	"hash/fnv"
	"strconv"
)

type Player struct {
	Client       *pkg.GameClient
	SceneId      uint32
	BehaviorTree []*BehaviorTree
	Blackboard   *Blackboard
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

// NewMainPlayer 创建一个新的玩家实例，初始化其行为树和黑板
func NewMainPlayer(playerId uint64, client *pkg.GameClient) *Player {
	client.PlayerId = playerId

	// 加载行为树配置文件
	projectConfig, result := LoadRawProjectCfg("etc/robot.b3")
	if !result {
		zap.L().Error("Failed to load behavior tree configuration", zap.Uint64("player id", playerId))
		return nil
	}

	// 自定义节点注册
	maps := b3.NewRegisterStructMaps()
	maps.Register("SendCreatePlayer", new(behaviortree.SendCreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.IsRoleListEmpty)) // 确保你的行为树中需要该节点

	// 初始化行为树
	behaviorTree := make([]*BehaviorTree, len(projectConfig.Data.Trees))
	for i, v := range projectConfig.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		behaviorTree[i] = tree
	}

	// 创建并返回玩家实例
	player := &Player{
		Client:       client,
		BehaviorTree: behaviorTree,
		Blackboard:   NewBlackboard(),
	}

	PlayerList.Set(playerId, player)
	zap.L().Info("Player created successfully", zap.Uint64("player id", playerId))
	return player
}

// Send 向服务器发送消息
func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}

// TickBehaviorTree 更新所有行为树的状态
func (p *Player) TickBehaviorTree() {
	for i, tree := range p.BehaviorTree {
		tree.Tick(i, p.Blackboard)
	}
}
