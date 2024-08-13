package logic

import (
	"client/pkg"
	"github.com/golang/protobuf/proto"
	cmap "github.com/orcaman/concurrent-map/v2"
	"hash/fnv"
	"strconv"
)

type Player struct {
	Client  *pkg.GameClient
	SceneId uint32
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
	player := &Player{Client: client}
	PlayerList.Set(playerId, player)
	return player
}

func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}
