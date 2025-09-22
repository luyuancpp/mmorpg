package clientplayerloginlogic

import (
	"context"
	"login/generated/pb/game"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	login_proto_common "login/proto/common"
	login_proto "login/proto/service/go/grpc/login"

	"github.com/zeromicro/go-zero/core/logx"
)

type DisconnectLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewDisconnectLogic(ctx context.Context, svcCtx *svc.ServiceContext) *DisconnectLogic {
	return &DisconnectLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *DisconnectLogic) Disconnect(in *login_proto.LoginNodeDisconnectRequest) (*login_proto.LoginEmptyResponse, error) {
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during leave game")
		return &login_proto.LoginEmptyResponse{}, nil
	}

	_ = sessioncleaner.CleanupSession(
		l.ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"disconnect",
	)

	centreRequest := &login_proto_common.GateSessionDisconnectRequest{
		SessionInfo: &login_proto_common.SessionDetails{SessionId: in.SessionId},
	}
	node := l.svcCtx.GetCentreClient()
	if nil == node {
		return &login_proto.LoginEmptyResponse{}, nil
	}
	node.Send(centreRequest, game.CentreLoginNodeSessionDisconnectMessageId)
	return &login_proto.LoginEmptyResponse{}, nil
}
