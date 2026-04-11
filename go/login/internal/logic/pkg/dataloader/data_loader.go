package dataloader

import (
	"context"
	"errors"
	"fmt"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// collectSubMessages collects top-level initialized sub-message fields from a message.
func collectSubMessages(msg proto.Message) []proto.Message {
	msgReflect := msg.ProtoReflect()
	fields := msgReflect.Descriptor().Fields()
	var subMsgs []proto.Message

	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)

		if field.IsList() {
			continue
		}
		if field.Kind() != protoreflect.MessageKind {
			continue
		}

		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil || !subMsgReflect.IsValid() {
			continue
		}

		subMsgs = append(subMsgs, subMsgReflect.Interface())
	}

	if len(subMsgs) > 0 {
		return subMsgs
	}
	return []proto.Message{msg}
}

// buildParentKey generates the cache key for a parent message.
func buildParentKey(msg proto.Message, key uint64) string {
	return fmt.Sprintf("%s:%d", msg.ProtoReflect().Descriptor().FullName(), key)
}

// LoadProtoFromRedis loads a protobuf message from Redis.
func LoadProtoFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if errors.Is(err, redis.Nil) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	return true, proto.Unmarshal(val, msg)
}
