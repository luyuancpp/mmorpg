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
	mailpb "proto/mail"
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

// 规则 1:无会话 = 内部调用(运维 CLI / robot 直连 MailAdmin),放行,且不能被当成客户端。
func TestInternalCallWithoutSessionPassesThrough(t *testing.T) {
	for _, method := range []string{
		mailpb.MailAdmin_SendPersonalMail_FullMethodName,
		mailpb.ClientPlayerMail_ListMails_FullMethodName,
	} {
		result := call(nil, method)
		require.NoError(t, result.err, method)
		assert.True(t, result.handlerCalled, method)
		assert.False(t, result.fromClient, "%s:没有会话的调用是内部调用,不能被当成客户端", method)
		assert.Zero(t, result.playerID, method)
	}
}

func TestClientSessionCarriesAuthoritativePlayerID(t *testing.T) {
	result := call(clientSession(t), mailpb.ClientPlayerMail_ListMails_FullMethodName)

	require.NoError(t, result.err)
	assert.True(t, result.handlerCalled)
	assert.True(t, result.fromClient)
	assert.Equal(t, uint64(42), result.playerID)
}

// 规则 2:带了会话却解不开 → Unauthenticated,绝不退化成"当内部调用处理"。
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
			assertRejected(t, call(md, mailpb.ClientPlayerMail_ListMails_FullMethodName), codes.Unauthenticated)
		})
	}
}

// 规则 3a:S2C 推送方法不在白名单 → PermissionDenied(客户端伪造"新邮件"事件的入口)。
func TestClientCannotCallNotifyMailEvent(t *testing.T) {
	logtest.Discard(t)
	assertRejected(t, call(clientSession(t), mailpb.ClientPlayerMail_NotifyMailEvent_FullMethodName), codes.PermissionDenied)
}

// 规则 3b:客户端来源调 MailAdmin = 冒充运维发信 → PermissionDenied(设计 §2.2 的四重隔离之一)。
func TestClientCannotCallMailAdmin(t *testing.T) {
	logtest.Discard(t)
	for _, method := range []string{
		mailpb.MailAdmin_SendPersonalMail_FullMethodName,
		mailpb.MailAdmin_SendSystemMail_FullMethodName,
	} {
		assertRejected(t, call(clientSession(t), method), codes.PermissionDenied)
	}
}

// TestClientMethodsMatchServiceDescriptors 让新增 RPC 必须显式表态:
//   - ClientPlayerMail 上除 NotifyMailEvent 之外的方法必须全部在白名单;NotifyMailEvent 必须不在;
//   - MailAdmin 上的方法必须**全部不在**白名单;
//   - 白名单里不许有两个 ServiceDesc 之外的条目。
//
// 少了这条,以后加一个客户端 RPC 忘了登记 = 该功能对客户端静默 PermissionDenied;
// 反过来把 S2C 或运维方法抄进白名单 = 伪造入口。两种都只有遍历 ServiceDesc 才发现得了。
func TestClientMethodsMatchServiceDescriptors(t *testing.T) {
	desc := mailpb.ClientPlayerMail_ServiceDesc
	const s2cMethod = mailpb.ClientPlayerMail_NotifyMailEvent_FullMethodName

	s2cSeen := false
	for _, method := range desc.Methods {
		fullMethod := "/" + desc.ServiceName + "/" + method.MethodName
		_, allowed := ClientMethods[fullMethod]
		if fullMethod == s2cMethod {
			s2cSeen = true
			assert.False(t, allowed, "%s 是 S2C 推送方法,绝不能对客户端开放", fullMethod)
			continue
		}
		assert.True(t, allowed, "%s 是客户端协议方法,必须登记进 ClientMethods", fullMethod)
	}
	// 推送方法被改名 / 挪走时,上面的循环会静默全绿,所以单独钉一下"它确实还在这个服务上"。
	assert.True(t, s2cSeen, "ClientPlayerMail 上找不到 %s —— proto 改了就得同步改本测试的预期", s2cMethod)

	admin := mailpb.MailAdmin_ServiceDesc
	require.NotEmpty(t, admin.Methods)
	for _, method := range admin.Methods {
		fullMethod := "/" + admin.ServiceName + "/" + method.MethodName
		_, allowed := ClientMethods[fullMethod]
		assert.False(t, allowed, "%s 是运维发件方法,绝不能对客户端开放", fullMethod)
	}

	assert.Len(t, ClientMethods, len(desc.Methods)-1,
		"ClientMethods 只能恰好是 ClientPlayerMail 的方法减去 NotifyMailEvent")
}
