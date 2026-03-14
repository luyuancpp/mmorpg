package logic

import (
	"context"
	kafkacontracts "contracts/kafka"
	"fmt"
	game "generated/pb/game"
	"strconv"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

type EnterSceneByCentreLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewEnterSceneByCentreLogic(ctx context.Context, svcCtx *svc.ServiceContext) *EnterSceneByCentreLogic {
	return &EnterSceneByCentreLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Centre 请求某玩家进入场景，SceneManager 负责路由到具体 Scene 节点
func (l *EnterSceneByCentreLogic) EnterSceneByCentre(in *scene_manager.EnterSceneByCentreRequest) (*scene_manager.EnterSceneByCentreResponse, error) {
	// 1. Check if scene is on this node
	key := fmt.Sprintf("scene:%d:node", in.SceneId)
	nodeId, err := l.svcCtx.Redis.Get(key)
	if err != nil {
		l.Logger.Errorf("Scene lookup failed: %v", err)
		return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 1, ErrorMessage: "Scene lookup failed"}, nil
	}
	if nodeId != l.svcCtx.Config.NodeID {
		l.Logger.Errorf("Scene %d not on this node (expected %s, got %s)", in.SceneId, nodeId, l.svcCtx.Config.NodeID)
		return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 2, ErrorMessage: "Scene not on this node"}, nil
	}

	// 2. IDEMPOTENCY CHECK: Is player already here?
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err == nil && currentLoc != nil {
		if currentLoc.SceneId == in.SceneId && currentLoc.NodeId == l.svcCtx.Config.NodeID {
			// Already in the correct scene. Treat as success.
			l.Logger.Infof("Player %d already in scene %d, idempotent success", in.PlayerId, in.SceneId)
			return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 0}, nil
		}
	}

	// 3. Update Player Location (Source of Truth)
	err = UpdatePlayerLocation(l.ctx, l.svcCtx, in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)
	if err != nil {
		l.Logger.Errorf("Failed to update player location: %v", err)
		return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 1, ErrorMessage: "Failed to update location"}, nil
	}

	// 4. Send Route Command to Gate (via Kafka)
	if in.GateId != "" {
		targetNodeId, convErr := strconv.ParseUint(l.svcCtx.Config.NodeID, 10, 32)
		if convErr != nil {
			l.Logger.Errorf("Invalid SceneManager node id %q: %v", l.svcCtx.Config.NodeID, convErr)
			return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 1, ErrorMessage: "Invalid scene manager node id"}, nil
		}

		targetGateId, convErr := strconv.ParseUint(in.GateId, 10, 32)
		if convErr != nil {
			l.Logger.Errorf("Invalid GateID %q: %v", in.GateId, convErr)
			return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 1, ErrorMessage: "Invalid gate id"}, nil
		}

		eventId := game.ContractsKafkaRoutePlayerEventEventId
		event := &kafkacontracts.RoutePlayerEvent{
			SessionId:    in.SessionId,
			TargetNodeId: uint32(targetNodeId),
		}

		payload, err := proto.Marshal(event)
		if err != nil {
			l.Logger.Errorf("Failed to marshal RoutePlayerEvent: %v", err)
			return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 1, ErrorMessage: "Failed to encode route event"}, nil
		}

		cmd := &kafkacontracts.GateCommand{
			CommandType:      kafkacontracts.GateCommand_RoutePlayer,
			PlayerId:         in.PlayerId,
			TargetNodeId:     uint32(targetNodeId),
			SessionId:        in.SessionId,
			Payload:          payload,
			TargetGateId:     uint32(targetGateId),
			TargetInstanceId: in.GateInstanceId,
			EventId:          &eventId,
		}

		bytes, err := proto.Marshal(cmd)
		if err != nil {
			l.Logger.Errorf("Failed to marshal GateCommand: %v", err)
		} else {
			topic := fmt.Sprintf("gate-%s", in.GateId)
			err = l.svcCtx.Kafka.WriteMessages(l.ctx, kafkago.Message{
				Topic: topic,
				Key:   []byte(fmt.Sprintf("%d", in.PlayerId)),
				Value: bytes,
			})
			if err != nil {
				l.Logger.Errorf("Failed to push to Kafka topic %s: %v", topic, err)
			} else {
				l.Logger.Infof("Pushed RoutePlayer to Kafka topic %s for player %d -> node %s", topic, in.PlayerId, l.svcCtx.Config.NodeID)
			}
		}
	} else {
		l.Logger.Warnf("No GateID in EnterSceneByCentre request for player %d", in.PlayerId)
	}

	l.Logger.Infof("Player %d entered scene %d on node %s", in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)

	return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 0}, nil
}
