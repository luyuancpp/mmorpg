package taskmanager

import (
	"context"
	"errors"
	"fmt"
	"login/internal/config"
	"login/internal/logic/pkg/cache"
	"login/internal/logic/pkg/task"
	login_proto "login/proto/service/go/grpc/db"
	"strconv"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// 任务状态常量
const (
	TaskStatusPending = "pending"
	TaskStatusFailed  = "failed"
	TaskStatusDone    = "done"
)

// Aggregator 聚合器接口：定义子PB如何聚合到父PB
type Aggregator interface {
	Aggregate(subTasks []*MessageTask) (parentPB proto.Message, err error)
	ParentKey() string
}

// GenericAggregator 通用聚合器
type GenericAggregator struct {
	parentTemplate proto.Message
	parentKey      string
}

// NewGenericAggregator 创建通用聚合器
func NewGenericAggregator(parentTemplate proto.Message, parentKey string) (*GenericAggregator, error) {
	if parentTemplate == nil {
		return nil, errors.New("parentTemplate cannot be nil")
	}
	return &GenericAggregator{
		parentTemplate: parentTemplate,
		parentKey:      parentKey,
	}, nil
}

// Aggregate 实现聚合逻辑
func (g *GenericAggregator) Aggregate(subTasks []*MessageTask) (proto.Message, error) {
	parent := proto.Clone(g.parentTemplate)
	parentReflect := parent.ProtoReflect()
	parentDesc := parentReflect.Descriptor()
	fields := parentDesc.Fields()

	for _, task := range subTasks {
		subPB := task.Message
		if subPB == nil {
			continue
		}

		subDesc := subPB.ProtoReflect().Descriptor()
		subTypeName := string(subDesc.Name())

		found := false
		for i := 0; i < fields.Len(); i++ {
			field := fields.Get(i)
			if field.Kind() == protoreflect.MessageKind &&
				string(field.Message().Name()) == subTypeName {

				parentReflect.Set(field, protoreflect.ValueOf(subPB.ProtoReflect()))
				found = true
				break
			}
		}

		if !found {
			return nil, fmt.Errorf("父PB中未找到与子PB类型匹配的字段: %s", subTypeName)
		}
	}

	return parent, nil
}

// ParentKey 返回父PB缓存Key
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
	SkipSubCache bool
}

// BatchCallback 批次处理完成回调
type BatchCallback func(taskKey string, allSuccess bool, err error)

// TaskBatch 任务批次
type TaskBatch struct {
	Tasks        []*MessageTask
	Aggregator   Aggregator
	skipSubCache bool
	Callback     BatchCallback
}

// TaskManager 任务管理器
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch
}

// NewTaskManager 创建任务管理器
func NewTaskManager() *TaskManager {
	return &TaskManager{
		batches: make(map[string]*TaskBatch),
	}
}

// AddAggregateBatch 添加聚合任务批次（带回调）
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	for _, task := range tasks {
		task.SkipSubCache = true
	}

	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   aggregator,
		skipSubCache: true,
		Callback:     callback,
	}
}

// AddBatch 添加普通任务批次（带回调）
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	for _, task := range tasks {
		task.SkipSubCache = false
	}

	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   nil,
		skipSubCache: false,
		Callback:     callback,
	}
}

// GetBatch 获取任务批次
func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, exists := tm.batches[taskKey]
	return batch, exists
}

// SaveProtoToRedis 保存PB到Redis
func SaveProtoToRedis(ctx context.Context, redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto error: %v", err)
		return err
	}
	return redis.Set(ctx, key, data, ttl).Err()
}

// WaitForTaskResult 等待任务结果
func WaitForTaskResult(ctx context.Context, redisClient redis.Cmdable, key string, timeout time.Duration) ([]byte, error) {
	result, err := redisClient.BLPop(ctx, timeout, key).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return nil, fmt.Errorf("timeout waiting for task: %s", key)
		}
		return nil, err
	}

	if len(result) != 2 {
		return nil, fmt.Errorf("unexpected BLPOP result: %v", result)
	}

	return []byte(result[1]), nil
}

