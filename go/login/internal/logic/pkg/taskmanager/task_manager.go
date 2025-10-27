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

// Aggregator 聚合器接口：每个聚合器管理自己的子任务
type Aggregator interface {
	Aggregate() (parentPB proto.Message, err error) // 用自身子任务聚合，无需外部传参
	ParentKey() string                              // 聚合结果的Redis键（唯一标识聚合器）
	AddSubTask(task *MessageTask)                   // 添加子任务到聚合器
	GetSubTasks() []*MessageTask                    // 获取聚合器的所有子任务
	GetSubTaskIDs() []string                        // 获取子任务ID列表（用于批量等待）
}

// GenericAggregator 通用聚合器实现
type GenericAggregator struct {
	parentTemplate proto.Message  // 父消息模板
	parentKey      string         // 聚合结果的Redis键
	subTasks       []*MessageTask // 聚合器自己的子任务
	subTaskIDs     []string       // 子任务ID列表（方便批量等待）
	mu             sync.Mutex     // 保证子任务操作的并发安全
}

// NewGenericAggregator 创建通用聚合器
func NewGenericAggregator(parentTemplate proto.Message, parentKey string) (*GenericAggregator, error) {
	if parentTemplate == nil {
		return nil, errors.New("parentTemplate不能为空")
	}
	return &GenericAggregator{
		parentTemplate: parentTemplate,
		parentKey:      parentKey,
		subTasks:       make([]*MessageTask, 0),
		subTaskIDs:     make([]string, 0),
	}, nil
}

// Aggregate 聚合自身子任务：核心修复，只处理自己的子任务
func (g *GenericAggregator) Aggregate() (proto.Message, error) {
	parent := proto.Clone(g.parentTemplate)
	parentReflect := parent.ProtoReflect()
	parentFields := parentReflect.Descriptor().Fields()

	// 只处理聚合器自己持有的子任务
	subTasks := g.GetSubTasks()
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
			if field.Kind() == protoreflect.MessageKind && string(field.Message().Name()) == subTypeName {
				parentReflect.Set(field, protoreflect.ValueOf(subPB.ProtoReflect()))
				found = true
				break
			}
		}
		if !found {
			return nil, fmt.Errorf("聚合器[%s]：父消息缺少子消息类型字段[%s]", g.parentKey, subTypeName)
		}
	}
	return parent, nil
}

// ParentKey 返回聚合结果的Redis键（唯一标识）
func (g *GenericAggregator) ParentKey() string {
	return g.parentKey
}

// AddSubTask 新增子任务到聚合器：同时记录任务和任务ID
func (g *GenericAggregator) AddSubTask(task *MessageTask) {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.subTasks = append(g.subTasks, task)
	g.subTaskIDs = append(g.subTaskIDs, task.TaskID)
}

// GetSubTasks 获取子任务（返回副本，避免外部修改）
func (g *GenericAggregator) GetSubTasks() []*MessageTask {
	g.mu.Lock()
	defer g.mu.Unlock()
	tasksCopy := make([]*MessageTask, len(g.subTasks))
	copy(tasksCopy, g.subTasks)
	return tasksCopy
}

// GetSubTaskIDs 获取子任务ID列表（用于批量等待所有任务）
func (g *GenericAggregator) GetSubTaskIDs() []string {
	g.mu.Lock()
	defer g.mu.Unlock()
	idsCopy := make([]string, len(g.subTaskIDs))
	copy(idsCopy, g.subTaskIDs)
	return idsCopy
}

// MessageTask 单个任务信息（原有字段不变）
type MessageTask struct {
	TaskID       string
	Message      proto.Message
	RedisKey     string
	PlayerID     uint64
	Status       string
	Error        error
	SkipSubCache bool // 聚合子任务跳过单独缓存，非聚合任务需缓存
}

// BatchCallback 批次完成回调：一个taskKey对应一个回调
type BatchCallback func(taskKey string, allSuccess bool, err error)

// TaskBatch 任务批次：支持多个聚合器+非聚合任务
type TaskBatch struct {
	Aggregators []Aggregator   // 该taskKey下的所有聚合器（每个聚合器有自己的子任务）
	NormalTasks []*MessageTask // 该taskKey下的非聚合任务（独立任务）
	Callback    BatchCallback  // 批次完成回调
	CreateTime  time.Time      // 用于过期清理
}

// TaskManager 任务批次管理器：管理多个taskKey的批次
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch // key: taskKey
	ctx     context.Context
}

