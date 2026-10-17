package logic

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"sync"
	"time"

	scenepb "proto/scene"
	scenenodepb "proto/scene_manager"

	"scene_manager/internal/metrics"
	"scene_manager/internal/svc"
	"shared/safego"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// nodeConnCache caches gRPC connections to C++ scene nodes, keyed by
// (zoneId,nodeId). Node IDs are allocated independently in every zone.
// Connections are reused across calls and cleaned up on node removal.
var (
	nodeConnMu    sync.RWMutex
	nodeConnCache = make(map[string]*grpc.ClientConn)
)

// nodeDialer builds a gRPC ClientConn for the given endpoint. Swappable so
// integration tests can point the whole RPC pipeline at a bufconn-backed
// fake SceneNode server without standing up TCP listeners. Production
// callers never touch this; the default is grpc.NewClient over insecure
// transport (scene manager ↔ scene node is on an internal overlay).
var nodeDialer = defaultNodeDialer

func defaultNodeDialer(_ context.Context, endpoint string) (*grpc.ClientConn, error) {
	return grpc.NewClient(endpoint, grpc.WithTransportCredentials(insecure.NewCredentials()))
}

// SetNodeDialerForTest installs a custom dialer and returns a restore func.
// MUST be called from *_test.go via t.Cleanup so a failing test doesn't
// leak a bufconn dialer into sibling packages.
func SetNodeDialerForTest(d func(context.Context, string) (*grpc.ClientConn, error)) (restore func()) {
	prev := nodeDialer
	nodeDialer = d
	return func() { nodeDialer = prev }
}

// nodeEndpointOverride, when non-nil, replaces the etcd/knownNodes endpoint
// lookup. Same rationale as nodeDialer: tests need the whole RPC pipeline to
// reach an in-process fake without registering a knownNodes entry for every
// synthetic node id (several tests use non-numeric ids like "hot"/"A").
// Production never sets this.
var nodeEndpointOverride func(zoneId uint32, nodeId string) (string, bool)

// SetNodeEndpointResolverForTest installs an endpoint resolver override and
// returns a restore func. MUST be paired with t.Cleanup.
func SetNodeEndpointResolverForTest(r func(zoneId uint32, nodeId string) (string, bool)) (restore func()) {
	prev := nodeEndpointOverride
	nodeEndpointOverride = r
	return func() { nodeEndpointOverride = prev }
}

// ResetNodeConnCacheForTest evicts every cached gRPC ClientConn. Integration
// tests reuse nodeIDs across sub-tests (node-A in test 1 is an unrelated
// bufconn in test 2); without eviction the second test would dial the
// first test's already-closed listener and hang on RPC.
func ResetNodeConnCacheForTest() {
	nodeConnMu.Lock()
	defer nodeConnMu.Unlock()
	for id, c := range nodeConnCache {
		_ = c.Close()
		delete(nodeConnCache, id)
	}
}

// RequestNodeCreateScene dials the C++ scene node identified by nodeId and calls
// CreateScene to instantiate the ECS scene entity.
// sceneId is the Go-allocated unique ID; C++ uses it for per-scene idempotency.
// If the node is unreachable, the error is returned but Redis state is already committed.
func RequestNodeCreateScene(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, nodeId string, configId uint32, sceneId uint64) (*scenepb.CreateSceneResponse, error) {
	return RequestNodeCreateSceneWithOptions(ctx, svcCtx, zoneId, nodeId, configId, sceneId, 0, nil)
}

