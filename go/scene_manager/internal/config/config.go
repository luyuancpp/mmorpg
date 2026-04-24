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

	// WorldChannelCount: default number of channels per world scene.
	// Each channel is a separate ECS scene entity sharing the same config_id.
	// Players are assigned to the least-loaded channel. Default 1 (no split).
	WorldChannelCount int `json:",default=1"`

	// WorldChannelCountByConfId: per-confId override of WorldChannelCount.
	// Use for hot main cities / starter zones that need more copies than the
	// default, or niche maps that only need one. A value of 0 falls back to
	// WorldChannelCount. Example in yaml:
	//   WorldChannelCountByConfId:
	//     1001: 4      # big city — 4 channels
	//     1010: 1      # tutorial — 1 channel
	WorldChannelCountByConfId map[uint64]int `json:",optional"`

	// StrictNodeTypeSeparation: when true, main-world creation is only routed
	// to nodes whose scene_node_type is kMainSceneNode/kMainSceneCrossNode,
	// and instance creation is only routed to kSceneNode/kSceneSceneCrossNode.
	// Default true — production deployments should dedicate nodes by role.
	// Set false for dev / single-node deployments where one process hosts
	// both kinds (the filter falls back to the full zone pool).
	// Mirror co-location bypasses this filter regardless of the flag: the
	// gain from reusing the source scene's resident map/AI data beats type
	// purity, and operators opt in via source_scene_id explicitly.
	StrictNodeTypeSeparation bool `json:",default=true"`

	// NodeLoadWeightSceneCount / NodeLoadWeightPlayerCount: weights combined
	// into the per-node load score stored in scene_nodes:zone:{zone}:load.
	// score = α·scene_count + β·player_count. Defaults α=1.0, β=0.01 — one
	// scene entity costs the same as ~100 players, reflecting the fact that
	// an empty scene still burns CPU on ticks/AOI scaffolding.
	// Tune β higher if your scenes are cheap and players are expensive
	// (e.g. heavy physics or per-player AI).
	NodeLoadWeightSceneCount  float64 `json:",default=1.0"`
	NodeLoadWeightPlayerCount float64 `json:",default=0.01"`

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

	// MirrorDedupBySource: when true, CreateScene with source_scene_id > 0
	// returns an EXISTING mirror of that source instead of allocating a new
	// one (if any). Off by default because the typical mirror use case is
	// per-player phasing where independent copies are intentional. Turn it
	// on for "shared instance" semantics (e.g. raid lockouts, world bosses
	// where the whole zone shares one mirror copy). Selection is arbitrary
	// when multiple mirrors exist; do not enable this if your gameplay
	// requires deterministic mirror identity per request.
	MirrorDedupBySource bool `json:",default=false"`

	// GateTokenSecret: HMAC-SHA256 secret shared with Gate nodes for signing
	// connection tokens during cross-zone redirect.
	GateTokenSecret string `json:",optional"`

	// MetricsListenAddr: host:port to serve Prometheus /metrics. Empty
	// disables the scrape endpoint (default). Typical prod value is
	// ":9150" — keep it off the gRPC port and scrape it via ServiceMonitor.
	MetricsListenAddr string `json:",optional"`

	// MaxRebalanceMigrationsPerTick bounds the number of world-channel
	// migrations executed per world-node-set change event. Rebalancing
	// moves empty channels (player_count == 0) toward a more uniform hash
	// distribution; large values can thrash the cluster on scale events,
	// small values slow convergence. Default 10 balances responsiveness
	// with safety. Set to 0 to disable proactive rebalancing (channels on
	// dead nodes are still re-homed by GetBestWorldChannel on demand).
	MaxRebalanceMigrationsPerTick int `json:",default=10"`

	// RebalanceCheckIntervalSeconds: how often the LoadReporter runs a
	// periodic rebalance pass in addition to event-triggered passes.
	// Event-driven rebalance catches join/leave of world-hosting nodes,
	// but a channel can become opportunistic-migratable after its players
	// drain with no corresponding etcd event. The ticker closes that gap.
	// Default 300s (5 min). Set to 0 to disable — event-driven only.
	RebalanceCheckIntervalSeconds int64 `json:",default=300"`

	// CleanupOrphanChannelsOnStartup: when true (default), SceneManager
	// scans Redis on fullSync for world_channels:* sets whose confId is
	// not in World.json and deletes them. This removes drift left by
	// designers dropping a map from the table without a clean tear-down.
	// Set to false in environments where you migrate maps manually or
	// share a Redis cluster across incompatible World.json versions.
	// NB: cleanup is refused when worldConfIds() returns empty (a
	// defensive check against a table-load failure nuking prod data).
	CleanupOrphanChannelsOnStartup bool `json:",default=true"`
}

// ChannelCountFor returns the effective world-channel count for a confId,
// honoring per-confId overrides, clamped to at least 1.
func (c *Config) ChannelCountFor(confId uint64) int {
	if c.WorldChannelCountByConfId != nil {
		if v, ok := c.WorldChannelCountByConfId[confId]; ok && v > 0 {
			return v
		}
	}
	if c.WorldChannelCount < 1 {
		return 1
	}
	return c.WorldChannelCount
}
