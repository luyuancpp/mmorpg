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

	// TableDir: directory containing exported table JSON files (e.g. MainScene.json).
	// Default assumes the service runs from go/scene_manager/ with repo root two levels up.
	TableDir string `json:",default=../../generated/tables"`

	// MainSceneChannelCount: number of channels per main-world scene.
	// Each channel is a separate ECS scene entity sharing the same config_id.
	// Players are assigned to the least-loaded channel. Default 1 (no split).
	MainSceneChannelCount int `json:",default=1"`

	// InstanceIdleTimeoutSeconds: seconds an empty instance is kept alive
	// before being auto-destroyed. 0 = never auto-destroy (default).
	InstanceIdleTimeoutSeconds int64 `json:",default=300"`

	// InstanceCheckIntervalSeconds: how often the lifecycle manager scans
	// for idle instances. Default 30s.
	InstanceCheckIntervalSeconds int64 `json:",default=30"`
}
