package node_id_redis

import (
	"context"
	"fmt"
	"github.com/redis/go-redis/v9"
	"log"
	"time"
)

const (
	redisAddr     = "localhost:6379"  // Redis 服务地址
	idKey         = "node_id_counter" // 用于存储计数器的键
	recycledIDKey = "recycled_ids"    // 用于存储回收的 ID 键
	maxID         = 65535             // 最大 ID 值
)

var ctx = context.Background()

// 初始化 Redis 客户端
func initRedisClient() *redis.Client {
	client := redis.NewClient(&redis.Options{
		Addr: redisAddr,
	})
	return client
}

// 获取下一个自增的 ID，或者从回收池中获取
func generateID(client *redis.Client) (string, error) {
	// 先尝试从回收池中获取一个 ID
	recycledID, err := client.SPop(ctx, recycledIDKey).Result()
	if err != nil && err != redis.Nil {
		return "", fmt.Errorf("failed to get recycled IDs: %v", err)
	}

	if recycledID != "" {
		// 如果回收池中有 ID，直接从回收池中取一个
		log.Printf("Recycled ID %s", recycledID)
		return recycledID, nil
	}

	// 如果回收池为空，使用自增计数器生成 ID
	currentID, err := client.Incr(ctx, idKey).Result()
	if err != nil {
		return "", fmt.Errorf("failed to increment ID: %v", err)
	}

	// 如果 ID 超过最大值，则重置为 0
	if currentID > maxID {
		currentID = 0
		client.Set(ctx, idKey, currentID, 0) // 重置计数器
	}

	// 返回新的 ID
	return fmt.Sprintf("node-%d", currentID), nil
}

// 释放一个 ID 到回收池
func releaseID(client *redis.Client, id string) error {
	// 将 ID 放入回收池
	_, err := client.SAdd(ctx, recycledIDKey, id).Result()
	if err != nil {
		return fmt.Errorf("failed to release ID %s: %v", id, err)
	}
	log.Printf("ID %s successfully released", id)
	return nil
}

// 清除所有的 ID 键
func clearAllIDs(client *redis.Client) error {
	// 删除 ID 和回收池的键
	_, err := client.Del(ctx, idKey, recycledIDKey).Result()
	if err != nil {
		return fmt.Errorf("failed to delete ID-related keys: %v", err)
	}

	log.Println("All ID-related keys cleared")
	return nil
}

// 定期清理过期的 ID
func sweepExpiredIDs(client *redis.Client) {
	for {
		// 等待一段时间（例如 10 秒）
		time.Sleep(10 * time.Second)

		// 获取所有的 ID 键
		keys, err := client.Keys(ctx, "ids:*").Result()
		if err != nil {
			log.Printf("Failed to list IDs: %v", err)
			continue
		}

		// 遍历所有键，检查是否过期
		for _, key := range keys {
			ttl, err := client.TTL(ctx, key).Result()
			if err != nil {
				log.Printf("Failed to get TTL for key %s: %v", key, err)
				continue
			}

			// 如果 TTL 为 0，表示租约已经过期
			if ttl == 0 {
				// 删除过期的 ID
				_, err := client.Del(ctx, key).Result()
				if err != nil {
					log.Printf("Failed to delete expired ID %s: %v", key, err)
				} else {
					log.Printf("ID %s expired and deleted", key)
				}
			}
		}
	}
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
