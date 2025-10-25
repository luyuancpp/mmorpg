package taskmanager

import (
	"context"
	"errors"
	"fmt"
	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/cache"
	login_proto "login/proto/service/go/grpc/db"
	"strconv"
	"sync"
	"time"
)

// 任务状态
const (
	TaskStatusPending = "pending"
	TaskStatusFailed  = "failed"
	TaskStatusDone    = "done"
)

// Aggregator 聚合器接口
type Aggregator interface {
	Aggregate(subTasks []*MessageTask) (parentPB proto.Message, err error)
	ParentKey() string
}

// GenericAggregator 通用聚合器
type GenericAggregator struct {
	parentTemplate proto.Message
	parentKey      string
}

func NewGenericAggregator(parentTemplate proto.Message, parentKey string) (*GenericAggregator, error) {
	if parentTemplate == nil {
		return nil, errors.New("parentTemplate不能为空")
	}
	return &GenericAggregator{
		parentTemplate: parentTemplate,
		parentKey:      parentKey,
	}, nil
}

// Aggregate 将子任务结果聚合到父消息
func (g *GenericAggregator) Aggregate(subTasks []*MessageTask) (proto.Message, error) {
	parent := proto.Clone(g.parentTemplate)
	parentReflect := parent.ProtoReflect()
	parentFields := parentReflect.Descriptor().Fields()

	for _, task := range subTasks {
		if task.SkipSubCache {
			continue
		}
		subPB := task.Message
		if subPB == nil {
			continue
		}

		subDesc := subPB.ProtoReflect().Descriptor()
		subTypeName := string(subDesc.Name())

		// 匹配父消息中对应类型的字段
		found := false
		for i := 0; i < parentFields.Len(); i++ {
			field := parentFields.Get(i)
			if field.Kind() == protoreflect.MessageKind &&
				string(field.Message().Name()) == subTypeName {
				parentReflect.Set(field, protoreflect.ValueOf(subPB.ProtoReflect()))
				found = true
				break
			}
		}
		if !found {
			return nil, fmt.Errorf("父消息缺少子消息类型字段: %s", subTypeName)
		}
	}
	return parent, nil
}

func (g *GenericAggregator) ParentKey() string {
	return g.parentKey
}

// MessageTask 单个任务信息
type MessageTask struct {
	TaskID       string
	Message      proto.Message
	RedisKey     string
	PlayerID     uint64
	Status       string
	Error        error
	SkipSubCache bool // 是否跳过子消息缓存
}

// BatchCallback 批次完成回调
type BatchCallback func(taskKey string, allSuccess bool, err error)

// TaskBatch 任务批次
type TaskBatch struct {
	Tasks        []*MessageTask
	Aggregator   Aggregator
	SkipSubCache bool
	Callback     BatchCallback
	CreateTime   time.Time // 用于定时清理
}

// TaskManager 任务批次管理器
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch
	ctx     context.Context
}

func NewTaskManager(ctx context.Context) *TaskManager {
	tm := &TaskManager{
		batches: make(map[string]*TaskBatch),
		ctx:     ctx,
	}
	// 启动定时清理（每5秒清理10秒前的批次）
	go tm.cleanExpiredBatches(5*time.Second, 10*time.Second)
	return tm
}

// 清理过期批次
func (tm *TaskManager) cleanExpiredBatches(interval, expireTime time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			tm.mu.Lock()
			for key, batch := range tm.batches {
				if time.Since(batch.CreateTime) > expireTime {
					delete(tm.batches, key)
					logx.Infof("清理过期批次: %s", key)
				}
			}
			tm.mu.Unlock()
		case <-tm.ctx.Done():
			return
		}
	}
}

// 添加聚合批次
// AddAggregateBatch 聚合批次：相同taskKey则追加任务，不覆盖
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. 标记新任务的SkipSubCache为true
	for _, t := range tasks {
		t.SkipSubCache = true
	}

	// 2. 检查taskKey是否已存在旧批次
	existingBatch, exists := tm.batches[taskKey]
	if exists {
		// 存在则追加任务（保留原有任务，新增任务接在末尾）
		existingBatch.Tasks = append(existingBatch.Tasks, tasks...)
		// 若原有批次无回调，用新回调覆盖；若已有则保留（避免覆盖业务逻辑）
		if existingBatch.Callback == nil && callback != nil {
			existingBatch.Callback = callback
		}
		// 聚合器以首次传入的为准（确保聚合逻辑统一）
		return
	}

	// 3. 不存在则创建新批次
	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   aggregator,
		SkipSubCache: true,
		Callback:     callback,
		CreateTime:   time.Now(),
	}
}