// NewTaskManager 创建任务管理器：启动过期清理协程
func NewTaskManager(ctx context.Context) *TaskManager {
	tm := &TaskManager{
		batches: make(map[string]*TaskBatch),
		ctx:     ctx,
	}
	go tm.cleanExpiredBatches(config.AppConfig.Timeouts.TaskManagerCleanInterval, config.AppConfig.Timeouts.TaskBatchExpireTime)
	return tm
}

// cleanExpiredBatches 定时清理过期批次：避免内存泄漏
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
					logx.Infof("清理过期批次: taskKey=%s", key)
				}
			}
			tm.mu.Unlock()
		case <-tm.ctx.Done():
			logx.Info("任务管理器停止，过期清理协程退出")
			return
		}
	}
}

// AddAggregateBatch 给taskKey添加聚合器及子任务：支持多个聚合器
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. 标记子任务为“跳过单独缓存”，并添加到聚合器
	for _, t := range tasks {
		t.SkipSubCache = true
		aggregator.AddSubTask(t)
	}

	// 2. 处理批次：不存在则新建，存在则追加聚合器（避免重复）
	existingBatch, exists := tm.batches[taskKey]
	if !exists {
		// 新建批次：初始化聚合器列表和非聚合任务列表
		tm.batches[taskKey] = &TaskBatch{
			Aggregators: []Aggregator{aggregator},
			NormalTasks: make([]*MessageTask, 0),
			Callback:    callback,
			CreateTime:  time.Now(),
		}
		return
	}

	// 3. 批次已存在：检查聚合器是否已存在（按ParentKey唯一）
	aggExists := false
	targetParentKey := aggregator.ParentKey()
	for _, a := range existingBatch.Aggregators {
		if a.ParentKey() == targetParentKey {
			aggExists = true
			break
		}
	}
	// 仅添加新的聚合器
	if !aggExists {
		existingBatch.Aggregators = append(existingBatch.Aggregators, aggregator)
	}

	// 4. 回调逻辑：无则覆盖，有则保留（避免覆盖业务回调）
	if existingBatch.Callback == nil && callback != nil {
		existingBatch.Callback = callback
	}
}

// AddBatch 给taskKey添加非聚合任务
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. 标记子任务为“不跳过单独缓存”
	for _, t := range tasks {
		t.SkipSubCache = false
	}

	// 2. 处理批次：不存在则新建，存在则追加任务
	existingBatch, exists := tm.batches[taskKey]
	if exists {
		existingBatch.NormalTasks = append(existingBatch.NormalTasks, tasks...)
		// 回调逻辑：无则覆盖，有则保留
		if existingBatch.Callback == nil && callback != nil {
			existingBatch.Callback = callback
		}
		return
	}

	// 3. 新建非聚合批次
	tm.batches[taskKey] = &TaskBatch{
		Aggregators: make([]Aggregator, 0),
		NormalTasks: tasks,
		Callback:    callback,
		CreateTime:  time.Now(),
	}
}

// GetBatch 获取taskKey对应的批次
func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, ok := tm.batches[taskKey]
	return batch, ok
}

// SaveProtoToRedis 保存PB到Redis（带过期时间）
func SaveProtoToRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Errorf("PB序列化失败: %w", err)
	}
	if err := redisClient.Set(ctx, key, data, ttl).Err(); err != nil {
		return fmt.Errorf("Redis Set失败: %w", err)
	}
	return nil
}

