package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
)

// LoadCallback 加载完成回调函数（与 taskmanager.BatchCallback 类型对齐）

// KeyExtractor 从PB中提取key的函数（支持任意key类型）
type KeyExtractor func(proto.Message) (uint64, error)

// 默认的PlayerId提取函数（适配proto中的 player_id 字段）
func DefaultPlayerIdExtractor(msg proto.Message) (uint64, error) {
	return extractPlayerIdRecursive(msg.ProtoReflect())
}

// 递归提取：先检查当前消息，若没有则遍历子消息
func extractPlayerIdRecursive(msgReflect protoreflect.Message) (uint64, error) {
	// 1. 先检查当前消息是否有 player_id 字段
	field := msgReflect.Descriptor().Fields().ByName(protoreflect.Name("player_id"))
	if field != nil {
		// 当前消息有 player_id，直接提取
		val := msgReflect.Get(field)
		return uint64(val.Uint()), nil
	}

	// 2. 当前消息没有，遍历所有子消息字段递归查找
	fields := msgReflect.Descriptor().Fields()
	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)
		// 只处理子消息类型的字段
		if field.Kind() != protoreflect.MessageKind {
			continue
		}

		// 获取子消息实例
		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil {
			continue // 子消息未初始化，跳过
		}

		// 递归提取子消息的 player_id
		id, err := extractPlayerIdRecursive(subMsgReflect)
		if err == nil {
			return id, nil // 找到则返回
		}
		// 子消息也没有，继续遍历其他子字段
	}

	// 3. 所有层级都没有找到
	return 0, fmt.Errorf("PB类型 %s 及其子消息均缺少 player_id 字段", msgReflect.Descriptor().FullName())
}

// Load 批量加载PB数据：支持动态类型查找和自定义key提取
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
	for _, msg := range messages {
		if err := loadSingle(ctx, redisClient, kafkaProducer, executor, msg, keyExtractor, callback); err != nil {
			return err
		}
	}
	return nil
}

// LoadWithPlayerId 简化版：默认使用PlayerId作为key
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

// 处理单个PB加载
func loadSingle(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	executor *taskmanager.TaskExecutor,
	msg proto.Message,
	keyExtractor KeyExtractor,
	callback taskmanager.BatchCallback,
) error {
	// 1. 提取key
	key, err := keyExtractor(msg)
	if err != nil {
		return fmt.Errorf("提取key失败: %w", err)
	}

	// 2. 判断是否为聚合消息（获取子消息类型全名）
	isAggregate, subTypeNames := isAggregateMessage(msg)
	if !isAggregate {
		return batchLoadAndCache(ctx, redisClient, kafkaProducer, key, []proto.Message{msg}, executor, callback)
	}

	// 3. 聚合消息处理：通过protoregistry动态创建子消息
	parentKey := buildParentKey(msg, key)
	aggregator, err := taskmanager.NewGenericAggregator(proto.Clone(msg), parentKey)
	if err != nil {
		return fmt.Errorf("创建聚合器失败: %w", err)
	}

	// 4. 动态加载子消息类型（使用protoregistry.GlobalTypes）
	subMsgs, err := buildSubMessagesByTypeNames(subTypeNames, key, keyExtractor)
	if err != nil {
		return fmt.Errorf("构建子消息失败: %w", err)
	}

	// 5. 检查缓存并加载
	found, err := LoadProtoFromRedis(ctx, redisClient, parentKey, msg)
	if err != nil {
		return fmt.Errorf("查询Redis缓存失败: %w", err)
	}
	if found {
		if callback != nil {
			callback(parentKey, true, nil)
		}
		return nil
	}

	// 6. 提交聚合任务
	taskKey := taskmanager.GenerateTaskKey(msg, key)
	return taskmanager.InitAndAddMessageTasks(
		ctx,
		executor,
		taskKey,
		redisClient,
		kafkaProducer,
		key,
		subMsgs,
		taskmanager.InitTaskOptions{
			Aggregator: aggregator,
			Callback:   callback,
		},
	)
}

