package logic

import (
	"context"
	"fmt"
	kafkacontracts "proto/contracts/kafka"
	game "scene_manager/generated/pb/game"
	"scene_manager/internal/constants"
	"scene_manager/internal/metrics"
	"strconv"
	"time"

	"proto/scene_manager"
	"scene_manager/internal/svc"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

type EnterSceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewEnterSceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *EnterSceneLogic {
	return &EnterSceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func errResp(code uint32, msg string) *scene_manager.EnterSceneResponse {
	return &scene_manager.EnterSceneResponse{ErrorCode: code, ErrorMessage: msg}
}

// EnterScene routes a player into a scene, managed by SceneManager.
func (l *EnterSceneLogic) EnterScene(in *scene_manager.EnterSceneRequest) (*scene_manager.EnterSceneResponse, error) {
	// 0. REQUEST-LEVEL DEDUPLICATION: If caller provided a request_id,
	//    use Redis SET NX to guarantee at-most-once processing.
	if in.RequestId != "" {
		dedupStart := time.Now()
		dedupeKey := fmt.Sprintf("enter_scene:dedup:%s", in.RequestId)
		ok, err := l.svcCtx.Redis.SetnxEx(dedupeKey, "1", 60)
		metrics.ObserveEnterSceneStage(in.GateZoneId, metrics.EnterSceneStageDedup, time.Since(dedupStart))
		if err != nil {
			l.Logger.Errorf("Dedup Redis error (non-fatal, proceeding): %v", err)
		} else if !ok {
			l.Logger.Infof("Duplicate request_id %s for player %d, skipping", in.RequestId, in.PlayerId)
			return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
		}
	}

	// 1. Determine the target zone.
	//    - Caller provided zone_id: use it (explicit cross-zone teleport).
	//    - sceneId != 0, no zone_id: look up scene:{id}:zone (reconnect / follow).
	//    - sceneId == 0, no zone_id: fall back to existing PlayerLocation zone,
	//      then to GateZoneId (first login — gate zone = home zone).
	targetZoneId := in.ZoneId
	if targetZoneId == 0 && in.SceneId != 0 {
		targetZoneId = GetSceneZone(l.svcCtx, in.SceneId)
	}
	currentLoc, locErr := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if targetZoneId == 0 {
		if locErr == nil && currentLoc != nil && currentLoc.ZoneId != 0 {
			targetZoneId = currentLoc.ZoneId
		}
	}
	if targetZoneId == 0 {
		targetZoneId = in.GateZoneId
	}

	// 2. Resolve the target scene (sceneId + nodeId).
	resolveStart := time.Now()
	sceneId, nodeId, reserved, err := l.resolveSceneForEnter(in.SceneId, in.SceneConfId, targetZoneId)
	metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageSceneResolve, time.Since(resolveStart))
	if err != nil {
		return errResp(constants.ErrNoAvailableNode, err.Error()), nil
	}

	// 3. CROSS-ZONE CHECK: If gate is in a different zone, redirect the player.
	if in.GateZoneId != 0 && targetZoneId != 0 && in.GateZoneId != targetZoneId {
		crossStart := time.Now()
		// The reserve in step 2 may have already INCR'd the target scene's
		// counters; release them before bailing out so the redirect path
		// doesn't leak. The follow-up EnterScene from the new gate will
		// re-reserve from scratch.
		if reserved {
			DecrInstancePlayerCount(l.svcCtx, sceneId)
		}
		// Historically the player-was-in-another-zone case never decremented
		// the old scene's counter on redirect, so the old zone's
		// instance:{id}:player_count slowly drifted up under cross-zone
		// teleport. Drop it here while we have the location loaded.
		if currentLoc != nil && currentLoc.SceneId != 0 && currentLoc.SceneId != sceneId {
			DecrInstancePlayerCount(l.svcCtx, currentLoc.SceneId)
		}
		resp, redirErr := l.handleCrossZoneRedirect(in, targetZoneId)
		metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageCrossZone, time.Since(crossStart))
		return resp, redirErr
	}

	// 4. IDEMPOTENCY CHECK: Is player already in the same scene on the same node?
	//    Location is unchanged, but we must still route the gate so the new
	//    session/connection is wired to the correct scene node.
	if locErr == nil && currentLoc != nil {
		if currentLoc.SceneId == sceneId && currentLoc.NodeId == nodeId {
			if reserved {
				DecrInstancePlayerCount(l.svcCtx, sceneId)
			}
			l.Logger.Infof("Player %d already in scene %d, sending route to gate (reconnect)", in.PlayerId, sceneId)
			if in.GateId != "" {
				if err := l.routePlayerToGate(in, nodeId, sceneId); err != nil {
					l.Logger.Errorf("Failed to route player %d to gate (non-fatal): %v", in.PlayerId, err)
				}
			}
			return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
		}
	}

	if !reserved {
		reserveStart := time.Now()
		count, err := AtomicIncrPlayerCountIfSceneExists(l.svcCtx, sceneId)
		if err != nil {
			metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageReserve, time.Since(reserveStart))
			return errResp(constants.ErrUpdateLocation, fmt.Sprintf("reserve scene player count failed: %v", err)), nil
		}
		if count < 0 {
			metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageReserve, time.Since(reserveStart))
			return errResp(constants.ErrNoAvailableNode, fmt.Sprintf("scene %d disappeared during enter", sceneId)), nil
		}
		// AtomicIncrPlayerCountIfSceneExists only bumps the per-scene
		// counter — bump the per-node aggregate to match what
		// IncrInstancePlayerCount/ReserveBestWorldChannel do, so the
		// composite-load score stays consistent across both reservation
		// paths. Don't double-call IncrInstancePlayerCount here: that
		// would re-INCR the per-scene counter the Lua already moved.
		if nodeId != "" {
			l.svcCtx.Redis.Incr(fmt.Sprintf(NodePlayerCountKey, nodeId))
		}
		metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageReserve, time.Since(reserveStart))
	}

	// 5b. Cross-node switch: notify the old scene node to release the player so
	// it persists state and tears down the entity. Same-node switches skip this
	// because the C++ node reuses the in-memory entity directly.
	//
	// Round 14: dispatchReleasePlayer is fully async — caller returns immediately
	// while the release + retry chain runs in a background goroutine. Previously
	// the sync 500ms deadline pinned EnterScene latency at ~535ms under 45k-bot
	// load, cascading into 71k robot-side scene-ready timeouts. AFK cleanup on
	// the old node remains the final fallback.
	if currentLoc != nil && currentLoc.NodeId != "" && currentLoc.NodeId != nodeId {
		relStart := time.Now()
		dispatchReleasePlayer(l.svcCtx, l.Logger, targetZoneId,
			currentLoc.NodeId, in.PlayerId, sceneId, nodeId)
		metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageReleaseDispatch, time.Since(relStart))
	}

	// 6. Update Player Location (Source of Truth)
	updStart := time.Now()
	if err := UpdatePlayerLocation(l.ctx, l.svcCtx, in.PlayerId, sceneId, nodeId, targetZoneId); err != nil {
		metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageUpdateLoc, time.Since(updStart))
		DecrInstancePlayerCount(l.svcCtx, sceneId)
		l.Logger.Errorf("Failed to update player location: %v", err)
		return errResp(constants.ErrUpdateLocation, "Failed to update location"), nil
	}
	metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageUpdateLoc, time.Since(updStart))

	// 6b. Decrement old scene instance count after the new reservation is
	// committed so concurrent world-channel selection sees the in-flight enter.
	if currentLoc != nil && currentLoc.SceneId != 0 && currentLoc.SceneId != sceneId {
		DecrInstancePlayerCount(l.svcCtx, currentLoc.SceneId)
	}

	// 7. Send Route Command to Gate (via Kafka) — best-effort notification.
	if in.GateId != "" {
		routeStart := time.Now()
		if err := l.routePlayerToGate(in, nodeId, sceneId); err != nil {
			l.Logger.Errorf("Failed to route player %d to gate (non-fatal): %v", in.PlayerId, err)
		}
		metrics.ObserveEnterSceneStage(targetZoneId, metrics.EnterSceneStageRouteGate, time.Since(routeStart))
	} else {
		l.Logger.Infof("No GateID in EnterScene request for player %d", in.PlayerId)
	}

	l.Logger.Infof("Player %d entered scene %d on node %s (zone %d)", in.PlayerId, sceneId, nodeId, targetZoneId)
	return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
}