// ProcessBatch 核心逻辑：等待taskKey下所有任务完成，再处理聚合和回调
func (tm *TaskManager) ProcessBatch(ctx context.Context, taskKey string, redisClient redis.Cmdable) {
	// 1. 校验批次是否存在
	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("批次不存在: taskKey=%s", taskKey)
		return
	}

	allSuccess := true                       // 整个taskKey批次的执行结果
	var batchErr error                       // 批次级错误
	taskMap := make(map[string]*MessageTask) // 所有任务的映射：taskID -> 任务
	taskResultKeys := make([]string, 0)      // 所有任务的Redis结果键（用于批量等待）

	// 2. 收集该taskKey下的所有任务（所有聚合器的子任务 + 非聚合任务）
	// 2.1 收集所有聚合器的子任务
	for _, aggregator := range batch.Aggregators {
		for _, task := range aggregator.GetSubTasks() {
			taskID := task.TaskID
			taskMap[taskID] = task
			taskResultKeys = append(taskResultKeys, fmt.Sprintf("task:result:%s", taskID))
		}
	}
	// 2.2 收集非聚合任务
	for _, task := range batch.NormalTasks {
		taskID := task.TaskID
		taskMap[taskID] = task
		taskResultKeys = append(taskResultKeys, fmt.Sprintf("task:result:%s", taskID))
	}

	// 3. 等待该taskKey下所有任务完成
	completedTaskIDs := make(map[string]bool) // 已完成的任务ID
	for len(completedTaskIDs) < len(taskMap) {
		select {
		case <-ctx.Done():
			// 超时：标记所有未完成任务为失败
			batchErr = fmt.Errorf("taskKey[%s]等待任务超时", taskKey)
			allSuccess = false
			for taskID := range taskMap {
				if !completedTaskIDs[taskID] {
					task := taskMap[taskID]
					task.Status = TaskStatusFailed
					task.Error = batchErr
				}
			}
			tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
			return

		default:
			// BLPop阻塞等待任意任务结果（1秒超时，避免无限循环）
			result, err := redisClient.BLPop(ctx, 1*time.Second, taskResultKeys...).Result()
			if err != nil {
				if errors.Is(err, redis.Nil) {
					continue // 无结果，继续等待
				}
				// Redis错误：标记所有未完成任务为失败
				batchErr = fmt.Errorf("taskKey[%s] Redis BLPop失败: %w", taskKey, err)
				allSuccess = false
				for taskID := range taskMap {
					if !completedTaskIDs[taskID] {
						task := taskMap[taskID]
						task.Status = TaskStatusFailed
						task.Error = batchErr
					}
				}
				tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
				return
			}

			// 解析任务结果
			if len(result) != 2 {
				logx.Errorf("taskKey[%s] BLPop返回格式错误: %v", taskKey, result)
				continue
			}
			resKey, resVal := result[0], result[1]
			// 提取taskID（从resKey：task:result:{taskID}）
			var taskID string
			if _, err := fmt.Sscanf(resKey, "task:result:%s", &taskID); err != nil {
				logx.Errorf("taskKey[%s] 解析任务ID失败: resKey=%s, err=%v", taskKey, resKey, err)
				continue
			}
			task, ok := taskMap[taskID]
			if !ok {
				logx.Errorf("taskKey[%s] 未知任务ID: %s", taskKey, taskID)
				continue
			}

			// 标记任务已完成
			completedTaskIDs[taskID] = true

			// 4. 处理单个任务结果
			var taskResult login_proto.TaskResult
			if err := proto.Unmarshal([]byte(resVal), &taskResult); err != nil {
				err = fmt.Errorf("任务[%s]结果反序列化失败: %w", taskID, err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}
			if !taskResult.Success {
				err = errors.New(fmt.Sprintf("任务[%s]执行失败: %s", taskID, taskResult.Error))
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}
			// 反序列化PB消息
			if err := proto.Unmarshal(taskResult.Data, task.Message); err != nil {
				err = fmt.Errorf("任务[%s]消息反序列化失败: %w", taskID, err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}

			// 5. 非聚合任务需单独缓存，聚合子任务跳过
			if !task.SkipSubCache {
				if err := SaveProtoToRedis(
					ctx,
					redisClient,
					task.RedisKey,
					task.Message,
					config.AppConfig.Timeouts.RoleCacheExpire,
				); err != nil {
					err = fmt.Errorf("任务[%s]缓存失败: %w", taskID, err)
					task.Status = TaskStatusFailed
					task.Error = err
					allSuccess = false
					continue
				}
			}

			task.Status = TaskStatusDone
			logx.Infof("taskKey[%s] 任务处理完成: taskID=%s", taskKey, taskID)
		}
	} // 结束“等待所有任务完成”的 for 循环

	// 6. 所有任务完成后，处理每个聚合器的聚合逻辑（仅当整体成功时）
	if allSuccess && len(batch.Aggregators) > 0 {
		for _, aggregator := range batch.Aggregators {
			logx.Infof("taskKey[%s] 开始聚合: 聚合器Key=%s", taskKey, aggregator.ParentKey())
			// 聚合器用自身子任务执行聚合（已确保只处理自己的子任务）
			parentPB, err := aggregator.Aggregate()
			if err != nil {
				batchErr = fmt.Errorf("聚合器[%s]聚合失败: %w", aggregator.ParentKey(), err)
				allSuccess = false
				logx.Errorf("taskKey[%s] 聚合失败: %v", taskKey, batchErr)
				continue
			}

			// 缓存聚合后的父消息到Redis
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				aggregator.ParentKey(),
				parentPB,
				config.AppConfig.Timeouts.RoleCacheExpire,
			); err != nil {
				batchErr = fmt.Errorf("聚合器[%s]缓存聚合结果失败: %w", aggregator.ParentKey(), err)
				allSuccess = false
				logx.Errorf("taskKey[%s] 聚合结果缓存失败: %v", taskKey, batchErr)
				continue
			}

			logx.Infof("taskKey[%s] 聚合完成: 聚合器Key=%s, 父消息已缓存", taskKey, aggregator.ParentKey())
		}
	}

	// 7. 统一清理资源并触发批次回调
	tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
} // 结束 ProcessBatch 函数