// 判断是否为聚合消息（返回子消息的类型全名）
func isAggregateMessage(msg proto.Message) (bool, []protoreflect.FullName) {
	msgReflect := msg.ProtoReflect()
	fields := msgReflect.Descriptor().Fields()
	var subTypeNames []protoreflect.FullName

	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)
		if field.Kind() == protoreflect.MessageKind {
			// 获取子消息的完整类型名（如 "login_proto_database.PlayerDatabase"）
			subTypeNames = append(subTypeNames, field.Message().FullName())
		}
	}
	return len(subTypeNames) > 0, subTypeNames
}

// 通过类型全名构建子消息（使用protoregistry.GlobalTypes）
func buildSubMessagesByTypeNames(
	subTypeNames []protoreflect.FullName,
	key uint64,
	keyExtractor KeyExtractor,
) ([]proto.Message, error) {
	var subMsgs []proto.Message
	for _, typeName := range subTypeNames {
		// 1. 查找消息类型（与你提供的用法一致）
		msgType, err := protoregistry.GlobalTypes.FindMessageByName(typeName)
		if err != nil {
			return nil, fmt.Errorf("查找子消息类型失败 (name=%s): %w", typeName, err)
		}

		// 2. 创建子消息实例
		subMsg := msgType.New().Interface()

		// 3. 设置子消息的key
		if err := setKeyToMessage(subMsg, key, keyExtractor); err != nil {
			return nil, fmt.Errorf("设置子消息key失败 (type=%s): %w", typeName, err)
		}

		subMsgs = append(subMsgs, subMsg)
	}
	return subMsgs, nil
}

// 给消息设置key
// 给消息设置key（仅尝试 player_id 字段）
func setKeyToMessage(msg proto.Message, key uint64, keyExtractor KeyExtractor) error {
	tempMsg := proto.Clone(msg).(proto.Message)
	tempReflect := tempMsg.ProtoReflect()
	msgName := msg.ProtoReflect().Descriptor().FullName()

	// 只尝试 player_id 字段，不处理其他字段
	fieldName := protoreflect.Name("player_id")
	field := tempReflect.Descriptor().Fields().ByName(fieldName)
	if field == nil {
		return fmt.Errorf("消息 %s 不包含 player_id 字段（不支持其他字段）", msgName)
	}

	// 仅支持 uint64/int64 类型（与你的 proto 定义一致）
	var value protoreflect.Value
	switch field.Kind() {
	case protoreflect.Uint64Kind:
		value = protoreflect.ValueOfUint64(key)
	case protoreflect.Int64Kind:
		value = protoreflect.ValueOfInt64(int64(key))
	default:
		return fmt.Errorf("消息 %s 的 player_id 字段类型不支持（仅支持 uint64/int64）", msgName)
	}

	// 设置临时key并验证
	tempReflect.Set(field, value)
	extractedKey, err := keyExtractor(tempMsg)
	if err != nil {
		return fmt.Errorf("消息 %s 设置 player_id 后提取失败: %w", msgName, err)
	}
	if extractedKey != key {
		return fmt.Errorf("消息 %s 提取的 player_id 不匹配（期望 %d，实际 %d）", msgName, key, extractedKey)
	}

	// 正式设置key
	msg.ProtoReflect().Set(field, value)
	logx.Infof("消息 %s 成功设置 player_id: %d", msgName, key)
	return nil
}

// 生成父消息缓存键
func buildParentKey(msg proto.Message, key uint64) string {
	return fmt.Sprintf("%s:%d", msg.ProtoReflect().Descriptor().FullName(), key)
}

// 普通消息批量加载
func batchLoadAndCache(
	ctx context.Context,
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	key uint64,
	messages []proto.Message,
	executor *taskmanager.TaskExecutor,
	callback taskmanager.BatchCallback,
) error {
	taskKey := taskmanager.GenerateBatchTaskKey(messages, key)
	return taskmanager.InitAndAddMessageTasks(
		ctx,
		executor,
		taskKey,
		redisClient,
		kafkaProducer,
		key,
		messages,
		taskmanager.InitTaskOptions{
			Callback: callback,
		},
	)
}

// 从Redis加载单个PB
func LoadProtoFromRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message) (bool, error) {
	val, err := redisClient.Get(ctx, key).Bytes()
	if errors.Is(err, redis.Nil) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	if err := proto.Unmarshal(val, msg); err != nil {
		return false, err
	}
	return true, nil
}
