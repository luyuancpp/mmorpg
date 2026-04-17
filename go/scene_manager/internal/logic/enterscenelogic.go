package logic

import (
	"context"
	"fmt"
	kafkacontracts "proto/contracts/kafka"
	game "scene_manager/generated/pb/game"
	"scene_manager/internal/constants"
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
		dedupeKey := fmt.Sprintf("enter_scene:dedup:%s", in.RequestId)
		ok, err := l.svcCtx.Redis.SetnxEx(dedupeKey, "1", 60)
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
	if targetZoneId == 0 {
		if loc, _ := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId); loc != nil && loc.ZoneId != 0 {
			targetZoneId = loc.ZoneId
		}
	}
	if targetZoneId == 0 {
		targetZoneId = in.GateZoneId
	}

	// 2. Resolve the target scene (sceneId + nodeId).
	sceneId, nodeId, err := l.resolveScene(in.SceneId, in.SceneConfId, targetZoneId)
	if err != nil {
		return errResp(constants.ErrNoAvailableNode, err.Error()), nil
	}

	// 3. CROSS-ZONE CHECK: If gate is in a different zone, redirect the player.
	if in.GateZoneId != 0 && targetZoneId != 0 && in.GateZoneId != targetZoneId {
		return l.handleCrossZoneRedirect(in, targetZoneId)
	}

	// 4. IDEMPOTENCY CHECK: Is player already in the same scene on the same node?
	//    Location is unchanged, but we must still route the gate so the new
	//    session/connection is wired to the correct scene node.
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err == nil && currentLoc != nil {
		if currentLoc.SceneId == sceneId && currentLoc.NodeId == nodeId {
			l.Logger.Infof("Player %d already in scene %d, sending route to gate (reconnect)", in.PlayerId, sceneId)
			if in.GateId != "" {
				if err := l.routePlayerToGate(in, nodeId, sceneId); err != nil {
					l.Logger.Errorf("Failed to route player %d to gate (non-fatal): %v", in.PlayerId, err)
				}
			}
			return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
		}
	}

	// 5. Decrement old scene instance count if player is switching scenes.
	if currentLoc != nil && currentLoc.SceneId != 0 && currentLoc.SceneId != sceneId {
		DecrInstancePlayerCount(l.svcCtx, currentLoc.SceneId)
	}

	// 6. Update Player Location (Source of Truth)
	if err := UpdatePlayerLocation(l.ctx, l.svcCtx, in.PlayerId, sceneId, nodeId, targetZoneId); err != nil {
		l.Logger.Errorf("Failed to update player location: %v", err)
		return errResp(constants.ErrUpdateLocation, "Failed to update location"), nil
	}

	// 7. Send Route Command to Gate (via Kafka) — best-effort notification.
	if in.GateId != "" {
		if err := l.routePlayerToGate(in, nodeId, sceneId); err != nil {
			l.Logger.Errorf("Failed to route player %d to gate (non-fatal): %v", in.PlayerId, err)
		}
	} else {
		l.Logger.Infof("No GateID in EnterScene request for player %d", in.PlayerId)
	}

	// 8. Track instance player count.
	IncrInstancePlayerCount(l.svcCtx, sceneId)

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
		PlayerId:         in.PlayerId,
		SessionId:        in.SessionId,
		TargetGateIp:     redirect.TargetGateIp,
		TargetGatePort:   redirect.TargetGatePort,
		TokenPayload:     redirect.TokenPayload,
		TokenSignature:   redirect.TokenSignature,
		TokenDeadline:    redirect.TokenDeadline,
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
