package node_id_etcd

import (
	"context"
	"fmt"
	"log"
	"testing"
	"time"
)

// TestGenerateID 测试ID生成器的功能
func TestGenerateID(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := initEtcdClient()
	if err != nil {
		t.Fatalf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	err = clearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	// 测试多次生成 ID
	for i := 0; i < 10; i++ {
		// 生成新的 ID
		id, err := generateID(ctx, etcdClient)
		if err != nil {
			t.Errorf("Failed to generate ID: %v", err)
			continue
		}

		// 打印生成的 ID
		fmt.Println("Generated ID:", id)

		// 检查 ID 是否符合预期的格式
		if !isValidID(id) {
			t.Errorf("Generated ID %d is invalid", id)
		}
	}

	err = clearAllIDs(etcdClient)

	// 测试 ID 达到最大值后是否重置为 0
	for i := 0; i < int(maxID); i++ {
		id, err := generateID(ctx, etcdClient)
		if err != nil {
			t.Errorf("Failed to generate ID: %v", err)
			continue
		}

		// 期待生成的ID应该是自增的
		if uint64(i) != id {
			t.Errorf("ID not incremented properly: expected %d, got %d", i+1, id)
		}
	}

	// 测试 ID 是否在最大值后重置
	id, err := generateID(ctx, etcdClient)
	if err != nil {
		t.Fatalf("Failed to generate ID after max ID: %v", err)
	}

	// 期望重置为0
	if id != 0 {
		t.Errorf("ID should have been reset to 0, got: %d", id)
	}
}

// TestReleaseID 测试释放ID的功能
func TestReleaseID(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := initEtcdClient()
	if err != nil {
		t.Fatalf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	// 创建 context
	ctx := context.Background()

	// 生成新的 ID
	id, err := generateID(ctx, etcdClient)
	if err != nil {
		t.Fatalf("Failed to generate ID: %v", err)
	}

	// 释放生成的 ID
	err = releaseID(ctx, etcdClient, id)
	if err != nil {
		t.Errorf("Failed to release ID %d: %v", id, err)
	}

	// 检查ID是否已从Etcd中删除
	resp, err := etcdClient.Get(ctx, "ids/"+fmt.Sprintf("%d", id))
	if err != nil {
		t.Fatalf("Failed to get ID %d from Etcd: %v", id, err)
	}

	if len(resp.Kvs) > 0 {
		t.Errorf("ID %d should have been released, but it's still present in Etcd", id)
	} else {
		log.Printf("ID %d successfully released", id)
	}
}

// TestSweepExpiredIDs 测试清理过期ID的功能
func TestSweepExpiredIDs(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := initEtcdClient()
	if err != nil {
		t.Fatalf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	err = clearAllIDs(etcdClient)

	// 创建 context
	ctx := context.Background()

	// 创建一个租约，模拟过期的ID
	_, err = etcdClient.Grant(ctx, 1) // 设置 TTL 为1秒
	if err != nil {
		t.Fatalf("Failed to create lease: %v", err)
	}

	// 使用租约创建 ID
	id, err := generateID(ctx, etcdClient)
	if err != nil {
		t.Fatalf("Failed to generate ID with lease: %v", err)
	}

	// 等待1秒钟，确保租约过期
	time.Sleep(2 * time.Second)

	// 测试清理过期ID
	sweepExpiredIDs(etcdClient)

	// 检查 ID 是否被清理
	resp, err := etcdClient.Get(ctx, "ids/"+fmt.Sprintf("%d", id))
	if err != nil {
		t.Fatalf("Failed to get ID %d from Etcd: %v", id, err)
	}

	if len(resp.Kvs) > 0 {
		t.Errorf("Expired ID %d should have been cleaned, but it's still present in Etcd", id)
	} else {
		log.Printf("Expired ID %d successfully cleaned", id)
	}
}

// isValidID 检查ID的格式是否合法
func isValidID(num uint64) bool {
	// 检查数字是否在合理范围内
	return num >= 0 && num <= maxID
}

func TestGenerateIDClear(t *testing.T) {
	// 初始化 Etcd 客户端
	etcdClient, err := initEtcdClient()
	if err != nil {
		t.Fatalf("Error initializing Etcd client: %v", err)
	}
	defer etcdClient.Close()

	// 清除所有 ID
	err = clearAllIDs(etcdClient)
	if err != nil {
		t.Fatalf("Error clearing Etcd IDs: %v", err)
	}

	// 创建 context
	ctx := context.Background()

	// 测试生成 ID
	_, err = generateID(ctx, etcdClient)
	if err != nil {
		t.Fatalf("Error generating ID: %v", err)
	}

	// 继续你的其他测试逻辑...
}
