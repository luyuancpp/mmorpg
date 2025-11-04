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

// Task status constants
const (
	TaskStatusPending = "pending"
	TaskStatusFailed  = "failed"
	TaskStatusDone    = "done"
)

// Aggregator interface: Each aggregator manages its own subtasks
type Aggregator interface {
	Aggregate() (parentPB proto.Message, err error) // Aggregate using its own subtasks, no external parameters needed
	ParentKey() string                              // Redis key for aggregation result (unique identifier for the aggregator)
	AddSubTask(task *MessageTask)                   // Add a subtask to the aggregator
	GetSubTasks() []*MessageTask                    // Get all subtasks of the aggregator
	GetSubTaskIDs() []string                        // Get list of subtask IDs (for batch waiting)
}

// GenericAggregator implements the Aggregator interface
type GenericAggregator struct {
	parentTemplate proto.Message  // Parent message template
	parentKey      string         // Redis key for aggregation result
	subTasks       []*MessageTask // Subtasks managed by the aggregator
	subTaskIDs     []string       // List of subtask IDs (for batch waiting)
	mu             sync.Mutex     // Ensure concurrent safety for subtask operations
}

// NewGenericAggregator creates a GenericAggregator instance
func NewGenericAggregator(parentTemplate proto.Message, parentKey string) (*GenericAggregator, error) {
	if parentTemplate == nil {
		return nil, errors.New("parentTemplate cannot be empty")
	}
	return &GenericAggregator{
		parentTemplate: parentTemplate,
		parentKey:      parentKey,
		subTasks:       make([]*MessageTask, 0),
		subTaskIDs:     make([]string, 0),
	}, nil
}

// Aggregate combines subtasks into a parent message: Core fix - only process its own subtasks
func (g *GenericAggregator) Aggregate() (proto.Message, error) {
	parent := proto.Clone(g.parentTemplate)
	parentReflect := parent.ProtoReflect()
	parentFields := parentReflect.Descriptor().Fields()

	// Only process subtasks owned by the aggregator
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

		// Match the field in parent message corresponding to the subtask type
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
			return nil, fmt.Errorf("Aggregator[%s]: Parent message missing field for subtask type [%s]", g.parentKey, subTypeName)
		}
	}
	return parent, nil
}

// ParentKey returns the Redis key for the aggregation result (unique identifier)
func (g *GenericAggregator) ParentKey() string {
	return g.parentKey
}

// AddSubTask adds a subtask to the aggregator: Records both task and task ID
func (g *GenericAggregator) AddSubTask(task *MessageTask) {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.subTasks = append(g.subTasks, task)
	g.subTaskIDs = append(g.subTaskIDs, task.TaskID)
}

// GetSubTasks returns a copy of subtasks (prevents external modification)
func (g *GenericAggregator) GetSubTasks() []*MessageTask {
	g.mu.Lock()
	defer g.mu.Unlock()
	tasksCopy := make([]*MessageTask, len(g.subTasks))
	copy(tasksCopy, g.subTasks)
	return tasksCopy
}

// GetSubTaskIDs returns a copy of subtask IDs (for waiting all tasks in batch)
func (g *GenericAggregator) GetSubTaskIDs() []string {
	g.mu.Lock()
	defer g.mu.Unlock()
	idsCopy := make([]string, len(g.subTaskIDs))
	copy(idsCopy, g.subTaskIDs)
	return idsCopy
}

// MessageTask stores information for a single task (original fields unchanged)
type MessageTask struct {
	TaskID       string
	Message      proto.Message
	RedisKey     string
	PlayerID     uint64
	Status       string
	Error        error
	SkipSubCache bool // Skip individual caching for aggregated subtasks; required for non-aggregated tasks
}

// BatchCallback is the callback for batch completion: One callback per taskKey
type BatchCallback func(taskKey string, allSuccess bool, err error)

// TaskBatch represents a batch of tasks: Supports multiple aggregators + non-aggregated tasks
type TaskBatch struct {
	Aggregators []Aggregator   // All aggregators under this taskKey (each has its own subtasks)
	NormalTasks []*MessageTask // Non-aggregated tasks under this taskKey (independent tasks)
	Callback    BatchCallback  // Callback for batch completion
	CreateTime  time.Time      // For expired batch cleanup
}

