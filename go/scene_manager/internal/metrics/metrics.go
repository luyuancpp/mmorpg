// Package metrics exposes Prometheus gauges for SceneManager's per-node load
// surface. The intent is to mirror the Redis-backed state (which is authoritative
// for scheduling) into Prometheus so operators can dashboard and alert without
// shelling into redis-cli.
//
// All gauges are labelled {node_id, zone_id, role}. role is the string form of
// the declared scene_node_type: "main_world", "instance", "main_world_cross",
// "instance_cross", or "unknown".
//
// Reset() is invoked when a node leaves the cluster so stale label sets do not
// linger forever.
package metrics

import (
	"net/http"
	"strconv"
	"sync"
	"time"

	"scene_manager/internal/constants"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zeromicro/go-zero/core/logx"
)

const subsystem = "scene_manager"

var (
	playerCount = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "node_player_count",
		Help:      "Online players reported by SceneManager for each scene node.",
	}, []string{"node_id", "zone_id", "role"})

	sceneCount = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "node_scene_count",
		Help:      "Number of scene entities currently allocated to each scene node.",
	}, []string{"node_id", "zone_id", "role"})

	loadScore = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "node_load_score",
		Help:      "Composite load score used for scheduling (lower = more attractive).",
	}, []string{"node_id", "zone_id", "role"})

	nodesByRole = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "nodes_by_role",
		Help:      "Count of live scene nodes per zone and declared role.",
	}, []string{"zone_id", "role"})

	registerOnce sync.Once
)

func register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(playerCount, sceneCount, loadScore, nodesByRole)
	})
}

// RoleLabel returns the string label for a scene_node_type. Unknown values
// fall through to "unknown" so the metric stays well-formed even during
// misconfiguration (which will already have been logged by LoadReporter).
func RoleLabel(t uint32) string {
	switch t {
	case constants.SceneNodeTypeMainWorld:
		return "main_world"
	case constants.SceneNodeTypeInstance:
		return "instance"
	case constants.SceneNodeTypeMainWorldCross:
		return "main_world_cross"
	case constants.SceneNodeTypeInstanceCross:
		return "instance_cross"
	}
	return "unknown"
}

// ObserveNode updates the per-node gauges. Called by LoadReporter every
// scrape interval for every known node.
func ObserveNode(nodeID string, zoneID uint32, role uint32, sceneCnt, playerCnt int64, score float64) {
	register()
	zoneStr := strconv.FormatUint(uint64(zoneID), 10)
	roleStr := RoleLabel(role)
	playerCount.WithLabelValues(nodeID, zoneStr, roleStr).Set(float64(playerCnt))
	sceneCount.WithLabelValues(nodeID, zoneStr, roleStr).Set(float64(sceneCnt))
	loadScore.WithLabelValues(nodeID, zoneStr, roleStr).Set(score)
}

// ForgetNode drops per-node label sets when a node is removed. Keeping them
// around after removal would make rate()/increase() queries misleading and
// would leak memory in long-running SceneManagers that churn nodes.
func ForgetNode(nodeID string, zoneID uint32, role uint32) {
	register()
	zoneStr := strconv.FormatUint(uint64(zoneID), 10)
	roleStr := RoleLabel(role)
	playerCount.DeleteLabelValues(nodeID, zoneStr, roleStr)
	sceneCount.DeleteLabelValues(nodeID, zoneStr, roleStr)
	loadScore.DeleteLabelValues(nodeID, zoneStr, roleStr)
}

// SetNodesByRole publishes the live count of nodes per (zone, role). Called
// on the LoadReporter tick after all per-node observations are complete so
// the snapshot is internally consistent.
func SetNodesByRole(counts map[struct {
	ZoneID uint32
	Role   uint32
}]int) {
	register()
	nodesByRole.Reset()
	for k, v := range counts {
		nodesByRole.WithLabelValues(
			strconv.FormatUint(uint64(k.ZoneID), 10),
			RoleLabel(k.Role),
		).Set(float64(v))
	}
}

// Start exposes /metrics over HTTP. Returns immediately; failures log but do
// not block service startup — metrics are observability, not critical path.
// Empty addr is a no-op (disables the endpoint).
func Start(addr string) {
	if addr == "" {
		logx.Info("[metrics] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return
	}
	register()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	go func() {
		logx.Infof("[metrics] Prometheus /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("[metrics] HTTP server exited: %v", err)
		}
	}()
}
