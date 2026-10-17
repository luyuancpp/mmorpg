package session

import (
	"context"
	"encoding/base64"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	base "proto/common/base"
	rankpb "proto/rank"
)

type callResult struct {
	handlerCalled bool
	playerID      uint64
	fromClient    bool
	err           error
}

func encodeSession(t *testing.T, detail *base.SessionDetails) string {
	t.Helper()
	raw, err := proto.Marshal(detail)
	require.NoError(t, err)
	return base64.StdEncoding.EncodeToString(raw)
}

// call 走真拦截器,handler 里把"逻辑层看到的身份"抄下来 —— 断言的是业务能拿到什么,不是拦截器内部实现。
func call(md metadata.MD, method string) callResult {
	ctx := context.Background()
	if md != nil {
		ctx = metadata.NewIncomingContext(ctx, md)
	}
	var result callResult
	_, result.err = UnaryServerInterceptor(ClientMethods)(ctx, nil,
		&grpc.UnaryServerInfo{FullMethod: method},
		func(ctx context.Context, _ any) (any, error) {
			result.handlerCalled = true
			result.playerID, result.fromClient = ClientPlayerID(ctx)
			return "ok", nil
		})
	return result
}

func clientSession(t *testing.T) metadata.MD {
	return metadata.Pairs(MetadataKey, encodeSession(t, &base.SessionDetails{SessionId: 7, PlayerId: 42}))
}

func assertRejected(t *testing.T, result callResult, want codes.Code) {
	t.Helper()
	assert.False(t, result.handlerCalled, "被拒绝的调用不能进入业务 handler")
	st, ok := status.FromError(result.err)
	require.True(t, ok, "应返回 gRPC status,得到 %v", result.err)
	assert.Equal(t, want, st.Code())
}

// 规则 1:无会话 = 内部调用,放行,且不能被当成客户端(逻辑层据此回 kInvalidParameter、不查 Redis,T7)。
func TestInternalCallWithoutSessionPassesThrough(t *testing.T) {
	result := call(nil, rankpb.ClientPlayerRank_GetRankPage_FullMethodName)
	require.NoError(t, result.err)
	assert.True(t, result.handlerCalled)
	assert.False(t, result.fromClient, "没有会话的调用不能被当成客户端")
	assert.Zero(t, result.playerID)
}

func TestClientSessionCarriesAuthoritativePlayerID(t *testing.T) {
	result := call(clientSession(t), rankpb.ClientPlayerRank_GetRankPage_FullMethodName)
	require.NoError(t, result.err)
	assert.True(t, result.handlerCalled)
	assert.True(t, result.fromClient)
	assert.Equal(t, uint64(42), result.playerID)
}

// 规则 2:带了会话却解不开 → Unauthenticated,handler 不被调用(T7b)。
func TestBrokenSessionMetadataFailsClosed(t *testing.T) {
	logtest.Discard(t)
	cases := map[string]string{
		"not base64":         "%%%not-base64%%%",
		"not SessionDetails": base64.StdEncoding.EncodeToString([]byte{0xff, 0xff, 0xff}),
		"missing player_id":  encodeSession(t, &base.SessionDetails{SessionId: 7}),
	}
	for name, value := range cases {
		t.Run(name, func(t *testing.T) {
			md := metadata.Pairs(MetadataKey, value)
			assertRejected(t, call(md, rankpb.ClientPlayerRank_GetRankPage_FullMethodName), codes.Unauthenticated)
		})
	}
}

// 规则 3:白名单外的方法 → PermissionDenied(纵深防御:将来往 ClientPlayerRank 上加的内部方法)。
func TestClientCannotCallMethodsOutsideWhitelist(t *testing.T) {
	logtest.Discard(t)
	assertRejected(t, call(clientSession(t), "/rankpb.ClientPlayerRank/SomeFutureInternalWrite"), codes.PermissionDenied)
}

// TestClientMethodsMatchServiceDescriptor 让新增 RPC 必须显式表态:ClientPlayerRank 上的方法全部在白名单,
// 白名单里也不许有 ServiceDesc 之外的条目。少了这条,加一个客户端 RPC 忘了登记 = 该功能对客户端静默 PermissionDenied。
func TestClientMethodsMatchServiceDescriptor(t *testing.T) {
	desc := rankpb.ClientPlayerRank_ServiceDesc
	require.NotEmpty(t, desc.Methods)
	for _, method := range desc.Methods {
		fullMethod := "/" + desc.ServiceName + "/" + method.MethodName
		_, allowed := ClientMethods[fullMethod]
		assert.True(t, allowed, "%s 是客户端协议方法,必须登记进 ClientMethods", fullMethod)
	}
	assert.Len(t, ClientMethods, len(desc.Methods), "ClientMethods 只能恰好是 ClientPlayerRank 的方法")
}
