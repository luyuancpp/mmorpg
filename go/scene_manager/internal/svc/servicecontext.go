package svc

import (
	"context"
	"fmt"
	"os"
	"scene_manager/internal/config"
	"strconv"
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
	snowflakeNodePrefix  = "/scene_manager/snowflake_nodes/"
	snowflakeCounterKey  = "/scene_manager/snowflake_counter"
)

// mustAllocNodeID allocates a globally unique SnowFlake node ID via etcd.
// Same hostname always gets the same ID (survives restarts).
// Different hostnames are guaranteed different IDs (atomic CAS).
func mustAllocNodeID(cli *clientv3.Client) uint64 {
	host, err := os.Hostname()
	if err != nil {
		panic("snowflake: failed to get hostname: " + err.Error())
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	hostKey := snowflakeNodePrefix + host

	// 1. Check if this hostname already has an assigned ID.
	resp, err := cli.Get(ctx, hostKey)
	if err != nil {
		panic(fmt.Sprintf("snowflake: etcd get failed: %v", err))
	}
	if len(resp.Kvs) > 0 {
		id, _ := strconv.ParseUint(string(resp.Kvs[0].Value), 10, 64)
		logx.Infof("[SnowFlake] reused node ID = %d (hostname=%s)", id, host)
		return id
	}

	// 2. Atomically increment the counter and claim the new ID.
	for {
		counterResp, err := cli.Get(ctx, snowflakeCounterKey)
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd get counter failed: %v", err))
		}

		var nextID uint64
		var counterRev int64
		if len(counterResp.Kvs) > 0 {
			cur, _ := strconv.ParseUint(string(counterResp.Kvs[0].Value), 10, 64)
			nextID = cur + 1
			counterRev = counterResp.Kvs[0].ModRevision
		} else {
			nextID = 0
			counterRev = 0
		}

		if nextID > snowflake.NodeMask {
			panic(fmt.Sprintf("snowflake: node ID pool exhausted (max %d)", snowflake.NodeMask))
		}

		idStr := strconv.FormatUint(nextID, 10)

		// CAS: only succeed if counter hasn't changed and hostKey doesn't exist yet.
		var counterCmp clientv3.Cmp
		if counterRev == 0 {
			counterCmp = clientv3.Compare(clientv3.CreateRevision(snowflakeCounterKey), "=", 0)
		} else {
			counterCmp = clientv3.Compare(clientv3.ModRevision(snowflakeCounterKey), "=", counterRev)
		}

		txnResp, err := cli.Txn(ctx).
			If(
				counterCmp,
				clientv3.Compare(clientv3.CreateRevision(hostKey), "=", 0),
			).
			Then(
				clientv3.OpPut(snowflakeCounterKey, idStr),
				clientv3.OpPut(hostKey, idStr),
			).
			Commit()
		if err != nil {
			panic(fmt.Sprintf("snowflake: etcd txn failed: %v", err))
		}

		if txnResp.Succeeded {
			logx.Infof("[SnowFlake] allocated node ID = %d (hostname=%s)", nextID, host)
			return nextID
		}

		// Another replica raced us — re-check if our hostname was claimed by retry.
		resp, _ := cli.Get(ctx, hostKey)
		if len(resp.Kvs) > 0 {
			id, _ := strconv.ParseUint(string(resp.Kvs[0].Value), 10, 64)
			logx.Infof("[SnowFlake] reused node ID = %d (hostname=%s, after race)", id, host)
			return id
		}
		// Counter was bumped by another host — retry with new counter value.
	}
}

