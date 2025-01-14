package node_id_etcd

import (
	"context"
	"deploy/internal/config"
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/conf"
	"log"
	"time"

	"go.etcd.io/etcd/client/v3"
)

const (
	idTTL = 60 * time.Second // ID 的 TTL 设置为 60 秒
	maxID = 1000             // 最大 ID 值
)

var configFile = flag.String("config", "../../../../etc/deployservice.yaml", "the config file")

// 初始化 Etcd 客户端
func initEtcdClient() (*clientv3.Client, error) {
	var c config.Config
	conf.MustLoad(*configFile, &c)

	cli, err := clientv3.New(clientv3.Config{
		Endpoints: c.Etcd.Hosts,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create etcd client: %v", err)
	}
	return cli, nil
}

// 获取指定服务器类型的计数器键
func getServerTypeKey(serverType uint32) string {
	return fmt.Sprintf("node_id_counter_%d", serverType)
}

// 获取指定服务器类型的回收池键
func getRecycledIDKey(serverType uint32) string {
	return fmt.Sprintf("recycled_ids_%d", serverType)
}

// 获取下一个自增的 ID，或者从回收池中获取
func generateID(ctx context.Context, etcdClient *clientv3.Client, serverType uint32) (uint64, error) {
	// 先尝试从回收池中获取一个 ID
	recycledIDKey := getRecycledIDKey(serverType)
	resp, err := etcdClient.Get(ctx, recycledIDKey)
	if err != nil {
		return ^uint64(0), fmt.Errorf("failed to get recycled IDs for server type %d: %v", serverType, err)
	}

	if len(resp.Kvs) > 0 {
		// 如果回收池中有 ID，直接从回收池中取一个
		recycledID := string(resp.Kvs[0].Value)
		_, err := etcdClient.Delete(ctx, recycledIDKey)
		if err != nil {
			return ^uint64(0), fmt.Errorf("failed to delete recycled ID %s for server type %d: %v", recycledID, serverType, err)
		}
		log.Printf("Recycled ID %s for server type %d", recycledID, serverType)
		var id uint64
		_, err = fmt.Sscanf(recycledID, "%d", &id)
		if err != nil {
			return ^uint64(0), fmt.Errorf("failed to parse recycled ID %s for server type %d: %v", recycledID, serverType, err)
		}
		return id, nil
	}

	// 使用服务器类型生成 ID 计数器键
	serverTypeKey := getServerTypeKey(serverType)

	// 获取当前 ID 值并进行自增
	resp, err = etcdClient.Get(ctx, serverTypeKey)
	if err != nil {
		return ^uint64(0), fmt.Errorf("failed to get current ID for server type %d: %v", serverType, err)
	}

	// 获取当前的计数器值
	var currentID uint64
	if len(resp.Kvs) > 0 {
		_, err = fmt.Sscanf(string(resp.Kvs[0].Value), "%d", &currentID)
		if err != nil {
			return ^uint64(0), fmt.Errorf("failed to parse current ID for server type %d: %v", serverType, err)
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
	_, err = etcdClient.Put(ctx, serverTypeKey, fmt.Sprintf("%d", newID))
	if err != nil {
		return ^uint64(0), fmt.Errorf("failed to increment ID for server type %d: %v", serverType, err)
	}

	// 返回新的 ID
	return newID - 1, nil
}

// 释放一个 ID 到对应的回收池
func releaseID(ctx context.Context, etcdClient *clientv3.Client, id uint64, serverType uint32) error {
	// 获取对应服务器类型的回收池键
	recycledIDKey := getRecycledIDKey(serverType)
	// 将 ID 转换为字符串并放入回收池
	idStr := fmt.Sprintf("%d", id)
	_, err := etcdClient.Put(ctx, recycledIDKey, idStr)
	if err != nil {
		return fmt.Errorf("failed to release ID %d for server type %d: %v", id, serverType, err)
	}
	log.Printf("ID %d successfully released for server type %d", id, serverType)
	return nil
}

// 清除所有的 ID 键
func clearAllIDs(etcdClient *clientv3.Client) error {
	// 删除所有服务器类型的计数器键
	_, err := etcdClient.Delete(context.Background(), "node_id_counter_", clientv3.WithPrefix())
	if err != nil {
		return fmt.Errorf("failed to delete node_id_counter keys: %v", err)
	}

	// 删除所有回收池的键
	_, err = etcdClient.Delete(context.Background(), "recycled_ids_", clientv3.WithPrefix())
	if err != nil {
		return fmt.Errorf("failed to delete recycled_ids keys: %v", err)
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
