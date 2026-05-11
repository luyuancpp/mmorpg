#!/usr/bin/env bash
# Staging deployment helper for the HTTP /api/login migration.
#
# Why this exists:
#   The full deploy story is documented step-by-step in
#   docs/ops/linux-staging-stress-runbook.md §B, but copy-pasting that into
#   a tmux session is error-prone (wrong start order = login dialing
#   not-yet-listening player_locator, gate registering with stale etcd
#   leases, etc). This script encodes the exact tier-staged sequence so
#   a one-line invocation from staging /opt/mmorpg actually starts in
#   the right order with the right wait-for-ready gates between layers.
#
# Why not docker compose / k8s manifests:
#   The repo's existing manifests under deploy/k8s/ are the production
#   path. This script targets the in-between case: a single Linux box
#   running the dev binaries against docker-hosted infra, which is what
#   ops uses for the 1k/2k/5k stress runs (#221 in docs/notes/todo.md).
#   For real prod use the k8s manifests; for dry-runs / capacity tests
#   use this.
#
# Required PATH binaries:
#   - redis-cli        (for residue wipe + readiness check)
#   - etcdctl          (for snowflake compaction; optional but recommended)
#   - go               (only if --build is passed)
#   - mvn / java       (only if --build is passed AND --gateway is enabled)
#   - msbuild / cmake  (cpp build is out of scope; build cpp manually
#                       and drop binaries under bin/)
#
# Usage examples:
#   ./tools/scripts/deploy-staging.sh start
#   ./tools/scripts/deploy-staging.sh stop
#   ./tools/scripts/deploy-staging.sh restart
#   ./tools/scripts/deploy-staging.sh status
#   ./tools/scripts/deploy-staging.sh smoke           # one /api/login curl
#   ./tools/scripts/deploy-staging.sh stress 1000 2000 5000

set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG_DIR="${REPO_ROOT}/run/logs"
PID_DIR="${REPO_ROOT}/run/pids"
mkdir -p "${LOG_DIR}/go_services" "${LOG_DIR}/cpp_nodes" "${LOG_DIR}/java" "${PID_DIR}"

# ── Configuration ──────────────────────────────────────────────────
GATEWAY_HOST="${GATEWAY_HOST:-127.0.0.1}"
GATEWAY_PORT="${GATEWAY_PORT:-8081}"
LOGIN_GRPC="${LOGIN_GRPC:-127.0.0.1:50000}"

# Staged startup tier order — lower tier first, dependent tier waits for
# its dependency to LISTEN before booting. The numbers double as
# reasoning: "tier 1 = pure storage, tier 2 = stateless services that
# only need storage, tier 3 = services that depend on tier 2", etc.
declare -A SVC_TIER=(
  [db]=1
  [data_service]=1
  [player_locator]=2
  [scene_manager]=2
  [login]=3
)
# Listen-port mapping (matches go/<svc>/etc/<svc>.yaml). Used for
# wait-for-ready loops between tiers.
declare -A SVC_PORT=(
  [db]=6000
  [data_service]=9000
  [player_locator]=50200
  [scene_manager]=60300
  [login]=50000
)

declare -a TIER_ORDER=(db data_service player_locator scene_manager login)

# ── Helpers ────────────────────────────────────────────────────────
log() { printf '[deploy-staging %s] %s\n' "$(date +%H:%M:%S)" "$*"; }
die() { log "ERROR: $*"; exit 1; }

wait_for_port() {
  local port=$1 timeout=${2:-30}
  local elapsed=0
  while ! ss -ltn 2>/dev/null | grep -qE "[:.]$port[[:space:]]"; do
    sleep 1
    elapsed=$((elapsed + 1))
    [[ $elapsed -ge $timeout ]] && return 1
  done
  return 0
}

is_port_listen() {
  ss -ltn 2>/dev/null | grep -qE "[:.]$1[[:space:]]"
}

