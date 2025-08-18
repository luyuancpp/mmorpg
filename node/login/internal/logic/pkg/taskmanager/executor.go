package taskmanager

import (
	"bytes"
	"hash/fnv"
	"runtime"
	"strconv"
	"sync"

	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// KeyBasedTaskExecutor 基于key分配的任务执行器
type KeyBasedTaskExecutor struct {
	subPools  []*ants.Pool // 子池列表
	poolCount uint32       // 子池数量
	taskMgr   *TaskManager
	redis     redis.Cmdable
	mu        sync.RWMutex // 保护子池操作的锁
}

// NewKeyBasedTaskExecutor 创建基于key的任务执行器
// subPoolCount: 子池数量（建议为2的幂次方，如8、16）
// workerPerPool: 每个子池的worker数量
func NewKeyBasedTaskExecutor(subPoolCount uint32, workerPerPool int, taskMgr *TaskManager, redis redis.Cmdable) (*KeyBasedTaskExecutor, error) {
	executor := &KeyBasedTaskExecutor{
		subPools:  make([]*ants.Pool, subPoolCount),
		poolCount: subPoolCount,
		taskMgr:   taskMgr,
		redis:     redis,
	}

	// 初始化所有子池
	for i := uint32(0); i < subPoolCount; i++ {
		pool, err := ants.NewPool(workerPerPool)
		if err != nil {
			// 初始化失败时释放已创建的子池
			executor.Release()
			return nil, err
		}
		executor.subPools[i] = pool
	}

	return executor, nil
}

// SubmitTask 提交任务，根据taskKey分配到固定子池
func (k *KeyBasedTaskExecutor) SubmitTask(taskKey string) error {
	// 计算key的哈希值，映射到对应的子池
	poolIndex := uint32(k.hashKey(taskKey) % k.poolCount)

	k.mu.RLock()
	defer k.mu.RUnlock()

	// 提交任务到对应的子池
	return k.subPools[poolIndex].Submit(func() {
		logx.Infof("Processing task: %s (pool: %d)", taskKey, poolIndex)
		k.taskMgr.ProcessBatch(taskKey, k.redis)
		logx.Infof("Finished processing task: %s (pool: %d)", taskKey, poolIndex)
	})
}

// Release 释放所有子池资源
func (k *KeyBasedTaskExecutor) Release() {
	k.mu.Lock()
	defer k.mu.Unlock()

	for _, pool := range k.subPools {
		if pool != nil {
			pool.Release()
		}
	}
	k.subPools = nil
}

// hashKey 计算taskKey的哈希值
func (k *KeyBasedTaskExecutor) hashKey(key string) uint32 {
	h := fnv.New32a()
	h.Write([]byte(key))
	return h.Sum32()
}

// getGoroutineID 获取当前 goroutine ID（仅用于示例，生产环境谨慎使用）
func getGoroutineID() uint64 {
	b := make([]byte, 64)
	b = b[:runtime.Stack(b, false)]
	b = bytes.TrimPrefix(b, []byte("goroutine "))
	b = b[:bytes.IndexByte(b, ' ')]
	id, _ := strconv.ParseUint(string(b), 10, 64)
	return id
}
