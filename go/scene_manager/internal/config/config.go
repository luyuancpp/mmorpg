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

	// ZoneId: zone identifier for this SceneManager instance.
	// Used for C++ convention etcd registration so Scene nodes can discover this service.
	ZoneId uint32 `json:",default=1"`

	// LeaseTTL: etcd lease TTL in seconds for node registration keepalive.
	LeaseTTL int64 `json:",default=60"`

	// TableDir: directory containing exported table data files (JSON or binary pb).
	// Default assumes the service runs from go/scene_manager/ with repo root two levels up.
	TableDir string `json:",default=../../generated/tables"`

	// UseBinary: if true, load .pb (proto binary) table files; otherwise load .json.
	UseBinary bool `json:",default=false"`

	// WorldChannelCount: number of channels per world scene.
	// Each channel is a separate ECS scene entity sharing the same config_id.
	// Players are assigned to the least-loaded channel. Default 1 (no split).
	WorldChannelCount int `json:",default=1"`

	// InstanceIdleTimeoutSeconds: seconds an empty instance is kept alive
	// before being auto-destroyed. 0 = never auto-destroy (default).
	InstanceIdleTimeoutSeconds int64 `json:",default=300"`

	// MirrorIdleTimeoutSeconds: seconds an empty *mirror* instance is kept
	// alive before auto-destroy. Mirrors are ephemeral copies of a world
	// scene — NPCs/state are re-initialized on every re-entry — so keeping
	// them around empty is pure waste. 0 = fall back to
	// InstanceIdleTimeoutSeconds (treat mirrors like normal instances).
	// Default 30s: absorbs brief disconnects / loading screens, but doesn't
	// linger. Set to a small positive number to opt into aggressive cleanup.
	MirrorIdleTimeoutSeconds int64 `json:",default=30"`

	// InstanceCheckIntervalSeconds: how often the lifecycle manager scans
	// for idle instances. Default 30s.
	InstanceCheckIntervalSeconds int64 `json:",default=30"`

	// KafkaWriteTimeoutSeconds: timeout for best-effort Kafka pushes to Gate.
	// These writes use a detached context (not tied to the gRPC request).
	// Default 5 seconds.
	KafkaWriteTimeoutSeconds int64 `json:",default=5"`

	// MirrorSourceNodeLoadCap: soft cap on the source node's scene_count
	// above which mirror requests fall back to GetBestNode instead of
	// co-locating with the source scene. 0 disables the cap (always
	// co-locate). Default 0 = no cap; operators raise it when one world
	// tends to spawn many mirrors.
	MirrorSourceNodeLoadCap int64 `json:",default=0"`

	// GateTokenSecret: HMAC-SHA256 secret shared with Gate nodes for signing
	// connection tokens during cross-zone redirect.
	GateTokenSecret string `json:",optional"`
}
