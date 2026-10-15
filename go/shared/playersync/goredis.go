package playersync

import (
	"context"

	"github.com/redis/go-redis/v9"
)

// goRedisEvaler 把 go-redis v9 的客户端适配成 Evaler。
// trade(SharedRedis)等直接用 go-redis 的服务走这里;单机 / 哨兵 / 集群客户端都实现 UniversalClient。
type goRedisEvaler struct {
	c redis.UniversalClient
}

// NewGoRedisEvaler 用 go-redis v9 客户端构造 Evaler。c 必须指向核心实例(见包注释)。
// c 为 nil 时返回 nil,NewBumper 随之返回 nil(bump 全部是无操作)。
func NewGoRedisEvaler(c redis.UniversalClient) Evaler {
	if c == nil {
		return nil
	}
	return goRedisEvaler{c: c}
}

func (e goRedisEvaler) Eval(ctx context.Context, script string, keys []string, args ...string) error {
	return e.c.Eval(ctx, script, keys, toAny(args)...).Err()
}