// handleCrossZoneRedirect assigns a Gate in the target zone and returns redirect info.
// It also sends a RedirectToGateEvent to the current Gate via Kafka so the Gate can
// push the redirect to the client immediately.
func (l *EnterSceneLogic) handleCrossZoneRedirect(in *scene_manager.EnterSceneRequest, targetZoneId uint32) (*scene_manager.EnterSceneResponse, error) {
	l.Logger.Infof("Cross-zone detected: player %d gate_zone=%d target_zone=%d, redirecting",
		in.PlayerId, in.GateZoneId, targetZoneId)

	redirect, err := AssignGateForZone(l.ctx, l.svcCtx, targetZoneId)
	if err != nil {
		l.Logger.Errorf("Cross-zone redirect failed for player %d: %v", in.PlayerId, err)
		return errResp(constants.ErrNoAvailableNode, "no gate available in target zone"), nil
	}

	// Best-effort: push redirect event to current Gate via Kafka.
	if in.GateId != "" {
		if err := l.sendRedirectToGate(in, redirect); err != nil {
			l.Logger.Errorf("Failed to push redirect to gate (non-fatal): %v", err)
		}
	}

	return &scene_manager.EnterSceneResponse{
		ErrorCode: 0,
		Redirect:  redirect,
	}, nil
}

