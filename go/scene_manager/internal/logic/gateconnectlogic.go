package logic

import (
	"context"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type GateConnectLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGateConnectLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GateConnectLogic {
	return &GateConnectLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Gate 连接（保持长连接，双向流）
func (l *GateConnectLogic) GateConnect(stream scene_manager.SceneManager_GateConnectServer) error {
	var gateID string

	// Receive the first message to identify the gate
	firstMsg, err := stream.Recv()
	if err != nil {
		l.Logger.Errorf("Failed to receive initial heartbeat: %v", err)
		return err
	}
	gateID = firstMsg.GateId
	if gateID == "" {
		l.Logger.Error("Gate ID is empty in initial heartbeat")
		return nil // Or return error
	}

	l.Logger.Infof("Gate connected: %s", gateID)

	// Store the stream
	l.svcCtx.GateStreams.Store(gateID, stream)
	defer func() {
		l.svcCtx.GateStreams.Delete(gateID)
		l.Logger.Infof("Gate disconnected: %s", gateID)
	}()

	// Loop to receive heartbeats
	for {
		msg, err := stream.Recv()
		if err != nil {
			return err
		}
		// Update load or handle other logic here
		l.Logger.Debugf("Received heartbeat from gate %s: load=%d", msg.GateId, msg.ConnectionCount)
	}
}