// TaskManager manages batches of tasks: Handles multiple taskKey batches
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch // key: taskKey
	ctx     context.Context
}

// NewTaskManager creates a TaskManager instance: Starts expired batch cleanup goroutine
func NewTaskManager(ctx context.Context) *TaskManager {
	tm := &TaskManager{
		batches: make(map[string]*TaskBatch),
		ctx:     ctx,
	}
	go tm.cleanExpiredBatches(config.AppConfig.Timeouts.TaskManagerCleanInterval, config.AppConfig.Timeouts.TaskBatchExpireTime)
	return tm
}

// cleanExpiredBatches periodically cleans up expired batches: Prevents memory leaks
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
					logx.Infof("Cleaned up expired batch: taskKey=%s, batchCreateTime=%s", key, batch.CreateTime.Format(time.RFC3339))
				}
			}
			tm.mu.Unlock()
		case <-tm.ctx.Done():
			logx.Info("TaskManager stopped, expired batch cleanup goroutine exited")
			return
		}
	}
}

// AddAggregateBatch adds an aggregator and its subtasks to a taskKey: Supports multiple aggregators
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. Mark subtasks to "skip individual caching" and add to the aggregator
	for _, t := range tasks {
		t.SkipSubCache = true
		aggregator.AddSubTask(t)
	}

	// 2. Process batch: Create new if not exists; append aggregator if exists (avoid duplication)
	existingBatch, exists := tm.batches[taskKey]
	if !exists {
		// Create new batch: Initialize aggregator list and normal task list
		tm.batches[taskKey] = &TaskBatch{
			Aggregators: []Aggregator{aggregator},
			NormalTasks: make([]*MessageTask, 0),
			Callback:    callback,
			CreateTime:  time.Now(),
		}
		logx.Debugf("Created new aggregate batch: taskKey=%s, aggregatorParentKey=%s, subtaskCount=%d", taskKey, aggregator.ParentKey(), len(tasks))
		return
	}

	// 3. Batch exists: Check if aggregator already exists (unique by ParentKey)
	aggExists := false
	targetParentKey := aggregator.ParentKey()
	for _, a := range existingBatch.Aggregators {
		if a.ParentKey() == targetParentKey {
			aggExists = true
			break
		}
	}
	// Add only new aggregators
	if !aggExists {
		existingBatch.Aggregators = append(existingBatch.Aggregators, aggregator)
		logx.Debugf("Added new aggregator to batch: taskKey=%s, aggregatorParentKey=%s, newAggregatorCount=%d", taskKey, targetParentKey, len(existingBatch.Aggregators))
	}

	// 4. Callback logic: Override if empty; keep if exists (avoid overwriting business callbacks)
	if existingBatch.Callback == nil && callback != nil {
		existingBatch.Callback = callback
		logx.Debugf("Set callback for existing batch: taskKey=%s (callback was nil)", taskKey)
	}
}

// AddBatch adds non-aggregated tasks to a taskKey
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask, callback BatchCallback) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 1. Mark subtasks to "not skip individual caching"
	for _, t := range tasks {
		t.SkipSubCache = false
	}

	// 2. Process batch: Create new if not exists; append tasks if exists
	existingBatch, exists := tm.batches[taskKey]
	if exists {
		existingBatch.NormalTasks = append(existingBatch.NormalTasks, tasks...)
		logx.Debugf("Appended tasks to existing normal batch: taskKey=%s, originalTaskCount=%d, newTaskCount=%d", taskKey, len(existingBatch.NormalTasks)-len(tasks), len(existingBatch.NormalTasks))
		// Callback logic: Override if empty; keep if exists
		if existingBatch.Callback == nil && callback != nil {
			existingBatch.Callback = callback
			logx.Debugf("Set callback for existing normal batch: taskKey=%s (callback was nil)", taskKey)
		}
		return
	}

	// 3. Create new non-aggregated batch
	tm.batches[taskKey] = &TaskBatch{
		Aggregators: make([]Aggregator, 0),
		NormalTasks: tasks,
		Callback:    callback,
		CreateTime:  time.Now(),
	}
	logx.Debugf("Created new normal batch: taskKey=%s, taskCount=%d", taskKey, len(tasks))
}

