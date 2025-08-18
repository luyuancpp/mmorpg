package taskmanager

import (
	"context"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"login/internal/config"
	"login/pb/taskpb"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// 任务状态常量
const (
	TaskStatusPending = "pending"
	TaskStatusFailed  = "failed"
	TaskStatusDone    = "done"
)

// Aggregator 聚合器接口：定义子PB如何聚合到父PB
type Aggregator interface {
	// 聚合逻辑：将子任务的PB填充到父PB中
	Aggregate(subTasks []*MessageTask) (parentPB proto.Message, err error)
	// 父PB的Redis缓存Key
	ParentKey() string
}

// GenericAggregator 通用聚合器：自动将子PB按类型填充到父PB的对应字段
type GenericAggregator struct {
	parentTemplate proto.Message // 父PB的模板实例（如&PlayerFullData{}）
	parentKey      string        // 父PB的Redis缓存Key
}

// NewGenericAggregator 创建通用聚合器
func NewGenericAggregator(parentTemplate proto.Message, parentKey string) *GenericAggregator {
	return &GenericAggregator{
		parentTemplate: parentTemplate,
		parentKey:      parentKey,
	}
}

// Aggregate 实现Aggregator接口：自动匹配子PB类型与父PB字段并填充
func (g *GenericAggregator) Aggregate(subTasks []*MessageTask) (proto.Message, error) {
	// 基于模板创建父PB实例
	parent := proto.Clone(g.parentTemplate)
	parentReflect := parent.ProtoReflect()
	parentDesc := parentReflect.Descriptor()
	fields := parentDesc.Fields() // 获取父PB所有字段描述

	// 遍历所有子任务，按类型填充到父PB
	for _, task := range subTasks {
		subPB := task.Message
		if subPB == nil {
			continue
		}

		// 获取子PB的类型名称（如"PlayerBag"）
		subDesc := subPB.ProtoReflect().Descriptor()
		subTypeName := string(subDesc.Name())

		// 遍历父PB的字段（使用索引遍历，兼容所有版本）
		found := false
		for i := 0; i < fields.Len(); i++ {
			field := fields.Get(i) // 通过索引获取字段描述

			// 检查字段是否为消息类型，且类型名称与子PB一致
			if field.Kind() == protoreflect.MessageKind &&
				string(field.Message().Name()) == subTypeName {

				// 将子PB设置到父PB的对应字段
				parentReflect.Set(field, protoreflect.ValueOf(subPB.ProtoReflect()))
				found = true
				break // 找到匹配字段，退出循环
			}
		}

		if !found {
			return nil, fmt.Errorf("父PB中未找到与子PB类型匹配的字段: %s", subTypeName)
		}
	}

	return parent, nil
}

// ParentKey 实现Aggregator接口：返回父PB的缓存Key
func (g *GenericAggregator) ParentKey() string {
	return g.parentKey
}

// MessageTask 单个任务的信息
type MessageTask struct {
	TaskID       string
	Message      proto.Message // 子PB数据
	RedisKey     string        // 子PB的缓存Key（聚合任务中可能不用）
	PlayerID     uint64
	Status       string
	Error        error
	SkipSubCache bool // 是否跳过子PB的缓存（聚合任务中为true）
}

// TaskBatch 任务批次
type TaskBatch struct {
	Tasks        []*MessageTask
	Aggregator   Aggregator // 聚合器（非nil表示聚合任务）
	skipSubCache bool       // 是否所有子任务都跳过缓存
}

// TaskManager 任务管理器
type TaskManager struct {
	mu      sync.RWMutex
	batches map[string]*TaskBatch // 任务批次：key为taskKey
}

// NewTaskManager 创建任务管理器实例
func NewTaskManager() *TaskManager {
	return &TaskManager{
		batches: make(map[string]*TaskBatch),
	}
}

// AddAggregateBatch 添加聚合任务批次（自动标记子任务跳过缓存）
func (tm *TaskManager) AddAggregateBatch(taskKey string, tasks []*MessageTask, aggregator Aggregator) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 为所有子任务标记跳过子缓存
	for _, task := range tasks {
		task.SkipSubCache = true
	}

	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   aggregator,
		skipSubCache: true,
	}
}

// AddBatch 添加普通任务批次（子任务需要单独缓存）
func (tm *TaskManager) AddBatch(taskKey string, tasks []*MessageTask) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 普通任务不跳过子缓存
	for _, task := range tasks {
		task.SkipSubCache = false
	}

	tm.batches[taskKey] = &TaskBatch{
		Tasks:        tasks,
		Aggregator:   nil,
		skipSubCache: false,
	}
}

