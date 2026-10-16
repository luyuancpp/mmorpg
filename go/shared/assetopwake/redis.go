package assetopwake

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// go-redis 订阅通道的参数。
//
// 消费端(handle)只做解析与非阻塞拿名额,微秒级,通道正常不会积压;真积压时宁可丢也不要让 go-redis
// 默认的 60 秒发送超时把整个订阅连接拖住 —— 唤醒越旧越没用(同 login task_result_dispatcher 的教训)。
const (
	wakeChannelSize        = 1024
	wakeChannelSendTimeout = time.Second
	// subscribeConfirmTimeout 是等 SUBSCRIBE 确认的上限:Redis 连不上时尽快失败、交给 run 的重订阅节律。
	subscribeConfirmTimeout = 5 * time.Second
)

// goRedisBus 用 go-redis v9 实现 bus。UniversalClient 覆盖单机与集群两种部署。
type goRedisBus struct {
	c redis.UniversalClient
}

// subscribe 先等到 SUBSCRIBE 确认再交出消息通道:不确认就开始读,订阅失败会被误当成「一直没人落地」。
// Receive 必须在 Channel 之前调用(go-redis 约定:建了 Channel 之后不能再用 Receive*)。
// 之后的断线由 go-redis 自己重连重订阅(它按健康检查发 PING),通道只在 Close 时关闭。
func (b goRedisBus) subscribe(ctx context.Context, channel string) (<-chan *redis.Message, func() error, error) {
	ps := b.c.Subscribe(ctx, channel)
	if _, err := ps.ReceiveTimeout(ctx, subscribeConfirmTimeout); err != nil {
		_ = ps.Close()
		return nil, nil, fmt.Errorf("assetopwake: 订阅 %s 未确认: %w", channel, err)
	}
	msgs := ps.Channel(
		redis.WithChannelSize(wakeChannelSize),
		redis.WithChannelSendTimeout(wakeChannelSendTimeout),
	)
	return msgs, ps.Close, nil
}

// claimDedup 即 SET key 1 NX EX ttl。
func (b goRedisBus) claimDedup(ctx context.Context, key string, ttl time.Duration) (bool, error) {
	return b.c.SetNX(ctx, key, "1", ttl).Result()
}