// GetBatch retrieves the batch corresponding to a taskKey
func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, ok := tm.batches[taskKey]
	if !ok {
		logx.Debugf("Batch not found when GetBatch called: taskKey=%s", taskKey)
	} else {
		logx.Debugf("Retrieved batch: taskKey=%s, aggregatorCount=%d, normalTaskCount=%d", taskKey, len(batch.Aggregators), len(batch.NormalTasks))
	}
	return batch, ok
}

// SaveProtoToRedis saves a Protobuf message to Redis (with TTL)
func SaveProtoToRedis(ctx context.Context, redisClient redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Errorf("Protobuf marshaling failed for key [%s]: %w", key, err)
	}
	if err := redisClient.Set(ctx, key, data, ttl).Err(); err != nil {
		return fmt.Errorf("Redis Set failed for key [%s]: %w", key, err)
	}
	logx.Debugf("Saved Protobuf to Redis: key=%s, ttl=%s, messageType=%s", key, ttl, msg.ProtoReflect().Descriptor().FullName())
	return nil
}

// ProcessBatch Core logic: Wait for all tasks under a taskKey to complete, then process aggregation and callback
func (tm *TaskManager) ProcessBatch(taskKey string, redisClient redis.Cmdable) {
	ctx, cancel := context.WithTimeout(context.Background(), config.AppConfig.Timeouts.LoginTotalTimeout)
	defer cancel()
	// 1. Verify if batch exists
	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Errorf("Failed to process batch: Batch does not exist (taskKey=%s)", taskKey)
		return
	}

	allSuccess := true                       // Execution result of the entire taskKey batch
	var batchErr error                       // Batch-level error
	taskMap := make(map[string]*MessageTask) // Mapping of all tasks: taskID -> Task
	taskResultKeys := make([]string, 0)      // Redis result keys for all tasks (for batch waiting)

	// 2. Collect all tasks under this taskKey (subtasks from all aggregators + normal tasks)
	// 2.1 Collect subtasks from all aggregators
	for _, aggregator := range batch.Aggregators {
		for _, task := range aggregator.GetSubTasks() {
			taskID := task.TaskID
			taskMap[taskID] = task
			resultKey := fmt.Sprintf("task:result:%s", taskID)
			taskResultKeys = append(taskResultKeys, resultKey)
			logx.Debugf("Collected aggregated subtask: taskKey=%s, taskID=%s, resultKey=%s, redisKey=%s", taskKey, taskID, resultKey, task.RedisKey)
		}
	}
	// 2.2 Collect normal tasks
	for _, task := range batch.NormalTasks {
		taskID := task.TaskID
		taskMap[taskID] = task
		resultKey := fmt.Sprintf("task:result:%s", taskID)
		taskResultKeys = append(taskResultKeys, resultKey)
		logx.Debugf("Collected normal task: taskKey=%s, taskID=%s, resultKey=%s, redisKey=%s", taskKey, taskID, resultKey, task.RedisKey)
	}

	// Log warning if no tasks are collected (abnormal scenario)
	if len(taskMap) == 0 {
		logx.Errorf("No tasks found in batch: taskKey=%s (aggregatorCount=%d, normalTaskCount=%d) - skipping processing", taskKey, len(batch.Aggregators), len(batch.NormalTasks))
		tm.cleanupAndCallback(taskKey, batch, allSuccess, fmt.Errorf("no tasks in batch"), redisClient, taskResultKeys)
		return
	}

	// 3. Wait for all tasks under this taskKey to complete
	completedTaskIDs := make(map[string]bool) // IDs of completed tasks
	logx.Infof("Start waiting for batch tasks: taskKey=%s, totalTaskCount=%d, resultKeyCount=%d", taskKey, len(taskMap), len(taskResultKeys))

	for len(completedTaskIDs) < len(taskMap) {
		select {
		case <-ctx.Done():
			// Timeout: Mark all incomplete tasks as failed
			batchErr = fmt.Errorf("batch task waiting timed out (taskKey=%s, completedTaskCount=%d, totalTaskCount=%d)", taskKey, len(completedTaskIDs), len(taskMap))
			logx.Error(batchErr)
			allSuccess = false
			// Mark incomplete tasks
			for taskID := range taskMap {
				if !completedTaskIDs[taskID] {
					task := taskMap[taskID]
					task.Status = TaskStatusFailed
					task.Error = batchErr
					logx.Errorf("Marked incomplete task as failed (timeout): taskKey=%s, taskID=%s, redisKey=%s", taskKey, taskID, task.RedisKey)
				}
			}
			tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
			return

		default:
			// BLPop blocks to wait for any task result (1s timeout to avoid infinite loop)
			result, err := redisClient.BLPop(ctx, 1*time.Second, taskResultKeys...).Result()
			if err != nil {
				if errors.Is(err, redis.Nil) {
					// No result yet, continue waiting (log periodically to track pending tasks)
					if time.Since(batch.CreateTime)%10 == 0 {
						logx.Debugf("Still waiting for batch tasks (no result yet): taskKey=%s, completedTaskCount=%d, pendingTaskCount=%d", taskKey, len(completedTaskIDs), len(taskMap)-len(completedTaskIDs))
					}
					continue
				}
				// Redis error: Mark all incomplete tasks as failed
				batchErr = fmt.Errorf("Redis BLPop failed for batch (taskKey=%s): %w", taskKey, err)
				logx.Error(batchErr)
				allSuccess = false
				// Mark incomplete tasks
				for taskID := range taskMap {
					if !completedTaskIDs[taskID] {
						task := taskMap[taskID]
						task.Status = TaskStatusFailed
						task.Error = batchErr
						logx.Errorf("Marked incomplete task as failed (Redis error): taskKey=%s, taskID=%s, redisKey=%s", taskKey, taskID, task.RedisKey)
					}
				}
				tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
				return
			}

			// Parse task result
			if len(result) != 2 {
				logx.Errorf("Invalid BLPop result format for batch: taskKey=%s, result=%v (expected [key, value])", taskKey, result)
				continue
			}
			resKey, resVal := result[0], result[1]
			// Extract taskID from resKey (format: task:result:{taskID})
			var taskID string
			if _, err := fmt.Sscanf(resKey, "task:result:%s", &taskID); err != nil {
				logx.Errorf("Failed to parse taskID from result key: taskKey=%s, resKey=%s, err=%v", taskKey, resKey, err)
				continue
			}
			task, ok := taskMap[taskID]
			if !ok {
				logx.Errorf("Received result for unknown task: taskKey=%s, taskID=%s, resKey=%s (may be a stale key)", taskKey, taskID, resKey)
				continue
			}

			// Mark task as completed
			completedTaskIDs[taskID] = true
			logx.Debugf("Received result for task: taskKey=%s, taskID=%s, resKey=%s, completedCount=%d/%d", taskKey, taskID, resKey, len(completedTaskIDs), len(taskMap))

			// 4. Process individual task result
			var taskResult login_proto.TaskResult
			if err := proto.Unmarshal([]byte(resVal), &taskResult); err != nil {
				err = fmt.Errorf("task result unmarshaling failed: taskKey=%s, taskID=%s, err=%w", taskKey, taskID, err)
				logx.Error(err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}
			if !taskResult.Success {
				err = fmt.Errorf("task execution failed: taskKey=%s, taskID=%s, error=%s", taskKey, taskID, taskResult.Error)
				logx.Error(err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}
			// Unmarshal Protobuf message
			if err := proto.Unmarshal(taskResult.Data, task.Message); err != nil {
				err = fmt.Errorf("task message unmarshaling failed: taskKey=%s, taskID=%s, err=%w", taskKey, taskID, err)
				logx.Error(err)
				task.Status = TaskStatusFailed
				task.Error = err
				allSuccess = false
				continue
			}

			// 5. Cache non-aggregated tasks; skip aggregated subtasks
			if !task.SkipSubCache {
				if err := SaveProtoToRedis(
					ctx,
					redisClient,
					task.RedisKey,
					task.Message,
					config.AppConfig.Timeouts.RoleCacheExpire,
				); err != nil {
					err = fmt.Errorf("task result caching failed: taskKey=%s, taskID=%s, redisKey=%s, err=%w", taskKey, taskID, task.RedisKey, err)
					logx.Error(err)
					task.Status = TaskStatusFailed
					task.Error = err
					allSuccess = false
					continue
				}
				logx.Debugf("Cached non-aggregated task result: taskKey=%s, taskID=%s, redisKey=%s", taskKey, taskID, task.RedisKey)
			}

			task.Status = TaskStatusDone
			logx.Infof("Task processed successfully: taskKey=%s, taskID=%s, redisKey=%s", taskKey, taskID, task.RedisKey)
		}
	} // End of "wait for all tasks" loop

	// 6. After all tasks complete, process aggregation for each aggregator (only if all succeeded)
	if allSuccess && len(batch.Aggregators) > 0 {
		for _, aggregator := range batch.Aggregators {
			logx.Infof("Starting aggregation for batch: taskKey=%s, aggregatorKey=%s, subtaskCount=%d", taskKey, aggregator.ParentKey(), len(aggregator.GetSubTasks()))
			// Aggregator uses its own subtasks for aggregation (ensured to process only its own)
			parentPB, err := aggregator.Aggregate()
			if err != nil {
				batchErr = fmt.Errorf("aggregation failed: taskKey=%s, aggregatorKey=%s, err=%w", taskKey, aggregator.ParentKey(), err)
				allSuccess = false
				logx.Errorf(batchErr.Error())
				continue
			}

			// Cache aggregated parent message to Redis
			if err := SaveProtoToRedis(
				ctx,
				redisClient,
				aggregator.ParentKey(),
				parentPB,
				config.AppConfig.Timeouts.RoleCacheExpire,
			); err != nil {
				batchErr = fmt.Errorf("aggregated result caching failed: taskKey=%s, aggregatorKey=%s, err=%w", taskKey, aggregator.ParentKey(), err)
				allSuccess = false
				logx.Errorf(batchErr.Error())
				continue
			}

			logx.Infof("Aggregation completed successfully: taskKey=%s, aggregatorKey=%s (cached to Redis)", taskKey, aggregator.ParentKey())
		}
	}

	// 7. Cleanup resources and trigger batch callback
	tm.cleanupAndCallback(taskKey, batch, allSuccess, batchErr, redisClient, taskResultKeys)
} // End of ProcessBatch function

// cleanupAndCallback cleans up Redis task result keys, deletes the batch, and triggers the callback
func (tm *TaskManager) cleanupAndCallback(
	taskKey string,
	batch *TaskBatch,
	allSuccess bool,
	err error,
	redisClient redis.Cmdable,
	taskResultKeys []string,
) {
	// 7.1 Cleanup task result keys in Redis (prevent reprocessing)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	if len(taskResultKeys) > 0 {
		delResult, delErr := redisClient.Del(ctx, taskResultKeys...).Result()
		if delErr != nil {
			logx.Errorf("Failed to delete task result keys: taskKey=%s, keyCount=%d, keys=%v, err=%v", taskKey, len(taskResultKeys), taskResultKeys, delErr)
		} else {
			if delResult != int64(len(taskResultKeys)) {
				logx.Errorf("Mismatch in deleted result keys: taskKey=%s, expected=%d, actual=%d, keys=%v", taskKey, len(taskResultKeys), delResult, taskResultKeys)
			} else {
				logx.Debugf("Successfully deleted task result keys: taskKey=%s, count=%d", taskKey, delResult)
			}
		}
	} else {
		logx.Debugf("No task result keys to delete: taskKey=%s", taskKey)
	}

	// 7.2 Remove current batch from manager (free memory)
	tm.mu.Lock()
	delete(tm.batches, taskKey)
	tm.mu.Unlock()
	logx.Debugf("Removed batch from manager: taskKey=%s", taskKey)

	// 7.3 Trigger batch callback (run in separate goroutine to avoid blocking; recover from panics)
	if batch.Callback != nil {
		defer func() {
			if r := recover(); r != nil {
				logx.Errorf("Batch callback panicked: taskKey=%s, panic=%v", taskKey, r)
			}
		}()
		batch.Callback(taskKey, allSuccess, err)
		logx.Infof("Batch callback executed: taskKey=%s, allSuccess=%v", taskKey, allSuccess)
	} else {
		logx.Debugf("No callback defined for batch: taskKey=%s, allSuccess=%v", taskKey, allSuccess)
	}
}

