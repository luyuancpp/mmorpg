package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
)

// LoadCallback 加载完成回调函数（与 taskmanager.BatchCallback 类型对齐）

// KeyExtractor 从PB中提取key的函数（支持任意key类型）
type KeyExtractor func(proto.Message) (uint64, error)

// 默认的PlayerId提取函数
func DefaultPlayerIdExtractor(msg proto.Message) (uint64, error) {
	msgReflect := msg.ProtoReflect()
	field := msgReflect.Descriptor().Fields().ByName(protoreflect.Name("PlayerId"))
	if field == nil {
		return 0, fmt.Errorf("PB类型 %s 缺少PlayerId字段", msgReflect.Descriptor().FullName())
	}
	return uint64(msgReflect.Get(field).Uint()), nil
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
func setKeyToMessage(msg proto.Message, key uint64, keyExtractor KeyExtractor) error {
	// 临时设置key并验证提取函数是否能正确提取
	tempMsg := proto.Clone(msg).(proto.Message)
	tempReflect := tempMsg.ProtoReflect()

	// 尝试常见的key字段
	for _, fieldName := range []protoreflect.Name{"PlayerId", "RoleId", "UserId", "Id"} {
		field := tempReflect.Descriptor().Fields().ByName(fieldName)
		if field == nil {
			continue
		}
		// 设置临时key值
		tempReflect.Set(field, protoreflect.ValueOfUint64(key))
		// 验证提取函数是否能正确提取
		if extractedKey, err := keyExtractor(tempMsg); err == nil && extractedKey == key {
			// 验证通过，正式设置key
			msg.ProtoReflect().Set(field, protoreflect.ValueOfUint64(key))
			return nil
		}
	}

	return fmt.Errorf("消息 %s 不支持自动设置key（未找到匹配字段）", msg.ProtoReflect().Descriptor().FullName())
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
