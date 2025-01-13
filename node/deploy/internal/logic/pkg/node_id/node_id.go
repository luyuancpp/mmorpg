package node_id

import (
	"context"
	"fmt"
	"log"
	"time"

	"go.etcd.io/etcd/client/v3"
)

const (
	etcdAddr = "localhost:2379"  // Etcd 服务地址
	idTTL    = 60 * time.Second  // ID 的 TTL 设置为 60 秒
	idKey    = "node_id_counter" // 用于存储计数器的键
	maxID    = 65535             // 最大ID值
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

// 获取下一个自增的 ID
func generateID(ctx context.Context, etcdClient *clientv3.Client) (string, error) {
	// 使用事务确保自增操作是原子性的
	txn := etcdClient.Txn(ctx)

	// 获取当前 ID 值并进行自增
	resp, err := etcdClient.Get(ctx, idKey)
	if err != nil {
		return "", fmt.Errorf("failed to get current ID: %v", err)
	}

	// 获取当前的计数器值
	var currentID int64
	if len(resp.Kvs) > 0 {
		currentID = ParseIDValue(resp.Kvs[0].Value)
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
		return "", fmt.Errorf("failed to increment ID: %v", err)
	}

	// 返回新的 ID
	return fmt.Sprintf("node-%d", newID), nil
}

// 解析 ID 的值
func ParseIDValue(value []byte) int64 {
	var id int64
	_, err := fmt.Sscanf(string(value), "%d", &id)
	if err != nil {
		log.Printf("Failed to parse ID value: %v", err)
	}
	return id
}

// 回收 ID
func releaseID(ctx context.Context, etcdClient *clientv3.Client, id string) error {
	// Etcd 中不需要特别删除 ID，因为它是自增的计数器
	// 这里只是示例，没有实际的回收操作
	log.Printf("Released ID: %s", id)
	return nil
}

// 定期清理过期的 ID
func sweepExpiredIDs(etcdClient *clientv3.Client) {
	for {
		// 等待一段时间（例如 10 秒）
		time.Sleep(10 * time.Second)

		// 获取所有的 ID 键
		resp, err := etcdClient.Get(context.Background(), "ids/", clientv3.WithPrefix())
		if err != nil {
			log.Printf("Failed to list IDs: %v", err)
			continue
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
}