// GetBatch 获取任务批次
func (tm *TaskManager) GetBatch(taskKey string) (*TaskBatch, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	batch, exists := tm.batches[taskKey]
	return batch, exists
}

// SaveProtoToRedis 将protobuf消息保存到Redis
func SaveProtoToRedis(redis redis.Cmdable, key string, msg proto.Message, ttl time.Duration) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Errorf("Marshal proto to Redis error: %v", err)
		return err
	}
	return redis.Set(context.Background(), key, data, ttl).Err()
}

// WaitForTaskResult 等待任务结果（从Redis阻塞获取）
func WaitForTaskResult(redisClient redis.Cmdable, key string, timeout time.Duration) ([]byte, error) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

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

// ProcessBatch 处理任务批次
func (tm *TaskManager) ProcessBatch(taskKey string, redisClient redis.Cmdable) {
	logx.Infof("Start processing task batch for key: %s", taskKey)

	batch, exists := tm.GetBatch(taskKey)
	if !exists {
		logx.Infof("No task batch found for key: %s", taskKey)
		return
	}

	allSuccess := true

	// 处理所有子任务
	for _, task := range batch.Tasks {
		if task.Status != TaskStatusPending {
			logx.Infof("Skipping task %s: status is %s", task.TaskID, task.Status)
			continue
		}

		logx.Infof("Waiting for task result: %s", task.TaskID)
		resBytes, err := WaitForTaskResult(
			redisClient,
			task.TaskID,
			time.Duration(config.AppConfig.Timeouts.TaskWaitTimeoutSec)*time.Second,
		)
		if err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Task %s failed while waiting: %v", task.TaskID, err)
			allSuccess = false
			continue
		}

		// 解析任务结果
		var result taskpb.TaskResult
		if err := proto.Unmarshal(resBytes, &result); err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Failed to unmarshal TaskResult for task %s: %v", task.TaskID, err)
			allSuccess = false
			continue
		}

		if !result.Success {
			err := fmt.Errorf("task %s failed: %s", task.TaskID, result.Error)
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Task %s failed with error: %s", task.TaskID, result.Error)
			allSuccess = false
			continue
		}

		// 解析子PB数据
		if err := proto.Unmarshal(result.Data, task.Message); err != nil {
			task.Status = TaskStatusFailed
			task.Error = err
			logx.Errorf("Failed to unmarshal Message for task %s: %v", task.TaskID, err)
			allSuccess = false
			continue
		}

		// 仅普通任务缓存子PB，聚合任务跳过
		if !task.SkipSubCache {
			if err := SaveProtoToRedis(
				redisClient,
				task.RedisKey,
				task.Message,
				time.Duration(config.AppConfig.Timeouts.RoleCacheExpireHours)*time.Hour,
			); err != nil {
				task.Status = TaskStatusFailed
				task.Error = err
				logx.Errorf("Failed to save message to Redis for task %s: %v", task.TaskID, err)
				allSuccess = false
				continue
			}
			logx.Infof("Task %s saved to Redis (key: %s)", task.TaskID, task.RedisKey)
		}

		task.Status = TaskStatusDone
		logx.Infof("Task %s processed successfully", task.TaskID)
	}

	// 聚合任务：所有子任务成功后生成并缓存父PB
	if batch.Aggregator != nil && allSuccess {
		logx.Infof("Start aggregating sub-PBs for taskKey: %s", taskKey)

		parentPB, err := batch.Aggregator.Aggregate(batch.Tasks)
		if err != nil {
			logx.Errorf("Aggregation failed for taskKey %s: %v", taskKey, err)
		} else {
			parentKey := batch.Aggregator.ParentKey()
			if err := SaveProtoToRedis(
				redisClient,
				parentKey,
				parentPB,
				time.Duration(config.AppConfig.Timeouts.RoleCacheExpireHours)*time.Hour,
			); err != nil {
				logx.Errorf("Failed to save aggregated PB to Redis (key: %s): %v", parentKey, err)
			} else {
				logx.Infof("Aggregated PB saved to Redis successfully (key: %s)", parentKey)
			}
		}
	}

	logx.Infof("Finished processing task batch for key: %s", taskKey)

	// 清理资源
	tm.mu.Lock()
	delete(tm.batches, taskKey)
	tm.mu.Unlock()
}