// ProcessBatch 处理任务批次（触发回调）
func (tm *TaskManager) ProcessBatch(taskKey string, redisClient redis.Cmdable) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	logx.Infof("Processing task batch: %s", taskKey)

	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("Batch not found: %s", taskKey)
		return
	}

	allSuccess := true
	var batchErr error

	// 处理子任务
	for _, task := range batch.Tasks {
		if task.Status != TaskStatusPending {
			logx.Infof("Skip task %s (status: %s)", task.TaskID, task.Status)
			continue
		}

		logx.Infof("Waiting for task %s", task.TaskID)
		resBytes, err := WaitForTaskResult(
			ctx,
			redisClient,
			task.TaskID,
			time.Duration(config.AppConfig.Timeouts.TaskWaitTimeoutSec)*time.Second,
		)
		if err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Task %s wait failed: %v", task.TaskID, err)
			allSuccess = false
			continue
		}

		// 解析任务结果
		var result login_proto.TaskResult
		if err := proto.Unmarshal(resBytes, &result); err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Unmarshal TaskResult failed: %v", err)
			allSuccess = false
			continue
		}

		if !result.Success {
			err := fmt.Errorf("task failed: %s", result.Error)
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Task %s failed: %s", task.TaskID, result.Error)
			allSuccess = false
			continue
		}

		// 解析子PB
		if err := proto.Unmarshal(result.Data, task.Message); err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Unmarshal message failed: %v", err)
			allSuccess = false
			continue
		}

		// 缓存子PB（普通任务）
		if !task.SkipSubCache {
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				task.RedisKey,
				task.Message,
				time.Duration(config.AppConfig.Timeouts.RoleCacheExpireHours)*time.Hour,
			); err != nil {
				task.Status = TaskStatusFailed
				task.Error = err
				logx.Errorf("Save to Redis failed: %v", err)
				allSuccess = false
				continue
			}
			logx.Infof("Task %s saved to Redis: %s", task.TaskID, task.RedisKey)
		}

		task.Status = TaskStatusDone
		logx.Infof("Task %s processed successfully", task.TaskID)
	}

	// 处理聚合
	if batch.Aggregator != nil && allSuccess {
		logx.Infof("Aggregating sub-PBs for %s", taskKey)
		parentPB, err := batch.Aggregator.Aggregate(batch.Tasks)
		if err != nil {
			logx.Errorf("Aggregation failed: %v", err)
			batchErr = err
			allSuccess = false
		} else {
			parentKey := batch.Aggregator.ParentKey()
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				parentKey,
				parentPB,
				time.Duration(config.AppConfig.Timeouts.RoleCacheExpireHours)*time.Hour,
			); err != nil {
				logx.Errorf("Save aggregated PB failed: %v", err)
				batchErr = err
				allSuccess = false
			} else {
				logx.Infof("Aggregated PB saved: %s", parentKey)
			}
		}
	}

	// 清理批次
	tm.mu.Lock()
	delete(tm.batches, taskKey)
	tm.mu.Unlock()

	logx.Infof("Batch %s processed", taskKey)

	// 触发回调
	if batch.Callback != nil {
		go func() {
			defer func() {
				if r := recover(); r != nil {
					logx.Errorf("Callback panic for %s: %v", taskKey, r)
				}
			}()
			batch.Callback(taskKey, allSuccess, batchErr)
		}()
	}
}

// InitTaskOptions 初始化任务选项（带回调）
type InitTaskOptions struct {
	Aggregator Aggregator    // 聚合器（可选）
	Callback   BatchCallback // 回调函数（可选）
}

// InitAndAddMessageTasks 初始化并添加消息任务（支持回调传递）
func InitAndAddMessageTasks(
	ctx context.Context,
	executor *TaskExecutor, // 假设TaskExecutor已实现GetTaskManagerByKey方法
	taskKey string,
	redisClient redis.Cmdable,
	asyncClient *asynq.Client,
	playerId uint64,
	messages []proto.Message,
	options InitTaskOptions,
) error {
	playerIdStr := strconv.FormatUint(playerId, 10)
	var tasks []*MessageTask

	// 构建缓存键和映射
	keys := make([]string, 0, len(messages))
	msgMap := make(map[string]proto.Message, len(messages))
	for _, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr)
		keys = append(keys, key)
		msgMap[key] = msg
	}

	// 批量查询缓存
	values, err := redisClient.MGet(ctx, keys...).Result()
	if err != nil {
		return err
	}

	// 处理未命中缓存的消息
	for i, val := range values {
		if val != nil {
			continue // 缓存命中，跳过
		}

		key := keys[i]
		msg := msgMap[key]

		// 序列化消息
		data, err := proto.Marshal(msg)
		if err != nil {
			return err
		}

		// 创建任务ID
		taskID := uuid.NewString()
		msgType := string(msg.ProtoReflect().Descriptor().FullName())

		// 构建任务 payload
		taskPayload := &login_proto.DBTask{
			Key:       playerId,
			WhereCase: "where player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   msgType,
			Body:      data,
			TaskId:    taskID,
		}

		payloadBytes, err := proto.Marshal(taskPayload)
		if err != nil {
			return err
		}

		// 入队任务
		taskID, err = task.EnqueueTaskWithID(ctx, asyncClient, playerId, taskID, payloadBytes)
		if err != nil {
			return err
		}

		// 添加到任务列表
		tasks = append(tasks, &MessageTask{
			TaskID:   taskID,
			Message:  msg,
			RedisKey: key,
			PlayerID: playerId,
			Status:   TaskStatusPending,
		})
	}

	// 添加任务到批次（传递回调）
	if len(tasks) > 0 {
		manager := executor.GetTaskManagerByKey(taskKey)
		if options.Aggregator != nil {
			// 聚合任务：传入回调
			manager.AddAggregateBatch(taskKey, tasks, options.Aggregator, options.Callback)
		} else {
			// 普通任务：传入回调
			manager.AddBatch(taskKey, tasks, options.Callback)
		}
	} else if options.Callback != nil {
		// 所有任务都命中缓存时，手动触发回调
		go options.Callback(taskKey, true, nil)
	}

	return nil
}
