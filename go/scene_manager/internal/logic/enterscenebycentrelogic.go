package logic

import (
	"context"
	"fmt"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
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

	// 4. Send Route Command to Gate (if GateID is provided)
	if in.GateId != "" {
		streamVal, ok := l.svcCtx.GateStreams.Load(in.GateId)
		if ok {
			stream := streamVal.(scene_manager.SceneManager_GateConnectServer)
			cmd := &scene_manager.GateCommand{
				CommandType:  scene_manager.GateCommand_RoutePlayer,
				PlayerId:     in.PlayerId,
				TargetNodeId: l.svcCtx.Config.NodeID, // Route to THIS Scene Manager node (or specific Scene Node logic?)
				SessionId:    in.SessionId,
			}
			if err := stream.Send(cmd); err != nil {
				l.Logger.Errorf("Failed to send RoutePlayer command to Gate %s: %v", in.GateId, err)
				// Don't fail the request, but log it. The client might retry or rely on heartbeat sync.
			} else {
				l.Logger.Infof("Sent RoutePlayer to Gate %s for player %d -> node %s", in.GateId, in.PlayerId, l.svcCtx.Config.NodeID)
			}
		} else {
			l.Logger.Warnf("Gate %s not connected, cannot route player %d", in.GateId, in.PlayerId)
		}
	} else {
		l.Logger.Warnf("No GateID in EnterSceneByCentre request for player %d", in.PlayerId)
	}

	l.Logger.Infof("Player %d entered scene %d on node %s", in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)

	return &scene_manager.EnterSceneByCentreResponse{ErrorCode: 0}, nil
}
