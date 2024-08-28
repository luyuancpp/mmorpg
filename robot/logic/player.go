package logic

import (
	"client/pkg"
	"github.com/golang/protobuf/proto"
	cmap "github.com/orcaman/concurrent-map/v2"
	"go.uber.org/zap"
	"hash/fnv"
	"strconv"
)

type Player struct {
	Client  *pkg.GameClient
	SceneId uint32
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

	player := &Player{Client: client}

	PlayerList.Set(playerId, player)
	zap.L().Info("Player created successfully", zap.Uint64("player id", playerId))
	return player
}

// Send 向服务器发送消息
func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}
