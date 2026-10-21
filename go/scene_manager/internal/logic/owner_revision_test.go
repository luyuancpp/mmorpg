package logic

import (
	"context"
	"testing"

	"proto/scene_manager"
	"scene_manager/internal/constants"
	"scene_manager/internal/svc"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

func TestEnterSceneMissingOwnerRevisionRejectsBeforeDependencies(t *testing.T) {
	for _, in := range []*scene_manager.EnterSceneRequest{nil, {PlayerId: 11, SessionId: 7, GateId: "1", GateInstanceId: "original-gate"}} {
		out, err := NewEnterSceneLogic(context.Background(), &svc.ServiceContext{}).EnterScene(in)
		require.Equal(t, codes.InvalidArgument, status.Code(err))
		require.Nil(t, out, "缺归属修订不能从已存在的去重或路由补权")
	}
}

func TestEnterSceneOwnerRevisionIsAuthorizationFingerprintAndOriginalRoute(t *testing.T) {
	sc, mr := newTestSvcCtxWithWorldScenes(t)
	captured := capturingKafkaWriter(sc)
	const sceneID = uint64(77401)
	seedSceneOnNode(mr, testZoneId, sceneID, "10", "0")
	in := &scene_manager.EnterSceneRequest{PlayerId: 66401, SceneId: sceneID, ZoneId: testZoneId, SessionId: 11, RequestId: "owner-revision-once", GateZoneId: testZoneId, GateId: "1", GateInstanceId: "original-gate", LoginGeneration: 999, OwnerRevision: 17}
	first, err := NewEnterSceneLogic(context.Background(), sc).EnterScene(in)
	require.NoError(t, err)
	require.Zero(t, first.ErrorCode)
	require.Len(t, *captured, 1)
	event := decodeRoutePlayerEvent(t, (*captured)[0])
	require.Equal(t, uint64(17), event.OwnerRevision)
	require.Equal(t, uint64(999), event.LoginGeneration)
	retry := proto.Clone(in).(*scene_manager.EnterSceneRequest)
	retry.LoginGeneration++
	retry.CorrelationId++
	replayed, err := NewEnterSceneLogic(context.Background(), sc).EnterScene(retry)
	require.NoError(t, err)
	require.Zero(t, replayed.ErrorCode)
	require.Len(t, *captured, 1)
	retry.OwnerRevision++
	denied, err := NewEnterSceneLogic(context.Background(), sc).EnterScene(retry)
	require.NoError(t, err)
	require.Equal(t, constants.ErrEnterSceneIdempotencyConflict, denied.ErrorCode)
	require.Len(t, *captured, 1, "新归属身份不能复用旧request_id重放卖家路由")
	require.Equal(t, uint64(17), in.OwnerRevision, "指纹计算不得修改原请求")
}
