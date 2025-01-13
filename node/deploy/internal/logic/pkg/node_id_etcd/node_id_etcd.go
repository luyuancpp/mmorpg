package node_id_etcd

import (
	"context"
	"fmt"
	"log"
	"time"

	"go.etcd.io/etcd/client/v3"
)

const (
	etcdAddr      = "localhost:2379"  // Etcd 服务地址
	idTTL         = 60 * time.Second  // ID 的 TTL 设置为 60 秒
	idKey         = "node_id_counter" // 用于存储计数器的键
	recycledIDKey = "recycled_ids"    // 用于存储回收的 ID 键
	maxID         = 1000              // 最大 ID 值
)

// 初始化 Etcd 客户端
func initEtcdClient() (*clientv3.Client, error) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints: []string{etcdAddr},
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create etcd client: %v", err)
	}
	return cli, nil
}

// 获取下一个自增的 ID，或者从回收池中获取
func generateID(ctx context.Context, etcdClient *clientv3.Client) (uint64, error) {
	// 先尝试从回收池中获取一个 ID
	resp, err := etcdClient.Get(ctx, recycledIDKey)
	if err != nil {
		// 错误时返回 uint64 最大值
		return ^uint64(0), fmt.Errorf("failed to get recycled IDs: %v", err)
	}

	if len(resp.Kvs) > 0 {
		// 如果回收池中有 ID，直接从回收池中取一个
		recycledID := string(resp.Kvs[0].Value)
		_, err := etcdClient.Delete(ctx, recycledIDKey)
		if err != nil {
			// 错误时返回 uint64 最大值
			return ^uint64(0), fmt.Errorf("failed to delete recycled ID %s: %v", recycledID, err)
		}
		log.Printf("Recycled ID %s", recycledID)
		// 解析回收池的 ID 并返回
		var id uint64
		_, err = fmt.Sscanf(recycledID, "%d", &id)
		if err != nil {
			// 错误时返回 uint64 最大值
			return ^uint64(0), fmt.Errorf("failed to parse recycled ID %s: %v", recycledID, err)
		}
		return id, nil
	}

	// 如果回收池为空，使用自增计数器生成 ID
	txn := etcdClient.Txn(ctx)

	// 获取当前 ID 值并进行自增
	resp, err = etcdClient.Get(ctx, idKey)
	if err != nil {
		// 错误时返回 uint64 最大值
		return ^uint64(0), fmt.Errorf("failed to get current ID: %v", err)
	}

	// 获取当前的计数器值
	var currentID uint64
	if len(resp.Kvs) > 0 {
		_, err = fmt.Sscanf(string(resp.Kvs[0].Value), "%d", &currentID)
		if err != nil {
			// 错误时返回 uint64 最大值
			return ^uint64(0), fmt.Errorf("failed to parse current ID: %v", err)
		}
	} else {
		currentID = 0
	}

	// 如果 ID 超过最大值，则重置为 0
	if currentID >= maxID {
		currentID = 0
	}

	// 更新 ID 计数器
	newID := currentID + 1
	_, err = txn.Then(clientv3.OpPut(idKey, fmt.Sprintf("%d", newID))).Commit()
	if err != nil {
		// 错误时返回 uint64 最大值
		return ^uint64(0), fmt.Errorf("failed to increment ID: %v", err)
	}

	// 返回新的 ID
	return newID - 1, nil
}

// 释放一个 ID 到回收池
func releaseID(ctx context.Context, etcdClient *clientv3.Client, id uint64) error {
	// 将 ID 转换为字符串并放入回收池
	idStr := fmt.Sprintf("%d", id)
	_, err := etcdClient.Put(ctx, recycledIDKey, idStr)
	if err != nil {
		return fmt.Errorf("failed to release ID %d: %v", id, err)
	}
	log.Printf("ID %d successfully released", id)
	return nil
}

// 清除所有的 ID 键
func clearAllIDs(etcdClient *clientv3.Client) error {
	// 删除 ID 和回收池的键
	_, err := etcdClient.Delete(context.Background(), idKey)
	if err != nil {
		return fmt.Errorf("failed to delete node_id_counter: %v", err)
	}

	_, err = etcdClient.Delete(context.Background(), recycledIDKey)
	if err != nil {
		return fmt.Errorf("failed to delete recycled_ids: %v", err)
	}

	log.Println("All ID-related keys cleared")
	return nil
}

// 定期清理过期的 ID
func sweepExpiredIDs(etcdClient *clientv3.Client) {
	// 等待一段时间（例如 10 秒）
	time.Sleep(10 * time.Second)

	// 获取所有的 ID 键
	resp, err := etcdClient.Get(context.Background(), "ids/", clientv3.WithPrefix())
	if err != nil {
		log.Printf("Failed to list IDs: %v", err)
		return
	}

	// 遍历所有键，检查是否过期
	for _, kv := range resp.Kvs {
		key := string(kv.Key)

		// 检查每个键的租约状态
		leaseResp, err := etcdClient.TimeToLive(context.Background(), clientv3.LeaseID(kv.Lease))
		if err != nil {
			log.Printf("Failed to get TTL for key %s: %v", key, err)
			continue
		}

		// 如果 TTL 为 0，表示租约已经过期
		if leaseResp.TTL == 0 {
			// 删除过期的 ID
			_, err := etcdClient.Delete(context.Background(), key)
			if err != nil {
				log.Printf("Failed to delete expired ID %s: %v", key, err)
			} else {
				log.Printf("ID %s expired and deleted", key)
			}
		}
	}
}