// cleanupAndCallback 清理Redis任务结果键、删除批次、触发回调
func (tm *TaskManager) cleanupAndCallback(
	taskKey string,
	batch *TaskBatch,
	allSuccess bool,
	err error,
	redisClient redis.Cmdable,
	taskResultKeys []string,
) {
	// 7.1 清理Redis中的任务结果键（避免重复处理）
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if len(taskResultKeys) > 0 {
		delResult, delErr := redisClient.Del(ctx, taskResultKeys...).Result()
		if delErr != nil {
			logx.Errorf("taskKey[%s] 删除任务结果键失败: 键列表=%v, err=%v", taskKey, taskResultKeys, delErr)
		} else {
			logx.Debugf("taskKey[%s] 成功删除任务结果键: 删除数量=%d, 键列表=%v", taskKey, delResult, taskResultKeys)
		}
	}

	// 7.2 从管理器中删除当前批次（释放内存）
	tm.mu.Lock()
	delete(tm.batches, taskKey)
	tm.mu.Unlock()

	// 7.3 触发批次回调（独立协程执行，避免阻塞，recover防止回调恐慌）
	if batch.Callback != nil {
		go func() {
			defer func() {
				if r := recover(); r != nil {
					logx.Errorf("taskKey[%s] 回调函数恐慌: %v", taskKey, r)
				}
			}()
			batch.Callback(taskKey, allSuccess, err)
			logx.Infof("taskKey[%s] 批次回调执行完成", taskKey)
		}()
	}
}

// InitTaskOptions 任务初始化选项（保持原有结构）
type InitTaskOptions struct {
	Aggregator Aggregator
	Callback   BatchCallback
}

// InitAndAddMessageTasks 初始化并添加消息任务（适配多聚合器逻辑）
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

	// 1. 批量查询缓存（原有逻辑不变）
	keys := make([]string, len(messages))
	msgMap := make(map[string]proto.Message, len(messages))
	for i, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr)
		keys[i] = key
		msgMap[key] = msg
	}
	values, err := redisClient.MGet(ctx, keys...).Result()
	if err != nil {
		return fmt.Errorf("批量查询缓存失败: %w", err)
	}

	// 2. 收集未命中缓存的任务（原有逻辑不变）
	for i, val := range values {
		if val != nil {
			continue
		}
		key := keys[i]
		msg := msgMap[key]

		// 序列化消息
		data, err := proto.Marshal(msg)
		if err != nil {
			return fmt.Errorf("消息序列化失败: 类型=%s, err=%w", msg.ProtoReflect().Descriptor().FullName(), err)
		}

		// 创建任务ID和任务实例
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

	// 3. 批量投递Kafka（原有逻辑不变）
	if len(dbTasks) > 0 {
		if len(dbTasks) == 1 {
			if err := producer.SendTask(ctx, dbTasks[0], playerIdStr); err != nil {
				return fmt.Errorf("Kafka投递单任务失败: taskID=%s, err=%w", dbTasks[0].TaskId, err)
			}
		} else {
			if err := producer.SendTasks(ctx, dbTasks, playerIdStr); err != nil {
				return fmt.Errorf("Kafka投递批量任务失败: 任务数量=%d, err=%w", len(dbTasks), err)
			}
		}
		logx.Infof("taskKey[%s] Kafka投递完成: 任务数量=%d, playerId=%d", taskKey, len(dbTasks), playerId)
	}

	// 4. 添加任务到批次（适配多聚合器：有聚合器则添加到聚合批次，否则添加到普通批次）
	if len(tasks) > 0 {
		manager := executor.GetTaskManagerByKey(taskKey)
		if options.Aggregator != nil {
			manager.AddAggregateBatch(taskKey, tasks, options.Aggregator, options.Callback)
			logx.Infof("taskKey[%s] 添加聚合任务完成: 任务数量=%d, 聚合器Key=%s", taskKey, len(tasks), options.Aggregator.ParentKey())
		} else {
			manager.AddBatch(taskKey, tasks, options.Callback)
			logx.Infof("taskKey[%s] 添加普通任务完成: 任务数量=%d", taskKey, len(tasks))
		}
	}

	return nil
}
