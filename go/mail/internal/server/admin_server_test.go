package server

import (
	"context"
	"strings"
	"testing"
	"time"

	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"mail/internal/config"
	"mail/internal/data"
	"mail/internal/logic"

	mailpb "proto/mail"
)

// recordingStore 只关心"有没有走到存储层":鉴权失败的调用一次都不能到这里。
type recordingStore struct {
	logic.MailStore // 未实现的方法被调用时 nil 接口 panic —— 测试里不会走到它们
	sends           int
}

func (r *recordingStore) SendPersonal(context.Context, data.PersonalSend) (data.SendResult, error) {
	r.sends++
	return data.SendResult{Outcome: data.SendCreated, MailID: 1}, nil
}

func (r *recordingStore) SendSystem(context.Context, data.SystemSend) (data.SendResult, error) {
	r.sends++
	return data.SendResult{Outcome: data.SendCreated, MailID: 2}, nil
}

const goodToken = "0123456789abcdef0123456789abcdef-test-token"

func adminDeps(store logic.MailStore, token string) *logic.Deps {
	c := config.Config{}
	c.Timeout = 4000
	c.Mode = "dev"
	c.Mail = config.MailConf{MaxPersonalMailsPerPlayer: 100, MaxActiveSystemMails: 100, MaxPersonalMailTtlDays: 30, MaxSystemMailTtlDays: 30, AllowedCurrencyTypes: []uint32{0, 1, 2}}
	return &logic.Deps{
		Config:     c,
		Repo:       store,
		NextID:     func(context.Context) (uint64, error) { return 1, nil },
		AdminToken: token,
		Now:        time.Now,
	}
}

func personalReq() *mailpb.SendPersonalMailRequest {
	return &mailpb.SendPersonalMailRequest{
		SenderKind: mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL, RequestId: 1, RecipientPlayerId: 42,
		SenderName: "运营", Title: "t", ExpireMs: uint64(time.Now().Add(time.Hour).UnixMilli()),
	}
}

func withToken(token string) context.Context {
	return metadata.NewIncomingContext(context.Background(), metadata.Pairs(config.AdminTokenMetadataKey, token))
}

// TestAdminTokenFourStates 钉住设计 §10 ⑥:没配 / 不足 32 字节 / 错值 → PermissionDenied 且不触库;对值 → 放行。
// 日志里**不得出现令牌值**(只记"带没带")。
func TestAdminTokenFourStates(t *testing.T) {
	short := strings.Repeat("x", config.MinAdminTokenBytes-1)
	for _, tc := range []struct {
		name       string
		serverEnv  string // 进程环境变量的值(经 ResolveAdminToken)
		callerSent string
		wantOK     bool
	}{
		{name: "没配", serverEnv: "", callerSent: goodToken},
		{name: "不足 32 字节视同没配", serverEnv: short, callerSent: short},
		{name: "调用方带错值", serverEnv: goodToken, callerSent: goodToken + "x"},
		{name: "调用方没带", serverEnv: goodToken, callerSent: ""},
		{name: "对值", serverEnv: goodToken, callerSent: goodToken, wantOK: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			logs := logtest.NewCollector(t)
			deniedLabels := map[string]string{"method": "send_personal", "sender_kind": "ops_tool", "outcome": "denied"}
			deniedBefore := counterValue(t, "mail_admin_send_total", deniedLabels)
			token := config.ResolveAdminToken(func(string) string { return tc.serverEnv })
			store := &recordingStore{}
			srv := NewAdminServer(adminDeps(store, token))
			ctx := context.Background()
			if tc.callerSent != "" {
				ctx = withToken(tc.callerSent)
			}

			_, perr := srv.SendPersonalMail(ctx, personalReq())
			_, serr := srv.SendSystemMail(ctx, &mailpb.SendSystemMailRequest{
				SenderKind: mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL, RequestId: 2, SenderName: "运营", Title: "t",
				ExpireMs: uint64(time.Now().Add(time.Hour).UnixMilli()),
			})
			// 不论放行与否,日志里都不得出现令牌值(放行分支的发件日志带调用方身份,同样不能带令牌)。
			for _, secret := range []string{goodToken, short} {
				if strings.Contains(logs.String(), secret) {
					t.Fatal("日志里出现了令牌值")
				}
			}
			if tc.wantOK {
				if perr != nil || serr != nil {
					t.Fatalf("对值应放行: %v / %v", perr, serr)
				}
				if store.sends != 2 {
					t.Fatalf("对值应走到存储 2 次,实际 %d", store.sends)
				}
				if got := counterValue(t, "mail_admin_send_total", deniedLabels); got != deniedBefore {
					t.Fatalf("放行的调用不该计 denied:%v -> %v", deniedBefore, got)
				}
				return
			}
			for _, err := range []error{perr, serr} {
				if status.Code(err) != codes.PermissionDenied {
					t.Fatalf("期望 PermissionDenied,得到 %v", err)
				}
				if tc.callerSent != "" && strings.Contains(err.Error(), tc.callerSent) {
					t.Fatal("错误文本泄露了令牌")
				}
			}
			if store.sends != 0 {
				t.Fatalf("鉴权失败不得触库,实际 %d 次", store.sends)
			}
			if got := counterValue(t, "mail_admin_send_total", deniedLabels); got != deniedBefore+1 {
				t.Fatalf("鉴权失败应计 mail_admin_send_total{outcome=\"denied\"} +1:%v -> %v", deniedBefore, got)
			}
		})
	}
}
