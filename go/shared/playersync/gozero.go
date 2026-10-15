package playersync

import (
	"context"

	"github.com/zeromicro/go-zero/core/stores/redis"
)

// goZeroEvaler 把 go-zero 的 *redis.Redis 适配成 Evaler。
// friend / match(team)/ guild 的核心库句柄(SharedRedis / PlayerLocatorRedis)都是这套客户端。
type goZeroEvaler struct {
	r *redis.Redis
}

// NewGoZeroEvaler 用 go-zero Redis 客户端构造 Evaler。r 必须指向核心实例(见包注释)。
// r 为 nil 时返回 nil,NewBumper 随之返回 nil(bump 全部是无操作)。
func NewGoZeroEvaler(r *redis.Redis) Evaler {
	if r == nil {
		return nil
	}
	return goZeroEvaler{r: r}
}

func (e goZeroEvaler) Eval(ctx context.Context, script string, keys []string, args ...string) error {
	_, err := e.r.EvalCtx(ctx, script, keys, toAny(args)...)
	return err
}
