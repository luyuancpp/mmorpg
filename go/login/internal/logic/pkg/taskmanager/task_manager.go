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

// 新增：帧级结果Key前缀（区分单任务Key）
const frameResultKeyPrefix = "frame:result:"

// 1. 新增：TaskBatch增加创建时间（用于定时清理）
type TaskBatch struct {
	Tasks          []*MessageTask
	Aggregator     Aggregator
	skipSubCache   bool
	Callback       BatchCallback
	CreateTime     time.Time // 新增：批次创建时间
	FrameResultKey string    // 新增：帧级结果Key
}

// 2. TaskManager增加定时清理协程
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch
	ctx     context.Context // 新增：用于控制清理协程
}

// NewTaskManager 初始化（启动定时清理）
func NewTaskManager(ctx context.Context) *TaskManager {
	tm := &TaskManager{
		batches: make(map[string]*TaskBatch),
		ctx:     ctx,
	}
	// 启动定时清理：每5秒清理一次超时（10秒）批次
	go tm.startBatchCleaner(5*time.Second, 10*time.Second)
	return tm
}

// 新增：批次定时清理
func (tm *TaskManager) startBatchCleaner(interval, expireTime time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			tm.mu.Lock()
			// 遍历所有批次，删除超时批次
			for taskKey, batch := range tm.batches {
				if time.Since(batch.CreateTime) > expireTime {
					delete(tm.batches, taskKey)
					logx.Infof("clean expired batch: taskKey=%s, createTime=%v", taskKey, batch.CreateTime)
				}
			}
			tm.mu.Unlock()
		case <-tm.ctx.Done():
			logx.Debug("task manager closed: stop batch cleaner")
			return
		}
	}
}

// 3. 修改AddAggregateBatch：添加帧级结果Key和创建时间
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	for _, task := range tasks {
		task.SkipSubCache = true
	}

	// 生成帧级结果Key（taskKey唯一，确保帧级Key不重复）
	frameResultKey := frameResultKeyPrefix + taskKey

	tm.batches[taskKey] = &TaskBatch{
		Tasks:          tasks,
		Aggregator:     aggregator,
		skipSubCache:   true,
		Callback:       callback,
		CreateTime:     time.Now(),
		FrameResultKey: frameResultKey,
	}
}

// 4. 修改AddBatch：同理添加帧级结果Key和创建时间
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	for _, task := range tasks {
		task.SkipSubCache = false
	}

	frameResultKey := frameResultKeyPrefix + taskKey

	tm.batches[taskKey] = &TaskBatch{
		Tasks:          tasks,
		Aggregator:     nil,
		skipSubCache:   false,
		Callback:       callback,
		CreateTime:     time.Now(),
		FrameResultKey: frameResultKey,
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

// 5. 重写ProcessBatch：帧级结果监听（替代单任务BLPop）
func (tm *TaskManager) ProcessBatch(taskKey string, redisClient redis.Cmdable) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second) // 延长超时适配批量
	defer cancel()

	logx.Infof("Processing task batch: %s", taskKey)

	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("Batch not found: %s", taskKey)
		return
	}

	// 关键修改：监听帧级结果Key（1次监听替代N次单任务监听）
	logx.Debugf("waiting frame result: key=%s", batch.FrameResultKey)
	_, err := redisClient.BLPop(ctx, time.Duration(config.AppConfig.Timeouts.TaskWaitTimeoutSec)*time.Second, batch.FrameResultKey).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			err = fmt.Errorf("frame result timeout: taskKey=%s", taskKey)
		}
		logx.Errorf("frame result listen failed: %v", err)
		// 标记所有任务失败
		for _, task := range batch.Tasks {
			task.Status = TaskStatusFailed
			task.Error = err
		}
		allSuccess := false
		batchErr := err
		// 清理批次并触发回调
		tm.cleanBatchAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, ctx)
		return
	}

	// 帧级结果已到，批量查询所有任务结果
	allSuccess := true
	var batchErr error
	taskResultKeys := make([]string, 0, len(batch.Tasks))
	taskMap := make(map[string]*MessageTask, len(batch.Tasks))
	for _, task := range batch.Tasks {
		resultKey := fmt.Sprintf("task:%s", task.TaskID)
		taskResultKeys = append(taskResultKeys, resultKey)
		taskMap[resultKey] = task
	}

	// 批量获取任务结果（MGet替代循环Get，减少Redis请求）
	results, err := redisClient.MGet(ctx, taskResultKeys...).Result()
	if err != nil {
		batchErr = fmt.Errorf("batch get task results failed: %w", err)
		allSuccess = false
		logx.Errorf(batchErr.Error())
		tm.cleanBatchAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, ctx)
		return
	}

	// 解析批量结果
	for i, res := range results {
		resultKey := taskResultKeys[i]
		task := taskMap[resultKey]
		if res == nil {
			err = fmt.Errorf("task result not found: %s", resultKey)
			task.Status = TaskStatusFailed
			task.Error = err
			allSuccess = false
			logx.Error(err.Error())
			continue
		}

		resBytes, ok := res.(string)
		if !ok {
			err = fmt.Errorf("invalid task result type: %s", resultKey)
			task.Status = TaskStatusFailed
			task.Error = err
			allSuccess = false
			logx.Error(err.Error())
			continue
		}

		// 解析TaskResult
		var taskResult login_proto.TaskResult
		if err := proto.Unmarshal([]byte(resBytes), &taskResult); err != nil {
			err = fmt.Errorf("unmarshal task result failed: %w", err)
			task.Status = TaskStatusFailed
			task.Error = err
			allSuccess = false
			logx.Error(err.Error())
			continue
		}

		if !taskResult.Success {
			err = fmt.Errorf("task failed: %s, err=%s", task.TaskID, taskResult.Error)
			task.Status = TaskStatusFailed
			task.Error = err
			allSuccess = false
			logx.Error(err.Error())
			continue
		}

		// 解析子PB
		if err := proto.Unmarshal(taskResult.Data, task.Message); err != nil {
			err = fmt.Errorf("unmarshal task message failed: %w", err)
			task.Status = TaskStatusFailed
			task.Error = err
			allSuccess = false
			logx.Error(err.Error())
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
				err = fmt.Errorf("save task to redis failed: %w", err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				logx.Error(err.Error())
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
			batchErr = fmt.Errorf("aggregation failed: %w", err)
			allSuccess = false
			logx.Error(batchErr.Error())
		} else {
			parentKey := batch.Aggregator.ParentKey()
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				parentKey,
				parentPB,
				time.Duration(config.AppConfig.Timeouts.RoleCacheExpireHours)*time.Hour,
			); err != nil {
				batchErr = fmt.Errorf("save aggregated PB failed: %w", err)
				allSuccess = false
				logx.Error(batchErr.Error())
			} else {
				logx.Infof("Aggregated PB saved: %s", parentKey)
			}
		}
	}

	// 清理批次并触发回调
	tm.cleanBatchAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, ctx)
}

