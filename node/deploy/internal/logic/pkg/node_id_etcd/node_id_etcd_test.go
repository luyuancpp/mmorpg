package node_id_etcd

import (
	"context"
	"fmt"
	"github.com/zeromicro/go-zero/core/logx"
	"sync"
	"testing"
	"time"
)

const (
	nodeType = uint32(1)
)

// TestGenerateID_Success 测试ID生成器的功能
func TestGenerateID_Success(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		logx.Errorf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	err = ClearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	// 测试多次生成 ID
	for i := 0; i < 10; i++ {
		// 生成新的 ID
		id, err := GenerateID(ctx, etcdClient, nodeType)
		if err != nil {
			logx.Error("Failed to generate ID: ", err)
			continue
		}

		// 打印生成的 ID
		logx.Info("Generated ID:", id)

		// 检查 ID 是否符合预期的格式
		if !isValidID(id) {
			logx.Error("Generated ID ", id, " is invalid")
		}
	}

	err = ClearAllIDs(etcdClient)

	// 测试 ID 达到最大值后是否重置为 0
	for i := 0; i < maxID; i++ {
		id, err := GenerateID(ctx, etcdClient, nodeType)
		if err != nil {
			logx.Error("Failed to generate ID: ", err)
			continue
		}

		// 期待生成的ID应该是自增的
		if uint64(i) != id {
			logx.Error("ID not incremented properly: expected ", i, ", got ", id)
		}
	}

	// 测试 ID 是否在最大值后重置
	_, err = GenerateID(ctx, etcdClient, nodeType)
	if err != nil {
		logx.Info("Failed to generate ID after max ID: ", err)
	}
}

// TestReleaseID_Success 测试释放ID的功能
func TestReleaseID_Success(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		logx.Error("Error initializing Etcd client: ", err)
	}
	defer etcdClient.Close()

	err = ClearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	// 生成新的 ID
	id, err := GenerateID(ctx, etcdClient, nodeType)
	if err != nil {
		logx.Error("Failed to generate ID: ", err)
	}

	// 释放生成的 ID
	err = ReleaseID(ctx, etcdClient, id, nodeType)
	if err != nil {
		logx.Error("Failed to release ID ", id, ": ", err)
	}

	// 检查ID是否已从Etcd中删除
	resp, err := etcdClient.Get(ctx, "ids/"+fmt.Sprintf("%d", id))
	if err != nil {
		logx.Error("Failed to get ID ", id, " from Etcd: ", err)
	}

	if len(resp.Kvs) > 0 {
		logx.Error("ID ", id, " should have been released, but it's still present in Etcd")
	} else {
		logx.Info("ID ", id, " successfully released")
	}
}

// TestSweepExpiredIDs_Success 测试清理过期ID的功能
func TestSweepExpiredIDs_Success(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		t.Fatalf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	// 清理所有 ID
	err = ClearAllIDs(etcdClient)
	if err != nil {
		t.Fatalf("Failed to clear all IDs: %v", err)
	}

	// 创建 context
	ctx := context.Background()

	// 创建一个租约，模拟过期的 ID
	_, err = etcdClient.Grant(ctx, 1) // 设置 TTL 为1秒
	if err != nil {
		t.Fatalf("Failed to create lease: %v", err)
	}

	// 使用租约生成 ID
	id, err := GenerateID(ctx, etcdClient, nodeType)
	if err != nil {
		t.Fatalf("Failed to generate ID with lease: %v", err)
	}

	// 等待1秒钟，确保租约过期
	time.Sleep(2 * time.Second)

	// 执行清理过期 ID 的操作
	SweepExpiredIDs(etcdClient)

	// 检查 ID 是否已被清理
	// 由于回收池是清理过期 ID 的地方，所以检查回收池是否有此 ID
	recycledIDKey := getRecycledIDKey(nodeType)
	resp, err := etcdClient.Get(ctx, recycledIDKey)
	if err != nil {
		t.Fatalf("Failed to get recycled IDs from Etcd: %v", err)
	}

	// 检查回收池中是否有该 ID
	found := false
	for _, kv := range resp.Kvs {
		if string(kv.Value) == fmt.Sprintf("%d", id) {
			found = true
			break
		}
	}

	if found {
		t.Errorf("Expired ID %d should have been cleaned, but it is still in the recycled pool", id)
	} else {
		t.Logf("Expired ID %d successfully cleaned", id)
	}
}