// InitTaskOptions contains options for task initialization (original structure retained)
type InitTaskOptions struct {
	Aggregator Aggregator
	Callback   BatchCallback
}

// InitAndAddMessageTasks initializes and adds message tasks (adapts to multi-aggregator logic)
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

	// 1. Batch query cache (original logic unchanged)
	keys := make([]string, len(messages))
	msgMap := make(map[string]proto.Message, len(messages))
	for i, msg := range messages {
		key := cache.BuildRedisKey(msg, playerIdStr)
		keys[i] = key
		msgMap[key] = msg
	}
	logx.Debugf("Initiating batch cache query: taskKey=%s, playerId=%d, keyCount=%d, keys=%v", taskKey, playerId, len(keys), keys)

	values, err := redisClient.MGet(ctx, keys...).Result()
	if err != nil {
		return fmt.Errorf("batch cache query failed: taskKey=%s, playerId=%d, err=%w", taskKey, playerId, err)
	}

	// 2. Collect tasks with cache misses (original logic unchanged)
	cacheHitCount := 0
	for i, val := range values {
		if val != nil {
			cacheHitCount++
			continue
		}
		key := keys[i]
		msg := msgMap[key]
		msgType := msg.ProtoReflect().Descriptor().FullName()

		// Serialize message
		data, err := proto.Marshal(msg)
		if err != nil {
			return fmt.Errorf("message marshaling failed: taskKey=%s, playerId=%d, msgType=%s, err=%w", taskKey, playerId, msgType, err)
		}

		// Create task ID and task instance
		taskID := uuid.NewString()
		dbTasks = append(dbTasks, &login_proto.DBTask{
			Key:       playerId,
			WhereCase: "player_id='" + playerIdStr + "'",
			Op:        "read",
			MsgType:   string(msgType),
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
		logx.Debugf("Created task for cache miss: taskKey=%s, taskID=%s, redisKey=%s, msgType=%s", taskKey, taskID, key, msgType)
	}

	logx.Infof("Batch cache query result: taskKey=%s, playerId=%d, total=%d, hits=%d, misses=%d", taskKey, playerId, len(messages), cacheHitCount, len(tasks))

	// 3. Batch deliver to Kafka (original logic unchanged)
	if len(dbTasks) > 0 {
		if len(dbTasks) == 1 {
			if err := producer.SendTask(ctx, dbTasks[0], playerIdStr); err != nil {
				return fmt.Errorf("Kafka single task delivery failed: taskKey=%s, taskID=%s, err=%w", taskKey, dbTasks[0].TaskId, err)
			}
			logx.Debugf("Delivered single task to Kafka: taskKey=%s, taskID=%s, playerId=%d", taskKey, dbTasks[0].TaskId, playerId)
		} else {
			if err := producer.SendTasks(ctx, dbTasks, playerIdStr); err != nil {
				return fmt.Errorf("Kafka batch task delivery failed: taskKey=%s, taskCount=%d, err=%w", taskKey, len(dbTasks), err)
			}
			logx.Debugf("Delivered batch tasks to Kafka: taskKey=%s, taskCount=%d, playerId=%d", taskKey, len(dbTasks), playerId)
		}
		logx.Infof("Kafka delivery completed: taskKey=%s, totalTasks=%d, playerId=%d", taskKey, len(dbTasks), playerId)
	}

	// 4. Add tasks to batch (adapt to multi-aggregator: use aggregate batch if aggregator exists, else normal batch)
	if len(tasks) > 0 {
		manager := executor.GetTaskManagerByKey(taskKey)
		if options.Aggregator != nil {
			manager.AddAggregateBatch(taskKey, tasks, options.Aggregator, options.Callback)
			logx.Infof("Added aggregated tasks to batch: taskKey=%s, taskCount=%d, aggregatorKey=%s", taskKey, len(tasks), options.Aggregator.ParentKey())
		} else {
			manager.AddBatch(taskKey, tasks, options.Callback)
			logx.Infof("Added normal tasks to batch: taskKey=%s, taskCount=%d", taskKey, len(tasks))
		}
	} else {
		logx.Debugf("No tasks to add to batch (all cache hits): taskKey=%s, playerId=%d", taskKey, playerId)
		// Trigger callback immediately if no tasks (all cache hits)
		if options.Callback != nil {
			go options.Callback(taskKey, true, nil)
			logx.Debugf("Triggered immediate callback (all cache hits): taskKey=%s", taskKey)
		}
	}

	return nil
}
