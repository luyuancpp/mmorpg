package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
)

// KeyExtractor 从PB中提取key的函数（支持uint64类型key）
type KeyExtractor func(proto.Message) (uint64, error)

// DefaultPlayerIdExtractor 默认提取player_id（适配proto中全小写字段）
func DefaultPlayerIdExtractor(msg proto.Message) (uint64, error) {
	return extractPlayerIdRecursive(msg.ProtoReflect())
}

// 递归提取player_id（支持嵌套消息）
func extractPlayerIdRecursive(msgReflect protoreflect.Message) (uint64, error) {
	// 检查当前消息是否有player_id字段
	field := msgReflect.Descriptor().Fields().ByName(protoreflect.Name("player_id"))
	if field != nil {
		val := msgReflect.Get(field)
		switch field.Kind() {
		case protoreflect.Uint64Kind:
			return val.Uint(), nil
		case protoreflect.Int64Kind:
			return uint64(val.Int()), nil
		default:
			return 0, fmt.Errorf("player_id字段类型不支持（%s）", field.Kind())
		}
	}

	// 递归检查子消息
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

	return 0, fmt.Errorf("PB类型 %s 及其子消息均缺少player_id字段", msgReflect.Descriptor().FullName())
}

// Load 批量加载PB数据（所有消息及子消息都命中缓存时才执行回调）
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
		return errors.New("keyExtractor不能为空")
	}

	taskKey := uuid.NewString()
	allCached := true // 标记是否所有消息及子消息都命中缓存

	for _, msg := range messages {
		// 提取当前消息的key
		key, err := keyExtractor(msg)
		if err != nil {
			return fmt.Errorf("提取key失败: %w", err)
		}

		// 构建父消息缓存键
		parentKey := buildParentKey(msg, key)

		// 1. 检查父消息缓存
		parentFound, err := LoadProtoFromRedis(ctx, redisClient, parentKey, proto.Clone(msg))
		if err != nil {
			return fmt.Errorf("查询父消息Redis缓存失败: %w", err)
		}
		if !parentFound {
			allCached = false // 父消息未命中，标记非全缓存
		} else {
			continue
		}

		// 3. 未全命中缓存，执行实际加载逻辑（与原逻辑一致）
		allSubMsgs := collectSubMessages(msg)
		subMsgCount := len(allSubMsgs)

		var aggregator taskmanager.Aggregator
		if subMsgCount > 1 {
			var aggErr error
			aggregator, aggErr = taskmanager.NewGenericAggregator(proto.Clone(msg), parentKey)
			if aggErr != nil {
				return fmt.Errorf("创建聚合器失败: %w", aggErr)
			}
			logx.Debugf("子消息数量=%d，创建聚合器（parentKey=%s）", subMsgCount, parentKey)
		}

		var processedSubMsgs []proto.Message
		for _, subMsg := range allSubMsgs {
			if err := setKeyToMessage(subMsg, key, keyExtractor); err != nil {
				return fmt.Errorf("设置子消息key失败 (type=%s): %w", subMsg.ProtoReflect().Descriptor().FullName(), err)
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

	// 4. 所有消息处理完成后，若全命中缓存则执行回调
	if allCached && callback != nil {
		callback(taskKey, true, nil)
		return nil
	}

	// 非全缓存时，提交任务执行加载
	return executor.SubmitTask(taskKey)
}

// LoadWithPlayerId 简化版：默认用player_id作为key
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

// 收集子消息：只收集顶层消息中显式声明且已初始化的一级子消息
func collectSubMessages(msg proto.Message) []proto.Message {
	msgReflect := msg.ProtoReflect()
	fields := msgReflect.Descriptor().Fields()
	var subMsgs []proto.Message

	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)

		// 跳过列表类型
		if field.IsList() {
			continue
		}

		// 只处理单个消息类型字段
		if field.Kind() != protoreflect.MessageKind {
			continue
		}

		// 关键：判断子消息是否已初始化（非nil）
		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil || subMsgReflect.IsValid() == false {
			continue // 未初始化的子消息字段，不收集
		}

		subMsg := subMsgReflect.Interface()
		subMsgs = append(subMsgs, subMsg)
	}

	// 有已初始化的子消息则返回，否则返回自身
	if len(subMsgs) > 0 {
		return subMsgs
	}
	return []proto.Message{msg}
}

// 为消息设置key（仅支持player_id字段）
func setKeyToMessage(msg proto.Message, key uint64, keyExtractor KeyExtractor) error {
	tempMsg := proto.Clone(msg).(proto.Message)
	tempReflect := tempMsg.ProtoReflect()
	msgName := msg.ProtoReflect().Descriptor().FullName()

	// 仅尝试player_id字段
	field := tempReflect.Descriptor().Fields().ByName(protoreflect.Name("player_id"))
	if field == nil {
		return fmt.Errorf("消息 %s 不包含player_id字段", msgName)
	}

	// 按字段类型设置值
	var value protoreflect.Value
	switch field.Kind() {
	case protoreflect.Uint64Kind:
		value = protoreflect.ValueOfUint64(key)
	case protoreflect.Int64Kind:
		value = protoreflect.ValueOfInt64(int64(key))
	default:
		return fmt.Errorf("消息 %s 的player_id类型不支持（%s）", msgName, field.Kind())
	}

	// 验证设置是否正确
	tempReflect.Set(field, value)
	extractedKey, err := keyExtractor(tempMsg)
	if err != nil || extractedKey != key {
		return fmt.Errorf("消息 %s 设置player_id失败（提取值不匹配）", msgName)
	}

	// 正式设置key
	msg.ProtoReflect().Set(field, value)
	return nil
}

// 生成父消息缓存键
func buildParentKey(msg proto.Message, key uint64) string {
	return fmt.Sprintf("%s:%d", msg.ProtoReflect().Descriptor().FullName(), key)
}

// 从Redis加载PB
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
