package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	NodeID string `json:",optional"` // deprecated: kept for backward compat
	ZoneID uint32
	Kafka  struct {
		Brokers []string
	}
	// NodeRemovalGraceSeconds: seconds to wait before removing a scene node
	// that disappeared from etcd. Allows breakpoint-paused nodes to re-register.
	// 0 = immediate removal (production default).
	NodeRemovalGraceSeconds int64 `json:",default=0"`
}