// 新增：统一清理批次和触发回调的工具函数
func (tm *TaskManager) cleanBatchAndCallback(
	taskKey string,
	batch *TaskBatch,
	allSuccess bool,
	batchErr error,
	redisClient redis.Cmdable,
	ctx context.Context,
) {
	// 清理Redis键（批量删除，减少请求）
	keysToDelete := make([]string, 0, len(batch.Tasks)+1)
	keysToDelete = append(keysToDelete, batch.FrameResultKey) // 删除帧级结果Key
	for _, task := range batch.Tasks {
		keysToDelete = append(keysToDelete, fmt.Sprintf("task:%s", task.TaskID)) // 删除单任务Key
	}
	if _, err := redisClient.Del(ctx, keysToDelete...).Result(); err != nil {
		logx.Errorf("batch delete redis keys failed: %v", err)
	} else {
		logx.Infof("cleaned up batch keys: taskKey=%s, keys=%v", taskKey, keysToDelete)
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

// 6. 修改InitAndAddMessageTasks：支持批量投递Kafka
type InitTaskOptions struct {
	Aggregator Aggregator    // 聚合器（可选）
	Callback   BatchCallback // 回调函数（可选）
}

// InitAndAddMessageTasks 初始化并添加消息任务（支持回调传递）
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
	var dbTasks []*login_proto.DBTask // 新增：批量DBTask

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

	// 处理未命中缓存的消息：收集批量DBTask
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

		// 创建任务ID
		taskID := uuid.NewString()
		msgType := string(msg.ProtoReflect().Descriptor().FullName())

		// 构建DBTask（加入批量列表）
		dbTask := &login_proto.DBTask{
			Key:       playerId,
			WhereCase: "where player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   msgType,
			Body:      data,
			TaskId:    taskID,
		}
		dbTasks = append(dbTasks, dbTask)

		// 添加到消息任务列表
		tasks = append(tasks, &MessageTask{
			TaskID:   taskID,
			Message:  msg,
			RedisKey: key,
			PlayerID: playerId,
			Status:   TaskStatusPending,
		})
	}

	// 关键修改：批量投递Kafka（大于1条时走批量，否则单条）
	if len(dbTasks) > 0 {
		if len(dbTasks) == 1 {
			if err := producer.SendTask(dbTasks[0], playerIdStr); err != nil {
				logx.Infof("Kafka enqueue single task failed: %v", err)
				return err
			}
		} else {
			if err := producer.SendTasks(dbTasks, playerIdStr); err != nil {
				logx.Infof("Kafka enqueue batch tasks failed: %v", err)
				return err
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
	} else if options.Callback != nil {
		// 所有任务都命中缓存时，手动触发回调
		go options.Callback(taskKey, true, nil)
	}

	return nil
}

// 其他原有结构（Aggregator、GenericAggregator、MessageTask、BatchCallback、TaskExecutor等）保持不变
