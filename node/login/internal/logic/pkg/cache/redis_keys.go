package cache

import (
	"fmt"
	"google.golang.org/protobuf/proto"
)

// 示例 BuildRedisKey，按你的 message 类型 + playerId 来生成 Redis key
func BuildRedisKey(msg proto.Message, playerID string) string {
	msgName := string(msg.ProtoReflect().Descriptor().FullName())
	return fmt.Sprintf("player:%s:%s", playerID, msgName)
}