// sendRedirectToGate pushes a RedirectToGateEvent to the current Gate via Kafka.
func (l *EnterSceneLogic) sendRedirectToGate(in *scene_manager.EnterSceneRequest, redirect *scene_manager.RedirectToGateInfo) error {
	targetGateId, err := strconv.ParseUint(in.GateId, 10, 32)
	if err != nil {
		return fmt.Errorf("invalid gate id %q: %w", in.GateId, err)
	}

	event := &kafkacontracts.RedirectToGateEvent{
		PlayerId:       in.PlayerId,
		SessionId:      in.SessionId,
		TargetGateIp:   redirect.TargetGateIp,
		TargetGatePort: redirect.TargetGatePort,
		TokenPayload:   redirect.TokenPayload,
		TokenSignature: redirect.TokenSignature,
		TokenDeadline:  redirect.TokenDeadline,
	}
	payload, err := proto.Marshal(event)
	if err != nil {
		return fmt.Errorf("marshal RedirectToGateEvent: %w", err)
	}

	cmd := &kafkacontracts.GateCommand{
		PlayerId:         in.PlayerId,
		SessionId:        in.SessionId,
		TargetGateId:     uint32(targetGateId),
		TargetInstanceId: in.GateInstanceId,
		Payload:          payload,
		EventId:          uint32(game.ContractsKafkaRedirectToGateEventEventId),
	}
	bytes, err := proto.Marshal(cmd)
	if err != nil {
		return fmt.Errorf("marshal GateCommand: %w", err)
	}

	topic := GateTopicName(in.GateId)
	writeCtx, cancel := context.WithTimeout(context.Background(), time.Duration(l.svcCtx.Config.KafkaWriteTimeoutSeconds)*time.Second)
	defer cancel()
	if err := l.svcCtx.Kafka.WriteMessages(writeCtx, kafkago.Message{
		Topic: topic,
		Key:   []byte(fmt.Sprintf("%d", in.PlayerId)),
		Value: bytes,
	}); err != nil {
		return fmt.Errorf("push to Kafka topic %s: %w", topic, err)
	}

	l.Logger.Infof("Pushed RedirectToGate to Kafka topic %s for player %d", topic, in.PlayerId)
	return nil
}

// resolveScene resolves the concrete (sceneId, nodeId) for an EnterScene request.
//   - sceneId != 0: direct lookup — the caller already knows the exact scene instance.
//   - sceneId == 0, sceneConfId != 0: auto-select least-loaded channel for the map.
//   - sceneId == 0, sceneConfId == 0: fallback to the first configured main scene.
func (l *EnterSceneLogic) resolveScene(sceneId uint64, sceneConfId uint64, zoneId uint32) (uint64, string, error) {
	// Case 1: exact scene instance specified.
	if sceneId != 0 {
		key := fmt.Sprintf("scene:%d:node", sceneId)
		nodeId, err := l.svcCtx.Redis.Get(key)
		if err != nil {
			return 0, "", fmt.Errorf("scene lookup failed: %w", err)
		}
		// Validate the mapped node is still alive; reassign if dead.
		if !IsNodeAlive(l.svcCtx, zoneId, nodeId) {
			// Pick a replacement of the correct purpose so a world scene
			// never gets moved onto an instance-only node (C++ EnterScene
			// would then reject the request on type mismatch).
			purpose := constants.NodePurposeInstance
			if sceneConfId != 0 && IsWorldConf(sceneConfId) {
				purpose = constants.NodePurposeWorld
			}
			newNodeId, err := GetBestNodeForPurpose(l.ctx, l.svcCtx, zoneId, purpose)
			if err != nil {
				return 0, "", fmt.Errorf("scene %d mapped to dead node %s and no live nodes for purpose %d: %w", sceneId, nodeId, purpose, err)
			}
			l.Logger.Infof("resolveScene: scene %d was on dead node %s, reassigning to %s (purpose=%d)", sceneId, nodeId, newNodeId, purpose)
			l.svcCtx.Redis.Set(key, newNodeId)
			nodeId = newNodeId

			// Ensure the new node creates the ECS scene entity.
			if sceneConfId != 0 {
				if _, err := RequestNodeCreateScene(l.ctx, l.svcCtx, nodeId, uint32(sceneConfId), sceneId); err != nil {
					l.Logger.Errorf("resolveScene: CreateScene on new node %s for scene %d failed: %v", nodeId, sceneId, err)
				}
			}
		}
		return sceneId, nodeId, nil
	}

	// Need a scene_conf_id to allocate.
	if sceneConfId == 0 {
		if wids := worldConfIds(); len(wids) > 0 {
			sceneConfId = wids[0]
		} else {
			return 0, "", fmt.Errorf("no scene_conf_id provided and no default world scene configured")
		}
	}

	// Case 2: auto-select least-loaded channel.
	sid, nid, err := GetBestWorldChannel(l.ctx, l.svcCtx, sceneConfId, zoneId)
	if err != nil || sid == 0 {
		return 0, "", fmt.Errorf("no available channel for conf %d in zone %d", sceneConfId, zoneId)
	}
	return sid, nid, nil
}

