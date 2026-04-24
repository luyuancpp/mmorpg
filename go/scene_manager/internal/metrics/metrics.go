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

	rebalanceMigrationsTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rebalance_migrations_total",
		Help:      "World channel migrations by outcome (migrated|failed) and reason.",
	}, []string{"zone_id", "reason", "outcome"})

	rebalancePending = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "rebalance_pending",
		Help:      "Channels currently queued for migration, broken down by reason.",
	}, []string{"zone_id", "reason"})

	// mirrorColocateTotal counts mirror placement outcomes. "hit" = the
	// mirror was co-located with its source scene's node; "fallback" = the
	// source node wasn't viable (dead, over load cap, no mapping) and we
	// used GetBestNode instead. Dashboards should chart hit / (hit+fallback)
	// as the mirror co-location effectiveness rate.
	mirrorColocateTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "mirror_colocate_total",
		Help:      "Mirror placement outcomes: hit (co-located) vs fallback (picked best node).",
	}, []string{"zone_id", "outcome", "reason"})

	// instanceDestroyedTotal breaks destroys down by kind (mirror vs
	// normal instance) and reason (idle auto-destroy, explicit RPC,
	// cascade from source destroy, node-death reconciliation). Operators
	// use this to sanity-check that instance lifecycles are short and
	// that mirrors are actually getting reclaimed by the shorter timeout.
	instanceDestroyedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "instance_destroyed_total",
		Help:      "Instance/mirror destroys by kind (instance|mirror) and reason (idle|explicit|cascade|node_death).",
	}, []string{"zone_id", "kind", "reason"})

	// enterSceneRejectedTotal is incremented when EnterScene bails out
	// because the scene disappeared mid-request. A nonzero steady rate
	// indicates destroy-while-entering pressure — likely a sign
	// instance idle timeouts are too aggressive or that a reconciliation
	// pass is destroying scenes the client is still trying to enter.
	enterSceneRejectedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "enter_scene_rejected_total",
		Help:      "EnterScene rejections by reason (scene_gone is the only one currently emitted).",
	}, []string{"zone_id", "reason"})

	// sceneOrphansReconciledTotal tracks scenes destroyed by the
	// node-death reconciliation loop. A spike here correlates with a
	// node crash / rollout and should match (approximately) the count
	// of instances that were hosted on the departed node.
	sceneOrphansReconciledTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "scene_orphans_reconciled_total",
		Help:      "Orphan scenes cleaned up after a node death, broken down by zone.",
	}, []string{"zone_id"})

	// mirrorSourceMissingTotal counts mirror create requests rejected
	// because their source_scene_id no longer exists. A nonzero rate
	// after a feature ships usually means the caller (gameplay code)
	// is racing the source scene's destroy path; consider holding a
	// soft "keep-alive" on the source while a mirror is in flight.
	mirrorSourceMissingTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "mirror_source_missing_total",
		Help:      "CreateScene mirror requests rejected because source_scene_id is gone.",
	}, []string{"zone_id"})

	// mirrorDedupTotal tracks the MirrorDedupBySource code path.
	//   outcome=hit   -> request returned an existing mirror
	//   outcome=miss  -> no mirror existed, fell through to fresh allocate
	//   outcome=stale -> mirrors set had a dangling id; cleaned + fell through
	// Only emitted when the config flag is on. Operators dashboard
	// hit-rate to confirm dedup is actually saving allocations.
	mirrorDedupTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "mirror_dedup_total",
		Help:      "Mirror dedup attempts by outcome (hit|miss|stale). Only emitted when MirrorDedupBySource=true.",
	}, []string{"zone_id", "outcome"})

	registerOnce sync.Once
)

func register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(
			playerCount, sceneCount, loadScore, nodesByRole,
			rebalanceMigrationsTotal, rebalancePending,
			mirrorColocateTotal, instanceDestroyedTotal,
			enterSceneRejectedTotal, sceneOrphansReconciledTotal,
			mirrorSourceMissingTotal, mirrorDedupTotal,
		)
	})
}

