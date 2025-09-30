package taskmanager

import (
	"fmt"
	"google.golang.org/protobuf/proto"
	"strings"
)

// GenerateTaskKey constructs a taskKey using proto message type and playerID.
// Suitable for uniquely identifying a task batch for a player.
func GenerateTaskKey(msg proto.Message, playerID uint64) string {
	msgType := msg.ProtoReflect().Descriptor().FullName()
	return fmt.Sprintf("%s:%d", msgType, playerID)
}

// GenerateBatchTaskKey builds a taskKey from the first proto message in a batch.
// Ensures all tasks in a batch use the same key, assuming same message type.
func GenerateBatchTaskKey(messages []proto.Message, playerID uint64) string {
	if len(messages) == 0 {
		return fmt.Sprintf("empty:%d", playerID)
	}

	var keyParts []string
	for _, msg := range messages {
		fullName := string(msg.ProtoReflect().Descriptor().FullName())
		keyParts = append(keyParts, fullName)
	}

	// Join all full names with "+"
	joined := strings.Join(keyParts, "+")
	return fmt.Sprintf("%s:%d", joined, playerID)
}
