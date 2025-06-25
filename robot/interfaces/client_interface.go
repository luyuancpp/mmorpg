// interfaces/client.go
package interfaces

import "github.com/golang/protobuf/proto"

type GameClientInterface interface {
	Send(message proto.Message, messageId uint32)
	Close()
	SetPlayerId(playerId uint64)
	SetSubTree(treeTitle string)
	GetAccount() string
}
