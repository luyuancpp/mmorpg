package logic

import (
	"context"
	kafkacontracts "proto/contracts/kafka"
	"fmt"
	game "scene_manager/generated/pb/game"
	"strconv"

	"scene_manager/internal/svc"
	"proto/scene_manager"

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

// EnterScene routes a player into a scene, managed by SceneManager.
func (l *EnterSceneLogic) EnterScene(in *scene_manager.EnterSceneRequest) (*scene_manager.EnterSceneResponse, error) {
	// 1. Check if scene is on this node
	key := fmt.Sprintf("scene:%d:node", in.SceneId)
	nodeId, err := l.svcCtx.Redis.Get(key)
	if err != nil {
		l.Logger.Errorf("Scene lookup failed: %v", err)
		return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Scene lookup failed"}, nil
	}
	if nodeId != l.svcCtx.Config.NodeID {
		l.Logger.Errorf("Scene %d not on this node (expected %s, got %s)", in.SceneId, nodeId, l.svcCtx.Config.NodeID)
		return &scene_manager.EnterSceneResponse{ErrorCode: 2, ErrorMessage: "Scene not on this node"}, nil
	}

	// 2. IDEMPOTENCY CHECK: Is player already here?
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err == nil && currentLoc != nil {
		if currentLoc.SceneId == in.SceneId && currentLoc.NodeId == l.svcCtx.Config.NodeID {
			// Already in the correct scene. Treat as success.
			l.Logger.Infof("Player %d already in scene %d, idempotent success", in.PlayerId, in.SceneId)
			return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
		}
	}

	// 3. Update Player Location (Source of Truth)
	err = UpdatePlayerLocation(l.ctx, l.svcCtx, in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)
	if err != nil {
		l.Logger.Errorf("Failed to update player location: %v", err)
		return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Failed to update location"}, nil
	}

	// 4. Send Route Command to Gate (via Kafka)
	if in.GateId != "" {
		targetNodeId, convErr := strconv.ParseUint(l.svcCtx.Config.NodeID, 10, 32)
		if convErr != nil {
			l.Logger.Errorf("Invalid SceneManager node id %q: %v", l.svcCtx.Config.NodeID, convErr)
			return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Invalid scene manager node id"}, nil
		}

		targetGateId, convErr := strconv.ParseUint(in.GateId, 10, 32)
		if convErr != nil {
			l.Logger.Errorf("Invalid GateID %q: %v", in.GateId, convErr)
			return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Invalid gate id"}, nil
		}

		eventId := uint32(game.ContractsKafkaRoutePlayerEventEventId)
		event := &kafkacontracts.RoutePlayerEvent{
			SessionId:    in.SessionId,
			TargetNodeId: uint32(targetNodeId),
		}

		payload, err := proto.Marshal(event)
		if err != nil {
			l.Logger.Errorf("Failed to marshal RoutePlayerEvent: %v", err)
			return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Failed to encode route event"}, nil
		}

		cmd := &kafkacontracts.GateCommand{
			PlayerId:         in.PlayerId,
			TargetNodeId:     uint32(targetNodeId),
			SessionId:        in.SessionId,
			Payload:          payload,
			TargetGateId:     uint32(targetGateId),
			TargetInstanceId: in.GateInstanceId,
			EventId:          eventId,
		}

		bytes, err := proto.Marshal(cmd)
		if err != nil {
			l.Logger.Errorf("Failed to marshal GateCommand: %v", err)
			return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Failed to encode gate command"}, nil
		}

		topic := fmt.Sprintf("gate-%s", in.GateId)
		err = l.svcCtx.Kafka.WriteMessages(l.ctx, kafkago.Message{
			Topic: topic,
			Key:   []byte(fmt.Sprintf("%d", in.PlayerId)),
			Value: bytes,
		})
		if err != nil {
			l.Logger.Errorf("Failed to push to Kafka topic %s: %v", topic, err)
			return &scene_manager.EnterSceneResponse{ErrorCode: 1, ErrorMessage: "Failed to route player to gate"}, nil
		}
		l.Logger.Infof("Pushed RoutePlayer to Kafka topic %s for player %d -> node %s", topic, in.PlayerId, l.svcCtx.Config.NodeID)
	} else {
		l.Logger.Infof("No GateID in EnterScene request for player %d", in.PlayerId)
	}

	l.Logger.Infof("Player %d entered scene %d on node %s", in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)

	return &scene_manager.EnterSceneResponse{ErrorCode: 0}, nil
}
