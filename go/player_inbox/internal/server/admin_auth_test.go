package server

import (
	"context"
	"testing"

	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

const goodToken = "0123456789abcdef-test-token"

func runInterceptor(t *testing.T, token, method string, md metadata.MD) (called bool, err error) {
	t.Helper()
	ctx := context.Background()
	if md != nil {
		ctx = metadata.NewIncomingContext(ctx, md)
	}
	_, err = AdminAuthInterceptor(token)(ctx, nil, &grpc.UnaryServerInfo{FullMethod: method},
		func(context.Context, any) (any, error) {
			called = true
			return nil, nil
		})
	return called, err
}

// TestAdminAuth 钉住运维面鉴权(设计 §1.6、§11.4 验收「口令为空时停用;口令错误返回 PermissionDenied」)。
func TestAdminAuth(t *testing.T) {
	logtest.Discard(t)
	admin := inboxpb.PlayerInboxAdmin_GmSanctionPlayer_FullMethodName
	cases := []struct {
		name   string
		token  string
		method string
		md     metadata.MD
		called bool
		code   codes.Code
	}{
		{"口令为空:运维面停用,带什么都拒", "", admin, metadata.Pairs(AdminTokenMetadataKey, goodToken), false, codes.PermissionDenied},
		{"没带口令", goodToken, admin, nil, false, codes.PermissionDenied},
		{"口令错误", goodToken, admin, metadata.Pairs(AdminTokenMetadataKey, goodToken+"x"), false, codes.PermissionDenied},
		{"口令正确", goodToken, admin, metadata.Pairs(AdminTokenMetadataKey, goodToken), true, codes.OK},
		{"其它运维方法同样受保护", goodToken, inboxpb.PlayerInboxAdmin_VoidItem_FullMethodName, nil, false, codes.PermissionDenied},
		{"scene 的 Fetch 不经口令", "", inboxpb.PlayerInbox_FetchItems_FullMethodName, nil, true, codes.OK},
		{"scene 的 Ack 不经口令", goodToken, inboxpb.PlayerInbox_AckItems_FullMethodName, nil, true, codes.OK},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			called, err := runInterceptor(t, tc.token, tc.method, tc.md)
			if called != tc.called || status.Code(err) != tc.code {
				t.Fatalf("called=%v code=%v, want called=%v code=%v (err=%v)", called, status.Code(err), tc.called, tc.code, err)
			}
		})
	}
}

// TestAdminAuthCoversEveryAdminMethod:服务描述里的每个运维方法都落在拦截前缀内(新增方法不可能漏挂)。
func TestAdminAuthCoversEveryAdminMethod(t *testing.T) {
	logtest.Discard(t)
	for _, m := range inboxpb.PlayerInboxAdmin_ServiceDesc.Methods {
		method := "/" + inboxpb.PlayerInboxAdmin_ServiceDesc.ServiceName + "/" + m.MethodName
		if called, err := runInterceptor(t, goodToken, method, nil); called || status.Code(err) != codes.PermissionDenied {
			t.Errorf("%s 没带口令却放行了", method)
		}
	}
}

func TestPeerAddrWithoutPeer(t *testing.T) {
	if got := PeerAddr(context.Background()); got != "unknown" {
		t.Fatalf("PeerAddr = %q, want unknown", got)
	}
}
