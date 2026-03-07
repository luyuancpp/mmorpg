package logic

import (
	"context"
	"fmt"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type CreateSceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreateSceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreateSceneLogic {
	return &CreateSceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// 在指定节点创建一个场景（由 Scene 节点实现具体创建）
func (l *CreateSceneLogic) CreateScene(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	// 1. Determine Target Node
	targetNode := in.TargetNodeId
	if targetNode == "" {
		// Load Balancing: Select best node
		bestNode, err := GetBestNode(l.ctx, l.svcCtx)
		if err != nil {
			l.Logger.Errorf("Failed to select best node: %v", err)
			// Fallback to local
			targetNode = l.svcCtx.Config.NodeID
		} else {
			targetNode = bestNode
		}
	}

	// 2. Generate new Scene ID
	id, err := l.svcCtx.Redis.Incr("scene:id_counter")
	if err != nil {
		l.Logger.Errorf("Failed to generate scene id: %v", err)
		return &scene_manager.CreateSceneResponse{ErrorCode: 1, ErrorMessage: "Internal error"}, nil
	}
	sceneId := uint64(id)

	// 3. Store mapping scene->node
	err = l.svcCtx.Redis.Set(fmt.Sprintf("scene:%d:node", sceneId), targetNode)
	if err != nil {
		l.Logger.Errorf("Failed to register scene: %v", err)
		return &scene_manager.CreateSceneResponse{ErrorCode: 1, ErrorMessage: "Redis error"}, nil
	}
	
	// 4. Create Scene Info in Redis (optional but good for metadata)
	// We should probably store more info like SceneConfId
	// ... (Skipping full metadata for brevity, but storing node mapping is key)

	l.Logger.Infof("Created scene %d on node %s (target was %s)", sceneId, targetNode, in.TargetNodeId)
	return &scene_manager.CreateSceneResponse{SceneId: sceneId, NodeId: targetNode, ErrorCode: 0}, nil
}