func (l *EnterSceneLogic) resolveSceneForEnter(sceneId uint64, sceneConfId uint64, zoneId uint32) (uint64, string, bool, error) {
	if sceneId != 0 {
		sid, nid, err := l.resolveScene(sceneId, sceneConfId, zoneId)
		return sid, nid, false, err
	}

	if sceneConfId == 0 {
		if wids := worldConfIds(); len(wids) > 0 {
			sceneConfId = wids[0]
		} else {
			return 0, "", false, fmt.Errorf("no scene_conf_id provided and no default world scene configured")
		}
	}

	sid, nid, err := ReserveBestWorldChannelForEnter(l.ctx, l.svcCtx, sceneConfId, zoneId)
	if err != nil || sid == 0 {
		return 0, "", false, fmt.Errorf("no available channel for conf %d in zone %d", sceneConfId, zoneId)
	}
	return sid, nid, true, nil
}

// routePlayerToGate builds a GateCommand and pushes it to the gate's Kafka topic.
func (l *EnterSceneLogic) routePlayerToGate(in *scene_manager.EnterSceneRequest, nodeId string, sceneId uint64) error {
	targetNodeId, err := strconv.ParseUint(nodeId, 10, 32)
	if err != nil {
		return fmt.Errorf("invalid scene node id %q: %w", nodeId, err)
	}
	targetGateId, err := strconv.ParseUint(in.GateId, 10, 32)
	if err != nil {
		return fmt.Errorf("invalid gate id %q: %w", in.GateId, err)
	}

	event := &kafkacontracts.RoutePlayerEvent{
		SessionId:    in.SessionId,
		TargetNodeId: uint32(targetNodeId),
		SceneId:      sceneId,
		PlayerId:     in.PlayerId,
	}
	payload, err := proto.Marshal(event)
	if err != nil {
		return fmt.Errorf("marshal RoutePlayerEvent: %w", err)
	}

	cmd := &kafkacontracts.GateCommand{
		PlayerId:         in.PlayerId,
		TargetNodeId:     uint32(targetNodeId),
		SessionId:        in.SessionId,
		Payload:          payload,
		TargetGateId:     uint32(targetGateId),
		TargetInstanceId: in.GateInstanceId,
		EventId:          uint32(game.ContractsKafkaRoutePlayerEventEventId),
	}
	bytes, err := proto.Marshal(cmd)
	if err != nil {
		return fmt.Errorf("marshal GateCommand: %w", err)
	}

	topic := GateTopicName(in.GateId)
	writeCtx, cancel := context.WithTimeout(context.Background(), time.Duration(l.svcCtx.Config.KafkaWriteTimeoutSeconds)*time.Second)
	defer cancel()
	if err := l.svcCtx.Kafka.WriteMessages(writeCtx, kafkago.Message{
		Topic: topic,
		Key:   []byte(fmt.Sprintf("%d", in.PlayerId)),
		Value: bytes,
	}); err != nil {
		return fmt.Errorf("push to Kafka topic %s: %w", topic, err)
	}

	l.Logger.Infof("Pushed RoutePlayer to Kafka topic %s for player %d -> node %s", topic, in.PlayerId, nodeId)
	return nil
}