// RequestNodeCreateSceneWithOptions is the full-featured variant that also
// forwards mirror_config_id and creator_ids to the C++ SceneNode. The basic
// RequestNodeCreateScene remains for world-scene paths that never carry mirror
// or creator metadata.
func RequestNodeCreateSceneWithOptions(
	ctx context.Context,
	svcCtx *svc.ServiceContext,
	zoneId uint32,
	nodeId string,
	configId uint32,
	sceneId uint64,
	mirrorConfigId uint32,
	creatorIds []uint64,
) (*scenepb.CreateSceneResponse, error) {
	if sceneId == 0 {
		return nil, fmt.Errorf("CreateScene refused: scene id must be nonzero")
	}
	conn, err := getOrDialNode(ctx, svcCtx, zoneId, nodeId)
	if err != nil {
		return nil, fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	resp, err := client.CreateScene(ctx, &scenepb.CreateSceneRequest{
		ConfigId:       configId,
		SceneId:        sceneId,
		MirrorConfigId: mirrorConfigId,
		CreatorIds:     creatorIds,
	})
	if err != nil {
		return nil, fmt.Errorf("CreateScene RPC to node %s: %w", nodeId, err)
	}
	// RPC 的 OK 只代表传输完成;C++ 创建入口仍可能因参数或分配许可拒绝而留下空应答。
	// 在发布/切换场景归属之前确认实际创建的是本次请求的实例,失败沿调用方原有补偿收尾。
	if resp == nil || resp.SceneInfo == nil || resp.SceneInfo.SceneId != sceneId {
		return nil, fmt.Errorf("CreateScene node %s did not confirm requested scene %d (received %d)",
			nodeId, sceneId, resp.GetSceneInfo().GetSceneId())
	}

	return resp, nil
}

// RequestNodeDestroyScene dials the C++ scene node and calls DestroyScene
// to remove the ECS scene entity.
func RequestNodeDestroyScene(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, nodeId string, sceneId uint64) error {
	conn, err := getOrDialNode(ctx, svcCtx, zoneId, nodeId)
	if err != nil {
		return fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	if _, err := client.DestroyScene(ctx, &scenepb.DestroySceneRequest{SceneId: sceneId}); err != nil {
		return fmt.Errorf("DestroyScene RPC to node %s: %w", nodeId, err)
	}

	return nil
}

// RequestNodeReleasePlayer dials the C++ scene node identified by nodeId
// (the player's previous scene node) and asks it to release the player so
// the new node can load fresh state from Redis. Used during cross-node
// scene switches; same-node switches do not need this.
//
// expectedOwnerEpoch 是 SM 本次 EnterScene 观察到、并用于落点 CAS 的 owner_epoch(scene-enter-chain-residuals.md
// §4.2 ②):scene 只在本地缓存恰好等于它时才执行退出,0 = 按旧行为释放。
func RequestNodeReleasePlayer(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, nodeId string, playerId uint64,
	targetSceneId uint64, targetNodeId string, expectedOwnerEpoch uint64) error {
	conn, err := getOrDialNode(ctx, svcCtx, zoneId, nodeId)
	if err != nil {
		return fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	if _, err := client.ReleasePlayer(ctx, &scenenodepb.ReleasePlayerRequest{
		PlayerId:           playerId,
		TargetSceneId:      targetSceneId,
		TargetNodeId:       targetNodeId,
		ExpectedOwnerEpoch: expectedOwnerEpoch,
	}); err != nil {
		return fmt.Errorf("ReleasePlayer RPC to node %s (expected_owner_epoch=%d): %w", nodeId, expectedOwnerEpoch, err)
	}

	return nil
}

// getOrDialNode returns a cached connection or discovers the node endpoint
// from etcd and dials it.
func nodeIdentity(zoneId uint32, nodeId string) string {
	return fmt.Sprintf("%d/%s", zoneId, nodeId)
}

func getOrDialNode(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, nodeId string) (*grpc.ClientConn, error) {
	identity := nodeIdentity(zoneId, nodeId)
	// 不能让既有 cache 命中掩盖后来出现的重复注册。Create/Destroy/Release
	// 发到任意一个同号进程都会破坏 scene ownership，必须先拒绝身份歧义。
	if matches := knownNodeIdentityMatchCount(zoneId, nodeId); matches > 1 {
		return nil, fmt.Errorf("ambiguous scene node identity zone=%d node=%s has %d registrations", zoneId, nodeId, matches)
	}
	nodeConnMu.RLock()
	conn, ok := nodeConnCache[identity]
	nodeConnMu.RUnlock()
	if ok {
		return conn, nil
	}

	// Discover endpoint from etcd.
	endpoint, err := resolveNodeEndpoint(ctx, svcCtx, zoneId, nodeId)
	if err != nil {
		return nil, err
	}

	conn, err = nodeDialer(ctx, endpoint)
	if err != nil {
		return nil, fmt.Errorf("grpc dial %s: %w", endpoint, err)
	}

	nodeConnMu.Lock()
	// Double-check: another goroutine may have added it.
	if existing, ok := nodeConnCache[identity]; ok {
		nodeConnMu.Unlock()
		conn.Close()
		return existing, nil
	}
	nodeConnCache[identity] = conn
	nodeConnMu.Unlock()

	logx.Infof("[SceneNodeClient] Connected to scene node %s at %s", nodeId, endpoint)
	return conn, nil
}

// resolveNodeEndpoint returns the "ip:port" gRPC address for a scene node.
// It first checks the in-memory knownNodes map (maintained by the watch loop),
// then falls back to a fresh etcd query.
func resolveNodeEndpoint(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, nodeId string) (string, error) {
	if nodeEndpointOverride != nil {
		if ep, ok := nodeEndpointOverride(zoneId, nodeId); ok {
			return ep, nil
		}
	}

	// Fast path: use the watch-maintained in-memory registry.
	if ep, ok, knownErr := resolveFromKnownNodes(zoneId, nodeId); knownErr != nil {
		return "", knownErr
	} else if ok {
		return ep, nil
	}

	// Slow path: query etcd directly (covers race at startup before first fullSync).
	if svcCtx.Etcd == nil {
		return "", fmt.Errorf("scene node %s not in registry and etcd client unavailable", nodeId)
	}
	prefix := "SceneNodeService.rpc/"
	resp, err := svcCtx.Etcd.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return "", fmt.Errorf("etcd get %s: %w", prefix, err)
	}

	var matchedEndpoint string
	matched := 0
	for _, kv := range resp.Kvs {
		var reg sceneNodeRegistration
		if err := json.Unmarshal(kv.Value, &reg); err != nil {
			continue
		}
		if reg.ZoneId == zoneId && fmt.Sprintf("%d", reg.NodeId) == nodeId {
			matched++
			if reg.GrpcEndpoint.Port > 0 {
				endpoint := fmt.Sprintf("%s:%d", reg.GrpcEndpoint.IP, reg.GrpcEndpoint.Port)
				if matchedEndpoint != "" && matchedEndpoint != endpoint {
					return "", fmt.Errorf("ambiguous scene node identity zone=%d node=%s has multiple grpc endpoints", zoneId, nodeId)
				}
				matchedEndpoint = endpoint
				continue
			}
			return "", fmt.Errorf("scene node %s has no grpc_endpoint (would fall back to raw TCP port, causing protocol mismatch)", nodeId)
		}
	}
	if matched > 1 {
		return "", fmt.Errorf("ambiguous scene node identity zone=%d node=%s has %d registrations", zoneId, nodeId, matched)
	}
	if matchedEndpoint != "" {
		return matchedEndpoint, nil
	}

	return "", fmt.Errorf("scene node %s not found in etcd", nodeId)
}

// resolveFromKnownNodes checks the in-memory node registry for the endpoint.
func resolveFromKnownNodes(zoneId uint32, nodeId string) (string, bool, error) {
	knownNodesMu.RLock()
	defer knownNodesMu.RUnlock()
	var endpoint string
	matches := 0
	for _, entry := range knownNodes {
		if entry.reg.ZoneId == zoneId && entry.nodeID == nodeId {
			matches++
			if entry.reg.GrpcEndpoint.Port > 0 {
				candidate := fmt.Sprintf("%s:%d", entry.reg.GrpcEndpoint.IP, entry.reg.GrpcEndpoint.Port)
				if endpoint != "" && endpoint != candidate {
					return "", false, fmt.Errorf("ambiguous scene node identity zone=%d node=%s has conflicting endpoints", zoneId, nodeId)
				}
				endpoint = candidate
				continue
			}
			// Do NOT fall back to entry.reg.Endpoint -- that is the raw TCP protobuf
			// port. Dialing it with gRPC sends an HTTP/2 preface which the protobuf
			// codec rejects as InvalidLength.
			return "", false, fmt.Errorf("scene node zone=%d node=%s has no grpc endpoint", zoneId, nodeId)
		}
	}
	if matches > 1 {
		return "", false, fmt.Errorf("ambiguous scene node identity zone=%d node=%s has %d registrations", zoneId, nodeId, matches)
	}
	return endpoint, matches == 1, nil
}

// RemoveNodeConn closes and removes the cached connection for a node.
// Called when a node is removed from etcd.
func RemoveNodeConn(zoneId uint32, nodeId string) {
	identity := nodeIdentity(zoneId, nodeId)
	nodeConnMu.Lock()
	defer nodeConnMu.Unlock()
	if conn, ok := nodeConnCache[identity]; ok {
		conn.Close()
		delete(nodeConnCache, identity)
		logx.Infof("[SceneNodeClient] Removed connection to scene node %s", nodeId)
	}
}

// dispatchReleasePlayer fires a ReleasePlayer notification to the player's
// previous scene node fully asynchronously.
//
// History: prior to Round 14 this did a 500ms synchronous attempt on the hot
// path so EnterScene could "feel" the old node release before returning. Under
// 45k-bot load that 500ms deadline turned into a hard cliff — every reconnect
// hit it (scene node gRPC server was contended, NUM_POLLERS=2), pinning
// scene_manager.EnterScene latency at ~535ms and starving NotifyEnterScene
// (71k robot-side scene-ready timeouts).
//
// New behaviour (Round 14):
//  1. Caller returns immediately; the entire attempt + retry chain runs in a
//     detached goroutine with context.Background() so request cancellation
//     never aborts release.
//  2. First attempt has a 1s deadline (was 500ms, but now off the hot path);
//     followed by up to 2 retries with 1s/3s backoff and 1s deadline each.
//  3. Every terminal outcome (ok / retry_ok / timeout / error) is reported to
//     Prometheus via metrics.ObserveReleasePlayer.
//
// Failures are non-fatal — the C++ scene node's AFK sweeper is the final
// safety net for any player who is never released.
//
// 重试按代际幂等(scene-enter-chain-residuals.md §4.2):首发与每次重试都带同一个 expectedOwnerEpoch,
// scene 侧按「本地缓存 == 期望值才退出」裁决,所以迟到的重试不会退掉已采纳回滚(E+2)或回流后重新载入的实体;
// 被忽略的释放也回 Empty,重试不会改变结论。调用方(EnterScene 第 7b 步)只在本请求提交之后才派发。
func dispatchReleasePlayer(svcCtx *svc.ServiceContext, log logx.Logger, oldZoneID uint32,
	oldNodeId string, playerId uint64, targetSceneId uint64, targetNodeId string, expectedOwnerEpoch uint64) {
	// safego.Go 而不是裸 go func:这条链路挂在 EnterScene 热路径上,45k 并发时
	// 每次跨节点交接都会派生一条。裸写法里任何一次 panic(比如 nil svcCtx.Kafka、
	// 节点连接缓存被并发改坏)都会当场打死整个进程,而日志里只剩一段 runtime 栈,
	// 看不出是哪条后台链路炸的。
	// 退避表在派发时取一次快照,单测替换 releasePlayerRetryBackoffs 不会与在途协程竞争。
	backoffs := releasePlayerRetryBackoffs
	safego.Go(SafePointReleasePlayer, func() {
		attemptCtx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
		err := RequestNodeReleasePlayer(attemptCtx, svcCtx, oldZoneID, oldNodeId, playerId, targetSceneId, targetNodeId, expectedOwnerEpoch)
		cancel()

		if err == nil {
			metrics.ObserveReleasePlayer(oldZoneID, "ok")
			return
		}

		logx.Errorf("ReleasePlayer first attempt failed for player %d on old node %s expected_owner_epoch=%d (will retry in background): %v",
			playerId, oldNodeId, expectedOwnerEpoch, err)

		var lastErr = err
		for i, backoff := range backoffs {
			time.Sleep(backoff)
			retryCtx, retryCancel := context.WithTimeout(context.Background(), 1*time.Second)
			lastErr = RequestNodeReleasePlayer(retryCtx, svcCtx, oldZoneID, oldNodeId, playerId, targetSceneId, targetNodeId, expectedOwnerEpoch)
			retryCancel()
			if lastErr == nil {
				logx.Infof("[ReleasePlayer] Retry %d succeeded for player %d on old node %s expected_owner_epoch=%d",
					i+1, playerId, oldNodeId, expectedOwnerEpoch)
				metrics.ObserveReleasePlayer(oldZoneID, "retry_ok")
				return
			}
			logx.Errorf("[ReleasePlayer] Retry %d failed for player %d on old node %s expected_owner_epoch=%d: %v",
				i+1, playerId, oldNodeId, expectedOwnerEpoch, lastErr)
		}
		outcome := "error"
		if errors.Is(lastErr, context.DeadlineExceeded) {
			outcome = "timeout"
		}
		metrics.ObserveReleasePlayer(oldZoneID, outcome)
		logx.Errorf("[ReleasePlayer] All retries exhausted for player %d on old node %s expected_owner_epoch=%d; relying on AFK cleanup. last_err=%v",
			playerId, oldNodeId, expectedOwnerEpoch, lastErr)
	})
	_ = log // logger no longer used on hot path — kept in signature for ABI stability
}

// releasePlayerRetryBackoffs 是 dispatchReleasePlayer 首发失败后的重试间隔(最坏约 7s 送达)。
// 包级变量只为单测缩短等待;生产从不改它。
var releasePlayerRetryBackoffs = []time.Duration{1 * time.Second, 3 * time.Second}

// releaseDispatcher 是 EnterScene 第 7b 步派发 ReleasePlayer 的接缝,默认就是 dispatchReleasePlayer。
// 包级钩子只为单测能**同步**断言「派发了几次、带什么期望值、是否在路由之后」(不靠 sleep),做法同 nodeDialer;
// 生产从不改它。替换的用例必须用 t.Cleanup 还原。
var releaseDispatcher = dispatchReleasePlayer