# ── Subcommands ────────────────────────────────────────────────────
cmd_start() {
  log "checking docker infra (redis/etcd/mysql/kafka)..."
  for c in redis etcd mysql kafka; do
    if ! docker ps --format '{{.Names}}' | grep -qx "$c"; then
      die "$c container not running (run docker compose up -d first)"
    fi
  done

  log "wiping redis residue (player_locator/login_session/etc) for clean rollout..."
  for p in 'player:session:*' 'player:*:location' 'player_to_account:*' \
           'access_token:*' 'refresh_token:*' 'login_session:*' \
           'login_step:*' 'account_lock:*' 'player_locker:*'; do
    redis-cli --scan --pattern "$p" 2>/dev/null \
      | while read -r k; do [[ -n "$k" ]] && redis-cli DEL "$k" >/dev/null; done
  done

  for tier in 1 2 3; do
    log "starting tier $tier services..."
    for svc in "${TIER_ORDER[@]}"; do
      [[ "${SVC_TIER[$svc]}" != "$tier" ]] && continue
      local bin="${REPO_ROOT}/bin/go_services/${svc}"
      [[ ! -x "$bin" ]] && die "missing binary: $bin (run --build first)"
      local cfg="${REPO_ROOT}/go/${svc}/etc/${svc}.yaml"
      [[ ! -f "$cfg" ]] && die "missing config: $cfg"

      "$bin" -f "$cfg" \
        > "${LOG_DIR}/go_services/${svc}.log" 2>&1 &
      echo $! > "${PID_DIR}/${svc}.pid"
      log "  $svc (pid $(cat "${PID_DIR}/${svc}.pid"), port ${SVC_PORT[$svc]})"
    done

    log "waiting for tier $tier ports..."
    for svc in "${TIER_ORDER[@]}"; do
      [[ "${SVC_TIER[$svc]}" != "$tier" ]] && continue
      if ! wait_for_port "${SVC_PORT[$svc]}" 30; then
        die "$svc port ${SVC_PORT[$svc]} did not LISTEN within 30s; check ${LOG_DIR}/go_services/${svc}.log"
      fi
    done
  done

  log "starting cpp scene + gate (if binaries present)..."
  if [[ -x "${REPO_ROOT}/bin/scene" ]]; then
    "${REPO_ROOT}/bin/scene" > "${LOG_DIR}/cpp_nodes/scene.log" 2>&1 &
    echo $! > "${PID_DIR}/scene.pid"
    sleep 2
  else
    log "  WARN: bin/scene missing — skipping (build cpp before re-running)"
  fi
  if [[ -x "${REPO_ROOT}/bin/gate" ]]; then
    "${REPO_ROOT}/bin/gate" > "${LOG_DIR}/cpp_nodes/gate.log" 2>&1 &
    echo $! > "${PID_DIR}/gate.pid"
    sleep 5
  else
    log "  WARN: bin/gate missing — skipping"
  fi

  if [[ -f "${REPO_ROOT}/java/gateway_node/target/gateway-node-0.0.1-SNAPSHOT.jar" ]]; then
    log "starting Java Gateway on :${GATEWAY_PORT}..."
    java -jar "${REPO_ROOT}/java/gateway_node/target/gateway-node-0.0.1-SNAPSHOT.jar" \
      > "${LOG_DIR}/java/gateway.log" 2>&1 &
    echo $! > "${PID_DIR}/gateway.pid"
    if ! wait_for_port "$GATEWAY_PORT" 60; then
      die "gateway did not LISTEN on :$GATEWAY_PORT; check ${LOG_DIR}/java/gateway.log"
    fi
  else
    log "  WARN: gateway jar missing — run mvn package + spring-boot:repackage first"
  fi

  log "all services up. run: $0 status   |   $0 smoke"
}

cmd_stop() {
  log "stopping in reverse tier order..."
  for svc in gateway gate scene login scene_manager player_locator data_service db; do
    local pidfile="${PID_DIR}/${svc}.pid"
    if [[ -f "$pidfile" ]]; then
      local pid; pid=$(cat "$pidfile")
      if kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        log "  $svc (pid $pid) stopped"
      fi
      rm -f "$pidfile"
    fi
  done
}

cmd_restart() { cmd_stop; sleep 2; cmd_start; }

cmd_status() {
  printf '%-18s %-8s %-10s %s\n' SERVICE PORT PID STATUS
  for svc in "${TIER_ORDER[@]}" scene gate gateway; do
    local port="${SVC_PORT[$svc]:-}"
    [[ "$svc" == gateway ]] && port="$GATEWAY_PORT"
    local pid='-' status='-'
    if [[ -f "${PID_DIR}/${svc}.pid" ]]; then
      pid=$(cat "${PID_DIR}/${svc}.pid")
      kill -0 "$pid" 2>/dev/null && status='RUNNING' || status='DEAD-PID'
    fi
    if [[ -n "$port" ]] && is_port_listen "$port"; then
      [[ "$status" == '-' ]] && status='LISTENING(no-pidfile)'
    fi
    printf '%-18s %-8s %-10s %s\n' "$svc" "${port:-?}" "$pid" "$status"
  done
}

cmd_smoke() {
  log "smoke test: POST /api/login..."
  local resp
  resp=$(curl -fsS -m 5 -X POST "http://${GATEWAY_HOST}:${GATEWAY_PORT}/api/login" \
    -H 'Content-Type: application/json' \
    -d '{"zone_id":1,"account":"deploy_smoke","password":"123456"}' 2>&1) \
    || die "smoke failed: $resp"
  echo "$resp"
  if echo "$resp" | grep -q '"code":0'; then
    log "smoke OK: code=0, tokens issued"
  else
    die "smoke returned non-zero code: $resp"
  fi
}

cmd_stress() {
  shift  # drop the "stress" arg
  local script="${REPO_ROOT}/tools/scripts/stress-linux-tier.sh"
  [[ -x "$script" ]] || die "stress runner missing: $script"
  log "delegating to stress-linux-tier.sh $*"
  exec "$script" "$@"
}

# ── Dispatch ───────────────────────────────────────────────────────
case "${1:-status}" in
  start)   cmd_start ;;
  stop)    cmd_stop ;;
  restart) cmd_restart ;;
  status)  cmd_status ;;
  smoke)   cmd_smoke ;;
  stress)  cmd_stress "$@" ;;
  *)
    cat <<EOF
usage: $0 {start|stop|restart|status|smoke|stress [tiers...]}

  start          tier-staged startup of go services + cpp gate/scene + Java Gateway
  stop           reverse-order shutdown using PID files in run/pids/
  restart        stop + 2s + start
  status         table of service / port / pid / state (RUNNING|DEAD-PID|LISTENING)
  smoke          one curl to /api/login asserting code=0
  stress N M K   delegate to tools/scripts/stress-linux-tier.sh with the given tiers
EOF
    exit 2
    ;;
esac
