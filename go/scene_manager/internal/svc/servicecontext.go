package svc

import (
	"encoding/json"
	"os"
	"path/filepath"
	"scene_manager/internal/config"
	"time"

	"github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type ServiceContext struct {
	Config          config.Config
	Redis           *redis.Redis
	Kafka           *kafka.Writer
	Etcd            *clientv3.Client
	MainSceneConfIds []uint64
}

func NewServiceContext(c config.Config) *ServiceContext {
	etcdCli, err := clientv3.New(clientv3.Config{
		Endpoints:   c.Etcd.Hosts,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		panic("failed to create etcd client: " + err.Error())
	}

	mainSceneIds := loadMainSceneConfIds(c.TableDir)

	return &ServiceContext{
		Config:          c,
		Redis:           redis.MustNewRedis(c.Redis.RedisConf),
		Kafka: &kafka.Writer{
			Addr:                   kafka.TCP(c.Kafka.Brokers...),
			Balancer:               &kafka.LeastBytes{},
			AllowAutoTopicCreation: true,
		},
		Etcd:            etcdCli,
		MainSceneConfIds: mainSceneIds,
	}
}

// loadMainSceneConfIds reads World.json from tableDir and returns all base scene config IDs
// referenced by main-world scenes (the scene_id foreign key column).
func loadMainSceneConfIds(tableDir string) []uint64 {
	path := filepath.Join(tableDir, "World.json")
	raw, err := os.ReadFile(path)
	if err != nil {
		logx.Errorf("[Table] Failed to read World.json from %s: %v", path, err)
		return nil
	}

	var container struct {
		Data []struct {
			SceneId uint64 `json:"scene_id"`
		} `json:"data"`
	}
	if err := json.Unmarshal(raw, &container); err != nil {
		logx.Errorf("[Table] Failed to parse World.json: %v", err)
		return nil
	}

	ids := make([]uint64, 0, len(container.Data))
	for _, row := range container.Data {
		ids = append(ids, row.SceneId)
	}

	logx.Infof("[Table] Loaded %d main scene config IDs from %s", len(ids), path)
	return ids
}
