package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	NodeID string `json:",optional"` // deprecated: kept for backward compat
	Kafka  struct {
		Brokers []string
	}
	// NodeRemovalGraceSeconds: seconds to wait before removing a scene node
	// that disappeared from etcd. Allows breakpoint-paused nodes to re-register.
	// 0 = immediate removal (production default).
	NodeRemovalGraceSeconds int64 `json:",default=0"`

	// MainSceneConfIds: scene_conf_ids that are persistent main-world scenes.
	// These are created at startup and never auto-destroyed.
	// Anything not in this list is treated as an instance.
	MainSceneConfIds []uint64 `json:",optional"`

	// InstanceIdleTimeoutSeconds: seconds an empty instance is kept alive
	// before being auto-destroyed. 0 = never auto-destroy (default).
	InstanceIdleTimeoutSeconds int64 `json:",default=300"`

	// InstanceCheckIntervalSeconds: how often the lifecycle manager scans
	// for idle instances. Default 30s.
	InstanceCheckIntervalSeconds int64 `json:",default=30"`
}
