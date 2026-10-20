package saveoutbox

import (
	"context"
	"errors"
	"sync"
	"time"

	dbconfig "db/internal/config"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

type Service struct {
	cancel  context.CancelFunc
	done    chan struct{}
	pub     *kafkaPublisher
	parent  *redis.Client
	mapping *redis.Client
	once    sync.Once
}

// Start只接主parentRedis，不把只读OwnerEpochRedis误当可写父缓存实例。
// G4a拆分跨实例原子提交未解决前拒绝该拓扑；legacy不具备SQL双ACK，不能开启补投回收。
// health 为本 zone 主库的冷却判定(落点库注册表)，nil 表示不判冷却。
func Start(ctx context.Context, app dbconfig.Config, parent, mapping *redis.Client, health StoreHealth) (*Service, error) {
	if app.ServerConfig.Fencing.EffectiveMode() != dbconfig.FencingModeDual {
		logx.Infof("[SaveOutbox] legacy模式无SQL双ACK，持久存盘补投未启用")
		return nil, nil
	}
	mainRedis := app.ServerConfig.RedisClient
	if owner := app.ServerConfig.OwnerEpochRedis; owner != nil &&
		(owner.Hosts != mainRedis.Hosts || owner.DB != mainRedis.DB) {
		return nil, errors.New("save outbox严格保存要求owner与父缓存同一Redis实例，当前拆分拓扑未获支持")
	}
	if parent == nil || parent.Options().Addr != mainRedis.Hosts || parent.Options().DB != mainRedis.DB {
		return nil, errors.New("save outbox主父缓存Redis装配不匹配")
	}
	if mapping == nil {
		return nil, errors.New("save outbox权威home_zone Redis依赖缺失")
	}
	pub, err := newKafkaPublisher(app.ServerConfig.Kafka.Brokers, topicFor(app.ZoneId, app.ServerConfig.Kafka.TopicGeneration))
	if err != nil {
		return nil, err
	}
	// 不修改共享客户端Options；专用小池明确遵循每拍context截止，最多2~4条连接。
	parentReplay, mappingReplay := replayRedis(parent), (*redis.Client)(nil)
	if parent == mapping {
		mappingReplay = parentReplay
	} else {
		mappingReplay = replayRedis(mapping)
	}
	replay, err := New(parentReplay, mappingReplay, pub, health, Config{ZoneID: app.ZoneId, TopicGeneration: app.ServerConfig.Kafka.TopicGeneration}, nil)
	if err != nil {
		_ = parentReplay.Close()
		if mappingReplay != parentReplay {
			_ = mappingReplay.Close()
		}
		_ = pub.close()
		return nil, err
	}
	loopCtx, cancel := context.WithCancel(ctx)
	s := &Service{cancel: cancel, done: make(chan struct{}), pub: pub, parent: parentReplay, mapping: mappingReplay}
	go func() {
		defer close(s.done)
		replay.Run(loopCtx)
	}()
	return s, nil
}

func (s *Service) Stop() {
	if s == nil {
		return
	}
	s.once.Do(func() {
		s.cancel()
		<-s.done
		_ = s.parent.Close()
		if s.mapping != s.parent {
			_ = s.mapping.Close()
		}
		if err := s.pub.close(); err != nil {
			logx.Errorf("[SaveOutbox] Kafka补投发布器关闭失败；未完成意图仍保留")
		}
	})
}

func replayRedis(source *redis.Client) *redis.Client {
	options := *source.Options()
	options.ContextTimeoutEnabled, options.MaxRetries = true, -1
	options.DialTimeout, options.ReadTimeout, options.WriteTimeout = time.Second, time.Second, time.Second
	options.PoolTimeout, options.DialerRetries = time.Second, 1
	options.PoolSize, options.MaxActiveConns, options.MaxConcurrentDials = 2, 2, 2
	options.MinIdleConns, options.MaxIdleConns = 0, 2
	return redis.NewClient(&options)
}