// AddBatch 普通批次：相同taskKey则追加任务，不覆盖
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. 标记新任务的SkipSubCache为false
	for _, t := range tasks {
		t.SkipSubCache = false
	}

	// 2. 检查taskKey是否已存在旧批次
	existingBatch, exists := tm.batches[taskKey]
	if exists {
		// 存在则追加任务（原有任务保留，新任务接在末尾）
		existingBatch.Tasks = append(existingBatch.Tasks, tasks...)
		// 回调逻辑：无则覆盖，有则保留
		if existingBatch.Callback == nil && callback != nil {
			existingBatch.Callback = callback
		}
		return
	}

	// 3. 不存在则创建新批次
	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   nil,
		SkipSubCache: false,
		Callback:     callback,
		CreateTime:   time.Now(),
	}
}

// 获取批次
func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, ok := tm.batches[taskKey]
	return batch, ok
}

// 保存PB到Redis
func SaveProtoToRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return err
	}
	return redisClient.Set(ctx, key, data, ttl).Err()
}

func (tm *TaskManager) ProcessBatch(ctx context.Context, taskKey string, redisClient redis.Cmdable) {
	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("批次不存在: %s", taskKey)
		return
	}

	// 准备所有任务的Redis结果键（建议统一前缀，避免冲突）
	taskResultKeys := make([]string, len(batch.Tasks))
	taskMap := make(map[string]*MessageTask, len(batch.Tasks))
	for i, t := range batch.Tasks {
		resultKey := fmt.Sprintf("task:result:%s", t.TaskID) // 带前缀的结果键
		taskResultKeys[i] = resultKey
		taskMap[resultKey] = t // 映射：结果键 -> 任务
	}

	// 用于标记每个任务是否已完成
	completed := make(map[string]bool, len(taskResultKeys))
	allSuccess := true
	var batchErr error

	// 循环阻塞等待所有任务结果（直到全部完成或超时）
	for len(completed) < len(taskResultKeys) {
		select {
		case <-ctx.Done():
			// 超时：标记未完成的任务为失败
			batchErr = fmt.Errorf("等待任务结果超时")
			allSuccess = false
			for _, key := range taskResultKeys {
				if !completed[key] {
					task := taskMap[key]
					task.Status = TaskStatusFailed
					task.Error = batchErr
				}
			}
			tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient)
			return

		default:
			// 阻塞等待任意一个任务结果（超时1秒，避免无限阻塞）
			result, err := redisClient.BLPop(ctx, 1*time.Second, taskResultKeys...).Result()
			if err != nil {
				if errors.Is(err, redis.Nil) {
					continue // 1秒内无结果，继续等待
				}
				// Redis操作错误：标记所有未完成任务为失败
				batchErr = fmt.Errorf("Redis BLPop失败: %w", err)
				allSuccess = false
				for _, key := range taskResultKeys {
					if !completed[key] {
						task := taskMap[key]
						task.Status = TaskStatusFailed
						task.Error = batchErr
					}
				}
				tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient)
				return
			}

			// 解析返回结果（BLPop返回 [key, value]）
			if len(result) != 2 {
				logx.Errorf("BLPop返回格式错误: %v", result)
				continue
			}
			resultKey := result[0]
			resultValue := result[1]
			task, ok := taskMap[resultKey]
			if !ok {
				logx.Errorf("未知的任务结果键: %s", resultKey)
				continue
			}

			// 标记任务已完成
			completed[resultKey] = true

			// 反序列化任务结果
			var taskResult login_proto.TaskResult
			if err := proto.Unmarshal([]byte(resultValue), &taskResult); err != nil {
				err = fmt.Errorf("反序列化任务结果失败: %w", err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}

			if !taskResult.Success {
				err = errors.New(taskResult.Error)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}

			// 反序列化子消息
			if err := proto.Unmarshal(taskResult.Data, task.Message); err != nil {
				err = fmt.Errorf("反序列化消息失败: %w", err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}

			// 缓存子消息（非聚合任务）
			if !task.SkipSubCache {
				if err := SaveProtoToRedis(
					ctx,
					redisClient,
					task.RedisKey,
					task.Message,
					config.AppConfig.Timeouts.RoleCacheExpire,
				); err != nil {
					err = fmt.Errorf("缓存失败: %w", err)
					task.Status = TaskStatusFailed
					task.Error = err
					allSuccess = false
					continue
				}
			}

			task.Status = TaskStatusDone
			logx.Infof("任务 %s 处理完成", task.TaskID)
		}
	}

	// 聚合结果（若需要）
	if batch.Aggregator != nil && allSuccess {
		logx.Infof("聚合批次 %s 的子任务结果", taskKey)
		parentPB, err := batch.Aggregator.Aggregate(batch.Tasks)
		if err != nil {
			batchErr = fmt.Errorf("聚合失败: %w", err)
			allSuccess = false
		} else {
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				batch.Aggregator.ParentKey(),
				parentPB,
				config.AppConfig.Timeouts.RoleCacheExpire,
			); err != nil {
				batchErr = fmt.Errorf("缓存聚合结果失败: %w", err)
				allSuccess = false
			}
		}
	}

	tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient)
}