// ObserveMirrorColocate records one mirror placement outcome. outcome is
// "hit" or "fallback". reason is only meaningful on fallbacks
// ("no_mapping" | "node_dead" | "overloaded" | "no_source"); pass "ok"
// for hit rows so the label set stays well-formed.
func ObserveMirrorColocate(zoneID uint32, outcome, reason string) {
	register()
	mirrorColocateTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), outcome, reason,
	).Inc()
}

// ObserveInstanceDestroyed records one scene destroy. kind is
// "instance" or "mirror"; reason is "idle" | "explicit" | "cascade" |
// "node_death".
func ObserveInstanceDestroyed(zoneID uint32, kind, reason string) {
	register()
	instanceDestroyedTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), kind, reason,
	).Inc()
}

// ObserveEnterSceneRejected records one EnterScene request that was
// rejected. Currently only the destroy-while-entering path emits this
// (reason="scene_gone"); other fast-fail paths bubble their own errors.
func ObserveEnterSceneRejected(zoneID uint32, reason string) {
	register()
	enterSceneRejectedTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), reason,
	).Inc()
}

// ObserveSceneOrphansReconciled records the count of orphan scenes
// cleaned up in a single node-death reconciliation sweep. Zero-count
// sweeps are silently dropped so we don't fabricate a counter tick
// every time a healthy node exits.
func ObserveSceneOrphansReconciled(zoneID uint32, count int) {
	if count <= 0 {
		return
	}
	register()
	sceneOrphansReconciledTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10),
	).Add(float64(count))
}

// ObserveMirrorSourceMissing records one mirror create rejected because
// its source scene no longer exists. See ErrSourceSceneGone.
func ObserveMirrorSourceMissing(zoneID uint32) {
	register()
	mirrorSourceMissingTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10),
	).Inc()
}

// ObserveMirrorDedup records one outcome of the MirrorDedupBySource code
// path. outcome must be one of "hit" | "miss" | "stale".
func ObserveMirrorDedup(zoneID uint32, outcome string) {
	register()
	mirrorDedupTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), outcome,
	).Inc()
}

// ObserveRebalanceMigration records one completed migration attempt.
// outcome is either "migrated" or "failed". reason echoes rebalanceReason
// ("node_gone" | "better_home").
func ObserveRebalanceMigration(zoneID uint32, reason, outcome string) {
	register()
	rebalanceMigrationsTotal.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), reason, outcome,
	).Inc()
}

// SetRebalancePending publishes the queue depth per (zone, reason).
// Called by the rebalance planner so dashboards can distinguish
// transient spikes ("a world pod just restarted") from chronic drift
// ("budget too low; urgent queue keeps growing").
func SetRebalancePending(zoneID uint32, reason string, count int) {
	register()
	rebalancePending.WithLabelValues(
		strconv.FormatUint(uint64(zoneID), 10), reason,
	).Set(float64(count))
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

// debugMux holds handlers registered via RegisterDebugHandler. Mutating it
// after Start() returns is safe because the HTTP server reads it through
// the mux lock. Registration before Start is the expected pattern.
var (
	debugMuxOnce sync.Once
	debugMux     *http.ServeMux
)

func ensureDebugMux() *http.ServeMux {
	debugMuxOnce.Do(func() { debugMux = http.NewServeMux() })
	return debugMux
}

// RegisterDebugHandler attaches a handler under /debug/<path>. Use for
// operational introspection (rebalance plan, dead-node probe, etc). Do
// NOT use for business logic — these endpoints sit next to /metrics on
// an internal port and must never authenticate users.
//
// path should start with a leading "/"; the "/debug" prefix is added.
func RegisterDebugHandler(path string, handler http.Handler) {
	if path == "" || path[0] != '/' {
		path = "/" + path
	}
	ensureDebugMux().Handle("/debug"+path, handler)
}

// Start exposes /metrics over HTTP alongside any /debug/* handlers that
// were registered before Start returned. Failures log but do not block
// service startup — metrics are observability, not critical path.
// Empty addr is a no-op (disables the endpoint).
func Start(addr string) {
	if addr == "" {
		logx.Info("[metrics] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return
	}
	register()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	// Mount the debug sub-mux if any handlers registered. Using a dedicated
	// sub-mux means late registrations (rare, but possible in tests) still
	// route correctly without racing on mux setup.
	mux.Handle("/debug/", ensureDebugMux())
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
