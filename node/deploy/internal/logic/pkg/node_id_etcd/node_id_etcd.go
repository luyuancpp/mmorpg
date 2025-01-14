package node_id_etcd

import (
	"context"
	"deploy/internal/config"
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"log"
	"time"

	"go.etcd.io/etcd/client/v3"
)

const (
	idTTL         = 60 * time.Second // ID 的 TTL 设置为 60 秒
	maxID         = 1000             // 最大 ID 值
	maxServerType = 2
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
	// 定义回收池和计数器键
	recycledIDKey := getRecycledIDKey(serverType)
	idKey := getServerTypeKey(serverType)

	// 事务 1：从回收池中获取 ID
	txn1 := etcdClient.Txn(ctx)
	txn1.If(
		clientv3.Compare(clientv3.Version(recycledIDKey), ">", 0), // 判断回收池中是否有 ID
	).
		Then(
			clientv3.OpGet(recycledIDKey),    // 获取回收池中的 ID
			clientv3.OpDelete(recycledIDKey), // 删除回收池中的 ID
		)

	// 提交事务 1
	txn1Resp, err := txn1.Commit()
	if err != nil {
		return 0, fmt.Errorf("failed to commit txn for recycled ID: %v", err)
	}

	var currentID uint64

	// 如果回收池有 ID，直接返回
	if len(txn1Resp.Responses) > 0 && txn1Resp.Responses[0].GetResponseRange() != nil {
		recycledID := string(txn1Resp.Responses[0].GetResponseRange().Kvs[0].Value)
		_, err := fmt.Sscanf(recycledID, "%d", &currentID)
		if err != nil {
			return 0, fmt.Errorf("failed to parse recycled ID: %v", err)
		}
		logx.Info("Returned recycled ID ", currentID, " for server type ", serverType)
		return currentID, nil
	}

	// 事务 2：如果回收池没有 ID，则获取自增 ID，并自增
	var prevID uint64

	// 确保 idKey 存在并初始化为 "0"（即使之前没有此键）
	txn2 := etcdClient.Txn(ctx)
	txn2.If(
		clientv3.Compare(clientv3.Version(idKey), "=", 0), // idKey doesn't exist
	).
		Then(
			clientv3.OpPut(idKey, "0"), // 初始化 idKey 为 0
			clientv3.OpGet(idKey),      // 获取新 ID
		).
		Else(
			clientv3.OpGet(idKey), // 如果 idKey 已存在，则直接获取值
		)

	// 提交事务 2
	txn2Resp, err := txn2.Commit()
	if err != nil {
		return 0, fmt.Errorf("failed to commit txn for initializing or getting ID: %v", err)
	}

	if len(txn2Resp.Responses) == 1 && txn2Resp.Responses[0].GetResponseRange() != nil {
		value := txn2Resp.Responses[0].GetResponseRange().Kvs[0].Value
		_, err := fmt.Sscanf(string(value), "%d", &currentID)
		if err != nil {
			return 0, fmt.Errorf("failed to parse current ID: %v", err)
		}
		prevID = currentID
	} else if len(txn2Resp.Responses) == 2 && txn2Resp.Responses[1].GetResponseRange() != nil {
		value := txn2Resp.Responses[1].GetResponseRange().Kvs[0].Value
		_, err := fmt.Sscanf(string(value), "%d", &currentID)
		if err != nil {
			return 0, fmt.Errorf("failed to parse current ID: %v", err)
		}
		prevID = currentID
	}

	// 开始循环进行自增
	for {
		// 获取当前 ID 值并准备自增
		txn3 := etcdClient.Txn(ctx)
		txn3.If(
			clientv3.Compare(clientv3.Value(idKey), "=", fmt.Sprintf("%d", prevID)), // 保证版本未变
		).
			Then(
				clientv3.OpPut(idKey, fmt.Sprintf("%d", prevID+1)), // 自增
				clientv3.OpGet(idKey),                              // 获取新的 ID
			).
			Else(
				clientv3.OpPut(idKey, fmt.Sprintf("%d", prevID+1)), // 自增
				clientv3.OpGet(idKey),
			)

		// 提交事务 3
		txn3Resp, err := txn3.Commit()
		if err != nil {
			return 0, fmt.Errorf("failed to commit txn for generating ID: %v", err)
		}

		// 如果事务提交成功
		if len(txn3Resp.Responses) > 0 && txn3Resp.Responses[1].GetResponseRange() != nil {
			value := txn3Resp.Responses[1].GetResponseRange().Kvs[0].Value
			_, err := fmt.Sscanf(string(value), "%d", &currentID)
			if err != nil {
				return 0, fmt.Errorf("failed to parse current ID: %v", err)
			}

			// 如果成功自增，则退出循环
			if prevID == currentID-1 {
				logx.Info("Generated new ID ", currentID, " for server type ", serverType)
				return currentID, nil
			}
		}

		// 如果版本冲突，重新获取 ID 和重试
		prevID = currentID
	}
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
	logx.Info("ID ", id, " successfully released for server type ", serverType)
	return nil
}

// 清除所有的 ID 键
// 清除所有的 ID 键，包括每种服务器类型的计数器键和回收池键
func clearAllIDs(etcdClient *clientv3.Client) error {
	// 获取所有的服务器类型（假设服务器类型范围是 [0, maxServerType]）
	// 在实际情况下，你可以从配置或者其他方式获取所有服务器类型
	for serverType := uint32(0); serverType < maxServerType; serverType++ {
		// 获取对应服务器类型的计数器键
		serverTypeKey := getServerTypeKey(serverType)

		// 删除服务器类型的计数器键
		_, err := etcdClient.Delete(context.Background(), serverTypeKey)
		if err != nil {
			return fmt.Errorf("failed to delete node_id_counter for server type %d: %v", serverType, err)
		}

		// 获取对应服务器类型的回收池键
		recycledIDKey := getRecycledIDKey(serverType)

		// 删除回收池的键
		_, err = etcdClient.Delete(context.Background(), recycledIDKey)
		if err != nil {
			return fmt.Errorf("failed to delete recycled_ids for server type %d: %v", serverType, err)
		}

		logx.Info("ID-related keys for server type ", serverType, " cleared")
	}

	log.Println("All server type ID-related keys cleared")
	return nil
}

// 定期清理过期的 ID
func sweepExpiredIDs(etcdClient *clientv3.Client) {
	// 等待一段时间（例如 10 秒）
	time.Sleep(10 * time.Second)

	// 获取所有的 ID 键
	resp, err := etcdClient.Get(context.Background(), "ids/", clientv3.WithPrefix())
	if err != nil {
		logx.Error("Failed to list IDs: ", err)
		return
	}

	// 遍历所有键，检查是否过期
	for _, kv := range resp.Kvs {
		key := string(kv.Key)

		// 检查每个键的租约状态
		leaseResp, err := etcdClient.TimeToLive(context.Background(), clientv3.LeaseID(kv.Lease))
		if err != nil {
			logx.Error("Failed to get TTL for key ", key, ": ", err)
			continue
		}

		// 如果 TTL 为 0，表示租约已经过期
		if leaseResp.TTL == 0 {
			// 删除过期的 ID
			_, err := etcdClient.Delete(context.Background(), key)
			if err != nil {
				logx.Error("Failed to delete expired ID ", key, ": ", err)
			} else {
				logx.Info("ID ", key, " expired and deleted")
			}
		}
	}
}
