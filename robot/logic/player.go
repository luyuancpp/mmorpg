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

func fnv32(key uint64) uint32 {
	hash32 := fnv.New32()
	strKey := strconv.FormatUint(key, 10)
	_, err := hash32.Write([]byte(strKey))
	if err != nil {
		return 0
	}
	return hash32.Sum32()
}

func init() {
	PlayerList = cmap.NewWithCustomShardingFunction[uint64, *Player](fnv32)
}

func NewMainPlayer(playerId uint64, client *pkg.GameClient) *Player {
	client.PlayerId = playerId
	projectConfig, result := LoadRawProjectCfg("etc/robot.b3")
	if !result {
		zap.L().Error("create player load bt tree  ", zap.Uint64("player id", playerId))
	}

	//自定义节点注册
	maps := b3.NewRegisterStructMaps()

	maps.Register("SendCreatePlayer", new(behaviortree.SendCreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.SendCreatePlayer))

	behaviorTree := make([]*BehaviorTree, len(projectConfig.Data.Trees))
	//载入
	for i, v := range projectConfig.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		behaviorTree[i] = tree
	}

	player := &Player{Client: client, BehaviorTree: behaviorTree, Blackboard: NewBlackboard()}

	PlayerList.Set(playerId, player)

	return player
}

func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}

func (p *Player) TickBehaviorTree() {
	for i, tree := range p.BehaviorTree {
		tree.Tick(i, p.Blackboard)
	}
}