// 清理资源并触发回调
func (tm *TaskManager) cleanupAndCallback(
	taskKey string,
	batch *TaskBatch,
	allSuccess bool,
	err error,
	redisClient redis.Cmdable,
) {
	ctx, cancel := context.WithCancel(context.Background())
	// 注意：使用完后需要调用 cancel() 释放资源，避免内存泄漏
	defer cancel()

	// 关键修复：生成带前缀的键（与存储时一致）
	keys := make([]string, len(batch.Tasks))
	for i, t := range batch.Tasks {
		// 与 ProcessBatch 中生成结果键的逻辑保持一致
		keys[i] = fmt.Sprintf("task:result:%s", t.TaskID)
	}

	// 执行删除
	if len(keys) > 0 {
		if _, delErr := redisClient.Del(ctx, keys...).Result(); delErr != nil {
			logx.Errorf("删除任务结果键失败: %v, 键列表: %v", delErr, keys)
		} else {
			logx.Debugf("成功删除任务结果键: %v", keys) // 新增日志，验证删除效果
		}
	}

	// 移除批次（原有逻辑不变）
	tm.mu.Lock()
	delete(tm.batches, taskKey)
	tm.mu.Unlock()

	// 触发回调（原有逻辑不变）
	if batch.Callback != nil {
		go func() {
			defer recover()
			batch.Callback(taskKey, allSuccess, err)
		}()
	}
}

// InitTaskOptions 任务初始化选项
type InitTaskOptions struct {
	Aggregator Aggregator
	Callback   BatchCallback
}

// InitAndAddMessageTasks 初始化并添加消息任务
func InitAndAddMessageTasks(
	ctx context.Context,
	executor *TaskExecutor,
	taskKey string,
	redisClient redis.Cmdable,
	producer *kafka.KeyOrderedKafkaProducer,
	playerId uint64,
	messages []proto.Message,
	options InitTaskOptions,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	var tasks []*MessageTask
	var dbTasks []*login_proto.DBTask

	// 批量查询缓存
	keys := make([]string, len(messages))
	msgMap := make(map[string]proto.Message, len(messages))
	for i, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr)
		keys[i] = key
		msgMap[key] = msg
	}
	values, err := redisClient.MGet(ctx, keys...).Result()
	if err != nil {
		return err
	}

	// 收集未命中缓存的任务
	for i, val := range values {
		if val != nil {
			continue
		}
		key := keys[i]
		msg := msgMap[key]

		// 序列化消息
		data, err := proto.Marshal(msg)
		if err != nil {
			return err
		}

		// 创建任务
		taskID := uuid.NewString()
		dbTasks = append(dbTasks, &login_proto.DBTask{
			Key:       playerId,
			WhereCase: "player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   string(msg.ProtoReflect().Descriptor().FullName()),
			Body:      data,
			TaskId:    taskID,
		})
		tasks = append(tasks, &MessageTask{
			TaskID:   taskID,
			Message:  msg,
			RedisKey: key,
			PlayerID: playerId,
			Status:   TaskStatusPending,
		})
	}

	// 批量投递Kafka
	if len(dbTasks) > 0 {
		if len(dbTasks) == 1 {
			if err := producer.SendTask(ctx, dbTasks[0], playerIdStr); err != nil {
				return fmt.Errorf("Kafka投递单任务失败: %w", err)
			}
		} else {
			if err := producer.SendTasks(ctx, dbTasks, playerIdStr); err != nil {
				return fmt.Errorf("Kafka投递批量任务失败: %w", err)
			}
		}
	}

	// 添加任务到批次
	if len(tasks) > 0 {
		manager := executor.GetTaskManagerByKey(taskKey)
		if options.Aggregator != nil {
			manager.AddAggregateBatch(taskKey, tasks, options.Aggregator, options.Callback)
		} else {
			manager.AddBatch(taskKey, tasks, options.Callback)
		}
	}
	return nil
}
