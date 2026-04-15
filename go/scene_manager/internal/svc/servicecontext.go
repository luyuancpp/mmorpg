package svc

import (
	"context"
	"fmt"
	"os"
	"scene_manager/internal/config"
	"strconv"
	"strings"
	"time"

	"shared/generated/table"
	"shared/snowflake"

	"github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type ServiceContext struct {
	Config     config.Config
	Redis      *redis.Redis
	Kafka      *kafka.Writer
	Etcd       *clientv3.Client
	SceneIDGen *snowflake.Node
}

func NewServiceContext(c config.Config) *ServiceContext {
	etcdCli, err := clientv3.New(clientv3.Config{
		Endpoints:   c.Etcd.Hosts,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		panic("failed to create etcd client: " + err.Error())
	}

	table.LoadTables(c.TableDir, c.UseBinary)

	return &ServiceContext{
		Config: c,
		Redis:  redis.MustNewRedis(c.Redis.RedisConf),
		Kafka: &kafka.Writer{
			Addr:                   kafka.TCP(c.Kafka.Brokers...),
			Balancer:               &kafka.LeastBytes{},
			AllowAutoTopicCreation: true,
		},
		Etcd:       etcdCli,
		SceneIDGen: snowflake.NewNode(mustAllocNodeID(etcdCli)),
	}
}

const (
	snowflakeNodePrefix = "/scene_manager/snowflake_nodes/"
	snowflakeIDPrefix   = "/scene_manager/snowflake_ids/"
	snowflakeLeaseTTL   = 60 // seconds
)

// mustAllocNodeID allocates a globally unique SnowFlake node ID via etcd.
// Same hostname always gets the same ID (survives restarts).
// Different hostnames are guaranteed different IDs (atomic CAS).
//
// Each allocation is protected by an etcd lease (60s TTL, background KeepAlive).
// When a pod dies, both its hostname key and ID slot key expire automatically,
// making the ID available for recycling by future allocations.
//
// Key layout:
//   /scene_manager/snowflake_nodes/{hostname} -> id   (lease)
//   /scene_manager/snowflake_ids/{id}         -> host (lease)
func mustAllocNodeID(cli *clientv3.Client) uint64 {
	host, err := os.Hostname()
	if err != nil {
		panic("snowflake: failed to get hostname: " + err.Error())
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Grant a lease and start background keep-alive.
	leaseResp, err := cli.Grant(ctx, snowflakeLeaseTTL)
	if err != nil {
		panic(fmt.Sprintf("snowflake: etcd lease grant failed: %v", err))
	}
	leaseID := leaseResp.ID

	// KeepAlive runs until the etcd client is closed or the process exits.
	if _, err := cli.KeepAlive(context.Background(), leaseID); err != nil {
		panic(fmt.Sprintf("snowflake: etcd keep-alive failed: %v", err))
	}

	hostKey := snowflakeNodePrefix + host

	for {
		// 1. Try to reclaim existing hostname entry with our new lease.
		resp, err := cli.Get(ctx, hostKey)
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd get failed: %v", err))
		}
		if len(resp.Kvs) > 0 {
			id, _ := strconv.ParseUint(string(resp.Kvs[0].Value), 10, 64)
			idStr := string(resp.Kvs[0].Value)
			idKey := snowflakeIDPrefix + idStr

			txnResp, err := cli.Txn(ctx).
				If(clientv3.Compare(clientv3.Value(hostKey), "=", idStr)).
				Then(
					clientv3.OpPut(hostKey, idStr, clientv3.WithLease(leaseID)),
					clientv3.OpPut(idKey, host, clientv3.WithLease(leaseID)),
				).
				Commit()
			if err == nil && txnResp.Succeeded {
				logx.Infof("[SnowFlake] reused node ID = %d (hostname=%s)", id, host)
				return id
			}
		}

		// 2. Collect all IDs currently in use.
		usedIDs := make(map[uint64]bool)

		// New-style ID slot keys (with lease).
		idsResp, err := cli.Get(ctx, snowflakeIDPrefix, clientv3.WithPrefix())
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd scan ids failed: %v", err))
		}
		for _, kv := range idsResp.Kvs {
			idStr := strings.TrimPrefix(string(kv.Key), snowflakeIDPrefix)
			id, _ := strconv.ParseUint(idStr, 10, 64)
			usedIDs[id] = true
		}

		// Old-style hostname keys (backward compat: permanent keys from before lease migration).
		nodesResp, err := cli.Get(ctx, snowflakeNodePrefix, clientv3.WithPrefix())
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd scan nodes failed: %v", err))
		}
		for _, kv := range nodesResp.Kvs {
			id, _ := strconv.ParseUint(string(kv.Value), 10, 64)
			usedIDs[id] = true
		}

		// 3. Find the smallest free ID.
		targetID := uint64(snowflake.NodeMask) + 1
		for i := uint64(0); i <= snowflake.NodeMask; i++ {
			if !usedIDs[i] {
				targetID = i
				break
			}
		}
		if targetID > snowflake.NodeMask {
			panic(fmt.Sprintf("snowflake: node ID pool exhausted (max %d)", snowflake.NodeMask))
		}

		// 4. Atomically claim the ID (dual-key CAS).
		idStr := strconv.FormatUint(targetID, 10)
		idKey := snowflakeIDPrefix + idStr

		txnResp, err := cli.Txn(ctx).
			If(
				clientv3.Compare(clientv3.CreateRevision(hostKey), "=", 0),
				clientv3.Compare(clientv3.CreateRevision(idKey), "=", 0),
			).
			Then(
				clientv3.OpPut(hostKey, idStr, clientv3.WithLease(leaseID)),
				clientv3.OpPut(idKey, host, clientv3.WithLease(leaseID)),
			).
			Commit()
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd txn failed: %v", err))
		}

		if txnResp.Succeeded {
			logx.Infof("[SnowFlake] allocated node ID = %d (hostname=%s)", targetID, host)
			return targetID
		}
		// CAS failed — another instance raced us. Retry with fresh scan.
	}
}

