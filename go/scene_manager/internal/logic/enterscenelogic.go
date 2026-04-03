package logic

import (
	"context"
	"fmt"
	kafkacontracts "proto/contracts/kafka"
	game "scene_manager/generated/pb/game"
	"scene_manager/internal/constants"
	"strconv"

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
	// 1. Resolve the target Scene node.
	nodeId, err := l.resolveNode(in.SceneId)
	if err != nil {
		return errResp(constants.ErrNoAvailableNode, err.Error()), nil
	}

	// 2. IDEMPOTENCY CHECK: Is player already here?
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err == nil && currentLoc != nil {
		if currentLoc.SceneId == in.SceneId && currentLoc.NodeId == nodeId {
			l.Logger.Infof("Player %d already in scene %d, idempotent success", in.PlayerId, in.SceneId)
			return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
		}
	}

	// 3. Update Player Location (Source of Truth)
	if err := UpdatePlayerLocation(l.ctx, l.svcCtx, in.PlayerId, in.SceneId, nodeId); err != nil {
		l.Logger.Errorf("Failed to update player location: %v", err)
		return errResp(constants.ErrUpdateLocation, "Failed to update location"), nil
	}

	// 4. Send Route Command to Gate (via Kafka) — best-effort notification.
	// The gate Kafka consumer may not be implemented yet; location in Redis
	// is the source of truth so we log but do not fail the request.
	if in.GateId != "" {
		if err := l.routePlayerToGate(in, nodeId); err != nil {
			l.Logger.Errorf("Failed to route player %d to gate (non-fatal): %v", in.PlayerId, err)
		}
	} else {
		l.Logger.Infof("No GateID in EnterScene request for player %d", in.PlayerId)
	}

	l.Logger.Infof("Player %d entered scene %d on node %s", in.PlayerId, in.SceneId, nodeId)
	return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
}

// resolveNode picks the target scene node: load-balanced for first login (sceneId=0),
// or looks up the owning node for an existing scene.
func (l *EnterSceneLogic) resolveNode(sceneId uint64) (string, error) {
	if sceneId == 0 {
		return GetBestNode(l.ctx, l.svcCtx)
	}
	key := fmt.Sprintf("scene:%d:node", sceneId)
	nodeId, err := l.svcCtx.Redis.Get(key)
	if err != nil {
		return "", fmt.Errorf("scene lookup failed: %w", err)
	}
	return nodeId, nil
}

// routePlayerToGate builds a GateCommand and pushes it to the gate's Kafka topic.
func (l *EnterSceneLogic) routePlayerToGate(in *scene_manager.EnterSceneRequest, nodeId string) error {
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
	if err := l.svcCtx.Kafka.WriteMessages(l.ctx, kafkago.Message{
		Topic: topic,
		Key:   []byte(fmt.Sprintf("%d", in.PlayerId)),
		Value: bytes,
	}); err != nil {
		return fmt.Errorf("push to Kafka topic %s: %w", topic, err)
	}

	l.Logger.Infof("Pushed RoutePlayer to Kafka topic %s for player %d -> node %s", topic, in.PlayerId, nodeId)
	return nil
}
