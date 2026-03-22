package dataloader

import (
	"context"
	"errors"
	"fmt"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// KeyExtractor extracts a uint64 key from a protobuf message.
type KeyExtractor func(proto.Message) (uint64, error)

// DefaultPlayerIdExtractor extracts player_id from a protobuf message.
func DefaultPlayerIdExtractor(msg proto.Message) (uint64, error) {
	return extractPlayerIdRecursive(msg.ProtoReflect())
}

// extractPlayerIdRecursive recursively searches for a player_id field.
func extractPlayerIdRecursive(msgReflect protoreflect.Message) (uint64, error) {
	// Check if this message has a player_id field
	field := msgReflect.Descriptor().Fields().ByName(protoreflect.Name("player_id"))
	if field != nil {
		val := msgReflect.Get(field)
		switch field.Kind() {
		case protoreflect.Uint64Kind:
			return val.Uint(), nil
		case protoreflect.Int64Kind:
			return uint64(val.Int()), nil
		default:
			return 0, fmt.Errorf("unsupported player_id field type (%s)", field.Kind())
		}
	}

	// Recursively check sub-messages
	fields := msgReflect.Descriptor().Fields()
	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)
		if field.Kind() != protoreflect.MessageKind {
			continue
		}
		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil {
			continue
		}
		if id, err := extractPlayerIdRecursive(subMsgReflect); err == nil {
			return id, nil
		}
	}

	return 0, fmt.Errorf("PB type %s and its sub-messages lack a player_id field", msgReflect.Descriptor().FullName())
}

// Load batch-loads PB data; executes callback only when all messages (and sub-messages) hit cache.
func Load(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	executor *taskmanager.TaskExecutor,
	messages []proto.Message,
	keyExtractor KeyExtractor,
	callback taskmanager.BatchCallback,
) error {
	if keyExtractor == nil {
		return errors.New("keyExtractor must not be nil")
	}

	taskKey := uuid.NewString()
	allCached := true

	for _, msg := range messages {
		key, err := keyExtractor(msg)
		if err != nil {
			return fmt.Errorf("failed to extract key: %w", err)
		}

		parentKey := buildParentKey(msg, key)

		parentFound, err := LoadProtoFromRedis(ctx, redisClient, parentKey, proto.Clone(msg))
		if err != nil {
			return fmt.Errorf("failed to query parent message Redis cache: %w", err)
		}
		if !parentFound {
			allCached = false
		} else {
			continue
		}

		allSubMsgs := collectSubMessages(msg)
		subMsgCount := len(allSubMsgs)

		var aggregator taskmanager.Aggregator
		if subMsgCount > 1 {
			var aggErr error
			aggregator, aggErr = taskmanager.NewGenericAggregator(proto.Clone(msg), parentKey)
			if aggErr != nil {
				return fmt.Errorf("failed to create aggregator: %w", aggErr)
			}
			logx.Debugf("subMsgCount=%d, created aggregator (parentKey=%s)", subMsgCount, parentKey)
		}

		var processedSubMsgs []proto.Message
		for _, subMsg := range allSubMsgs {
			if err := setKeyToMessage(subMsg, key, keyExtractor); err != nil {
				return fmt.Errorf("failed to set sub-message key (type=%s): %w", subMsg.ProtoReflect().Descriptor().FullName(), err)
			}
			processedSubMsgs = append(processedSubMsgs, subMsg)
		}

		if err := taskmanager.InitAndAddMessageTasks(
			ctx,
			executor,
			taskKey,
			redisClient,
			kafkaProducer,
			key,
			processedSubMsgs,
			taskmanager.InitTaskOptions{
				Aggregator: aggregator,
				Callback:   callback,
			},
		); err != nil {
			return err
		}
	}

	// All messages cached; invoke callback directly
	if allCached && callback != nil {
		callback(taskKey, true, nil)
		return nil
	}

	// Not fully cached; submit load tasks
	return executor.SubmitTask(taskKey)
}

// LoadWithPlayerId is a convenience wrapper that uses player_id as the key.
func LoadWithPlayerId(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	executor *taskmanager.TaskExecutor,
	messages []proto.Message,
	callback taskmanager.BatchCallback,
) error {
	return Load(ctx, redisClient, kafkaProducer, executor, messages, DefaultPlayerIdExtractor, callback)
}

// collectSubMessages collects top-level initialized sub-message fields from a message.
func collectSubMessages(msg proto.Message) []proto.Message {
	msgReflect := msg.ProtoReflect()
	fields := msgReflect.Descriptor().Fields()
	var subMsgs []proto.Message

	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)

		// Skip list fields
		if field.IsList() {
			continue
		}

		// Only process message-type fields
		if field.Kind() != protoreflect.MessageKind {
			continue
		}

		// Only collect initialized (non-nil) sub-messages
		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil || subMsgReflect.IsValid() == false {
			continue
		}

		subMsg := subMsgReflect.Interface()
		subMsgs = append(subMsgs, subMsg)
	}

	// Return initialized sub-messages if any, otherwise return the message itself
	if len(subMsgs) > 0 {
		return subMsgs
	}
	return []proto.Message{msg}
}

// setKeyToMessage sets the player_id field on a message.
func setKeyToMessage(msg proto.Message, key uint64, keyExtractor KeyExtractor) error {
	tempMsg := proto.Clone(msg).(proto.Message)
	tempReflect := tempMsg.ProtoReflect()
	msgName := msg.ProtoReflect().Descriptor().FullName()

	// Only try player_id field
	field := tempReflect.Descriptor().Fields().ByName(protoreflect.Name("player_id"))
	if field == nil {
		return fmt.Errorf("message %s does not contain a player_id field", msgName)
	}

	// Set value by field type
	var value protoreflect.Value
	switch field.Kind() {
	case protoreflect.Uint64Kind:
		value = protoreflect.ValueOfUint64(key)
	case protoreflect.Int64Kind:
		value = protoreflect.ValueOfInt64(int64(key))
	default:
		return fmt.Errorf("message %s has unsupported player_id type (%s)", msgName, field.Kind())
	}

	// Verify the value was set correctly
	tempReflect.Set(field, value)
	extractedKey, err := keyExtractor(tempMsg)
	if err != nil || extractedKey != key {
		return fmt.Errorf("message %s failed to set player_id (extracted value mismatch)", msgName)
	}

	// Apply to the actual message
	msg.ProtoReflect().Set(field, value)
	return nil
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
