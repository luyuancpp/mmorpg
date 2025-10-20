package dataloader

import (
	"context"
	"errors"
	"fmt"
	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
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

// Load 批量加载PB数据（支持自定义key提取）
func Load(
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

	for _, msg := range messages {
		if err := loadSingle(redisClient, kafkaProducer, executor, taskKey, msg, keyExtractor, callback); err != nil {
			return err
		}
	}
	return executor.SubmitTask(taskKey)
}

// LoadWithPlayerId 简化版：默认用player_id作为key
func LoadWithPlayerId(
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	executor *taskmanager.TaskExecutor,
	messages []proto.Message,
	callback taskmanager.BatchCallback,
) error {
	return Load(redisClient, kafkaProducer, executor, messages, DefaultPlayerIdExtractor, callback)
}

// 处理单个消息加载
func loadSingle(
	redisClient redis.Cmdable,
	kafkaProducer *kafka.KeyOrderedKafkaProducer,
	executor *taskmanager.TaskExecutor,
	taskKey string,
	msg proto.Message,
	keyExtractor KeyExtractor,
	callback taskmanager.BatchCallback,
) error {
	ctx, _ := context.WithCancel(context.Background())

	// 提取key
	key, err := keyExtractor(msg)
	if err != nil {
		return fmt.Errorf("提取key失败: %w", err)
	}

	// 收集子消息（最多两层嵌套）
	maxLevel := 1
	allSubMsgs := collectSubMessages(msg, 0, maxLevel)

	// 构建父消息缓存键（用于聚合结果存储）
	parentKey := buildParentKey(msg, key)

	// 检查父消息缓存（若存在直接返回）
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

	// 创建聚合器（无论是否有子消息，统一支持聚合）
	aggregator, err := taskmanager.NewGenericAggregator(proto.Clone(msg), parentKey)
	if err != nil {
		return fmt.Errorf("创建聚合器失败: %w", err)
	}

	// 为所有子消息设置key
	var processedSubMsgs []proto.Message
	for _, subMsg := range allSubMsgs {
		if err := setKeyToMessage(subMsg, key, keyExtractor); err != nil {
			return fmt.Errorf("设置子消息key失败 (type=%s): %w", subMsg.ProtoReflect().Descriptor().FullName(), err)
		}
		processedSubMsgs = append(processedSubMsgs, subMsg)
	}

	// 提交任务到执行器
	return taskmanager.InitAndAddMessageTasks(
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
	)
}

// 收集子消息（最多两层，一级有子则不收集自身）
// 收集子消息（最多两层，一级有子则不收集自身）
func collectSubMessages(msg proto.Message, currentLevel int, maxLevel int) []proto.Message {
	if currentLevel > maxLevel {
		return nil
	}

	msgReflect := msg.ProtoReflect()
	fields := msgReflect.Descriptor().Fields()
	var subMsgs []proto.Message

	// 遍历子消息字段
	for i := 0; i < fields.Len(); i++ {
		field := fields.Get(i)

		// 跳过repeated列表类型的字段（避免类型转换错误）
		if field.IsList() {
			continue // 列表类型不处理，只处理单个消息类型
		}

		// 只处理消息类型的字段
		if field.Kind() != protoreflect.MessageKind {
			continue
		}

		// 获取子消息实例（此时field一定是单个消息类型）
		subMsgReflect := msgReflect.Get(field).Message()
		if subMsgReflect == nil {
			continue // 子消息未初始化，跳过
		}
		subMsg := subMsgReflect.Interface()

		// 递归收集（层级+1）
		collected := collectSubMessages(subMsg, currentLevel+1, maxLevel)
		subMsgs = append(subMsgs, collected...)
	}

	// 有子消息则返回子消息，否则返回自身
	if len(subMsgs) > 0 {
		return subMsgs
	}
	if currentLevel <= maxLevel {
		return []proto.Message{msg}
	}
	return nil
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
