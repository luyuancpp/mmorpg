package cache

import (
	"fmt"

	"google.golang.org/protobuf/proto"
)

// BuildRedisKey generates a Redis key from the message type and playerID.
func BuildRedisKey(msg proto.Message, playerID string) string {
	msgName := string(msg.ProtoReflect().Descriptor().FullName())
	return fmt.Sprintf("%s:%s", msgName, playerID)
}