// isValidID 检查ID的格式是否合法
func isValidID(num uint64) bool {
	// 检查数字是否在合理范围内
	return num >= 0 && num <= maxID
}

func TestGenerateIDAndReleaseID_Concurrently_Success(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		logx.Errorf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	err = ClearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	var wg sync.WaitGroup

	// 启动多个 goroutine 来并发调用 GenerateID 和 ReleaseID
	for i := 0; i < 100; i++ {
		wg.Add(2) // 每次启动两个 goroutine，一个调用 GenerateID，另一个调用 ReleaseID
		go func() {
			defer wg.Done()
			// 并发调用 GenerateID
			id, err := GenerateID(ctx, etcdClient, nodeType)
			if err != nil {
				logx.Errorf("Generated ID: %s", err)
				return
			}
			logx.Infof("Generated ID: %d", id)
		}()

		go func() {
			defer wg.Done()
			// 并发调用 ReleaseID
			id := i + 1 // 假设每次 release 的 ID 是按顺序来的
			err := ReleaseID(ctx, etcdClient, uint64(id), nodeType)
			if err != nil {
				logx.Errorf("Released ID: %s", err)
				return
			}
			logx.Infof("Released ID: %d", id)
		}()
	}

	// 等待所有 goroutine 完成
	wg.Wait()
}

func TestGenerateIDAndReleaseID_Concurrently_SequentialRelease(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		logx.Errorf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	// 清理所有 ID 键
	err = ClearAllIDs(etcdClient)
	if err != nil {
		logx.Errorf("Error clearing all IDs: %v", err)
	}

	// 创建 context
	ctx := context.Background()

	var wg sync.WaitGroup
	// 用于存储生成的 ID
	var generatedIDs []uint64
	// 用于同步回收的操作
	var releaseWG sync.WaitGroup

	// 启动多个 goroutine 来并发调用 GenerateID，先生成所有 ID
	for i := 0; i < 100; i++ {
		wg.Add(1) // 每次启动一个 goroutine 调用 GenerateID
		go func() {
			defer wg.Done()

			// 并发调用 GenerateID
			id, err := GenerateID(ctx, etcdClient, nodeType)
			if err != nil {
				logx.Errorf("Generated ID error: %v", err)
				return
			}

			// 将生成的 ID 存储在切片中
			generatedIDs = append(generatedIDs, id)
			logx.Infof("Generated ID: %d", id)
		}()
	}

	// 等待所有的 GenerateID 操作完成
	wg.Wait()

	// 启动多个 goroutine 来并发回收 ID
	for _, id := range generatedIDs {
		releaseWG.Add(1) // 每次启动一个 goroutine 调用 ReleaseID
		go func(id uint64) {
			defer releaseWG.Done()

			// 并发调用 ReleaseID
			err := ReleaseID(ctx, etcdClient, id, nodeType)
			if err != nil {
				logx.Errorf("Released ID error: %v", err)
				return
			}
			logx.Infof("Released ID: %d", id)
		}(id)
	}

	// 等待所有的 ReleaseID 操作完成
	releaseWG.Wait()
}

// TestReleaseAndReuseID_Success 测试释放后的 ID 是否能被重用
func TestReleaseAndReuseID_Success(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := InitEtcdClient()
	if err != nil {
		logx.Errorf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	err = ClearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	// 生成新的 ID
	id, err := GenerateID(ctx, etcdClient, nodeType)
	if err != nil {
		logx.Errorf("Failed to generate ID: %v", err)
	}
	logx.Infof("Generated ID: %d", id)

	// 释放生成的 ID
	err = ReleaseID(ctx, etcdClient, id, nodeType)
	if err != nil {
		logx.Errorf("Failed to release ID %d: %v", id, err)
	}

	// 再次生成 ID
	reusedID, err := GenerateID(ctx, etcdClient, nodeType)
	if err != nil {
		logx.Errorf("Failed to generate ID after release: %v", err)
	}
	logx.Infof("Reused ID: %d", reusedID)

	// 验证生成的 ID 是否与释放的 ID 相同
	if reusedID != id {
		logx.Errorf("Expected reused ID %d, but got %d", id, reusedID)
	} else {
		logx.Infof("ID %d successfully reused", id)
	}
}
