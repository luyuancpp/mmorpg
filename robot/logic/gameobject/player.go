package gameobject

import (
	"github.com/golang/protobuf/proto"
	. "github.com/magicsea/behavior3go/core"
	cmap "github.com/orcaman/concurrent-map/v2"
	"go.uber.org/zap"
	"hash/fnv"
	"robot/interfaces"
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

// NewMainPlayer 创建一个新的玩家实例，初始化其行为树和黑板
func NewMainPlayer(playerId uint64, clientI interface{}) *Player {
	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		return nil
	}
	client.SetPlayerId(playerId)

	player := &Player{Client: client,
		Blackboard: NewBlackboard()}

	PlayerList.Set(playerId, player)
	return player
}

// Send 向服务器发送消息
func (player *Player) Send(message proto.Message, messageId uint32) {
	player.Client.Send(message, messageId)
}

func (player *Player) GetClient() interfaces.GameClientInterface {
	return player.Client
}

func (player *Player) TickBehaviorTree() {
	if player.CurrentTree != nil {
		player.CurrentTree.Tick(0, player.Blackboard)
	} else {
		zap.L().Warn("No behavior tree to tick")
	}
}
