package main

import (
	"bytes"
	"context"
	"database/sql"
	"database/sql/driver"
	"encoding/base64"
	"encoding/json"
	"errors"
	"strings"
	"testing"

	"mail/internal/config"
	"mail/internal/constants"
	"mail/internal/data"
	"mail/internal/session"

	base "proto/common/base"
	mailpb "proto/mail"

	"schemamigrate"

	"shared/killswitch"
	"shared/serverbase"

	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// chainUnary 按 gRPC 的语义把一组一元拦截器串成一个 handler:切片第一个是最外层(与 grpc.ChainUnaryInterceptor 一致)。
// grpc-go 的 chainUnaryInterceptors 是包内私有函数,这里复刻一份(与 friend / chat / guild / trade 的同名测试逐字相同)。
func chainUnary(
	interceptors []grpc.UnaryServerInterceptor,
	info *grpc.UnaryServerInfo,
	handler grpc.UnaryHandler,
) grpc.UnaryHandler {
	next := handler
	for i := len(interceptors) - 1; i >= 0; i-- {
		cur, downstream := interceptors[i], next
		next = func(ctx context.Context, req any) (any, error) {
			return cur(ctx, req, info, downstream)
		}
	}
	return next
}

// incomingSessionContext 模拟 gate 注入、路由服原样透传的 x-session-detail-bin(值 = base64(proto SessionDetails))。
func incomingSessionContext(t *testing.T, playerId uint64) context.Context {
	t.Helper()
	bin, err := proto.Marshal(&base.SessionDetails{SessionId: 1, PlayerId: playerId})
	if err != nil {
		t.Fatalf("序列化 SessionDetails 失败: %v", err)
	}
	md := metadata.Pairs(session.MetadataKey, base64.StdEncoding.EncodeToString(bin))
	return metadata.NewIncomingContext(context.Background(), md)
}

// TestBuildUnaryInterceptorsOrder 钉住链长与顺序:grpcstats → killswitch → session → serverbase(设计 §5.4)。
// 按可观测行为逐个探测(函数值不能比较):killswitch 是唯一不调 handler、回 Unavailable 的;session 是唯一让 handler
// 取到会话身份的;serverbase 是唯一把 fault 码打成 rpc_inband_fault 日志的;剩下的位置是 grpcstats。
func TestBuildUnaryInterceptorsOrder(t *testing.T) {
	ks := killswitch.New(killswitch.Config{})
	ks.SetRules(map[string]killswitch.Rule{
		strings.TrimPrefix(mailpb.ClientPlayerMail_ListMails_FullMethodName, "/"): {Deny: true, Reason: "单测:顺序探针"},
	})
	chain := buildUnaryInterceptors(ks)
	if len(chain) != 4 {
		t.Fatalf("拦截器链长度应为 4(grpcstats/killswitch/session/serverbase),实际 %d", len(chain))
	}

	type observed struct {
		handlerCalled bool
		blocked       bool
		sawSession    bool
		loggedFault   bool
	}
	want := []struct {
		name string
		obs  observed
	}{
		{"grpcstats", observed{handlerCalled: true}},
		{"killswitch", observed{blocked: true}},
		{"session", observed{handlerCalled: true, sawSession: true}},
		{"serverbase", observed{handlerCalled: true, loggedFault: true}},
	}

	info := &grpc.UnaryServerInfo{FullMethod: mailpb.ClientPlayerMail_ListMails_FullMethodName}
	// ErrStorage = kServiceUnavailable 是 mail 唯一的 fault 码:用它当探针才看得出 serverbase 那一层在不在。
	faultResp := &mailpb.ListMailsResponse{ErrorMessage: &base.TipInfoMessage{Id: constants.ErrStorage}}

	for i, w := range want {
		t.Run(w.name, func(t *testing.T) {
			logs := logtest.NewCollector(t)
			var got observed
			_, err := chain[i](incomingSessionContext(t, 42), &mailpb.ListMailsRequest{}, info,
				func(ctx context.Context, req any) (any, error) {
					got.handlerCalled = true
					_, got.sawSession = session.ClientPlayerID(ctx)
					return faultResp, nil
				})
			if st, ok := status.FromError(err); err != nil && ok && st.Code() == codes.Unavailable {
				got.blocked = true
			}
			got.loggedFault = strings.Contains(logs.String(), serverbase.EventInbandFault)
			if got != w.obs {
				t.Fatalf("位置 %d 应是 %s,行为不符: 期望 %+v,实际 %+v", i, w.name, w.obs, got)
			}
		})
	}
}

// TestKillSwitchWiredIntoUnaryChain:热关停确实挂在链上。killswitch 没写规则时与不存在一模一样,
// 删掉那一行所有联调都照样全绿 —— 这条测试是那一行的唯一守卫。
func TestKillSwitchWiredIntoUnaryChain(t *testing.T) {
	ks := killswitch.New(killswitch.Config{})
	// 模式从生成常量取:运维的 etcdctl 示例(设计 §9.5)写的就是 mailpb.ClientPlayerMail/...。
	blocked := strings.TrimPrefix(mailpb.ClientPlayerMail_ListMails_FullMethodName, "/")
	ks.SetRules(map[string]killswitch.Rule{
		blocked: {Deny: true, Reason: "单测:邮件库过载"},
	})
	chain := buildUnaryInterceptors(ks)

	t.Run("命中规则的方法被短路", func(t *testing.T) {
		handlerCalled := false
		info := &grpc.UnaryServerInfo{FullMethod: mailpb.ClientPlayerMail_ListMails_FullMethodName}
		h := chainUnary(chain, info, func(ctx context.Context, req any) (any, error) {
			handlerCalled = true
			return &mailpb.ListMailsResponse{}, nil
		})
		resp, err := h(incomingSessionContext(t, 42), &mailpb.ListMailsRequest{})
		if handlerCalled {
			t.Fatal("handler 被调用了:killswitch 没有挂上拦截器链(或不在 handler 上游)")
		}
		if resp != nil {
			t.Fatalf("被关停的调用不该有响应体,得到 %#v", resp)
		}
		st, ok := status.FromError(err)
		if !ok || st.Code() != codes.Unavailable {
			t.Fatalf("期望 gRPC Unavailable,得到 %v", err)
		}
		if !strings.Contains(st.Message(), "ListMails") || !strings.Contains(st.Message(), "邮件库过载") {
			t.Fatalf("错误文本缺少方法名/原因: %q", st.Message())
		}
	})

	t.Run("未命中的方法照常放行", func(t *testing.T) {
		handlerCalled := false
		info := &grpc.UnaryServerInfo{FullMethod: mailpb.ClientPlayerMail_ReadMail_FullMethodName}
		h := chainUnary(chain, info, func(ctx context.Context, req any) (any, error) {
			handlerCalled = true
			if _, ok := session.ClientPlayerID(ctx); !ok {
				t.Error("整条链走完后 handler 应能取到会话身份(D-9)")
			}
			return &mailpb.ReadMailResponse{}, nil
		})
		if _, err := h(incomingSessionContext(t, 42), &mailpb.ReadMailRequest{MailId: 1}); err != nil {
			t.Fatalf("未命中规则的方法不该报错: %v", err)
		}
		if !handlerCalled {
			t.Fatal("未命中规则的方法必须走到 handler")
		}
	})
}

// TestKillSwitchFailOpenWithoutRules:没有任何规则(etcd 没配 / 连不上)时链完全透明。
func TestKillSwitchFailOpenWithoutRules(t *testing.T) {
	ks := killswitch.New(killswitch.Config{})
	ks.Start(context.Background(), nil)

	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: mailpb.ClientPlayerMail_ListMails_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(ks), info, func(ctx context.Context, req any) (any, error) {
		handlerCalled = true
		return &mailpb.ListMailsResponse{}, nil
	})
	if _, err := h(incomingSessionContext(t, 42), &mailpb.ListMailsRequest{}); err != nil {
		t.Fatalf("无规则时必须放行: %v", err)
	}
	if !handlerCalled {
		t.Fatal("无规则时 handler 必须被调用(fail-open)")
	}
}

// TestSessionRejectsForbiddenMethodsFromClient 经**真链**验证三类方法挡得住客户端来源(设计 §2.2 / §5.4):
//   - NotifyMailEvent:S2C 方向,客户端能调它就能给自己伪造"新邮件"提示;
//   - MailAdmin 的两个发件方法:客户端来源调用 = 冒充运维发信。令牌之外的第一道隔离就在这里。
//
// 删掉 buildUnaryInterceptors 里 session 那一行,其余测试全绿,这条立刻 FAIL。
func TestSessionRejectsForbiddenMethodsFromClient(t *testing.T) {
	logtest.Discard(t)
	for _, tc := range []struct {
		fullMethod string
		req        any
	}{
		{mailpb.ClientPlayerMail_NotifyMailEvent_FullMethodName, &mailpb.MailEventS2C{}},
		{mailpb.MailAdmin_SendPersonalMail_FullMethodName, &mailpb.SendPersonalMailRequest{}},
		{mailpb.MailAdmin_SendSystemMail_FullMethodName, &mailpb.SendSystemMailRequest{}},
	} {
		t.Run(tc.fullMethod, func(t *testing.T) {
			handlerCalled := false
			info := &grpc.UnaryServerInfo{FullMethod: tc.fullMethod}
			h := chainUnary(buildUnaryInterceptors(killswitch.New(killswitch.Config{})), info,
				func(ctx context.Context, req any) (any, error) {
					handlerCalled = true
					return &base.Empty{}, nil
				})
			_, err := h(incomingSessionContext(t, 42), tc.req)
			if handlerCalled {
				t.Fatal("客户端会话调用走到了 handler:没有被会话白名单挡住")
			}
			if status.Code(err) != codes.PermissionDenied {
				t.Fatalf("期望 PermissionDenied,得到 %v", err)
			}
		})
	}
}

// TestInternalCallReachesMailAdmin:不带会话的内部调用(CLI / robot 直连)必须能到达 MailAdmin 的 handler ——
// 是否放行由 admin server 的令牌决定,会话层不能把它也挡掉(否则 MailAdmin 永远不可用)。
func TestInternalCallReachesMailAdmin(t *testing.T) {
	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: mailpb.MailAdmin_SendSystemMail_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(killswitch.New(killswitch.Config{})), info,
		func(ctx context.Context, req any) (any, error) {
			handlerCalled = true
			if _, ok := session.ClientPlayerID(ctx); ok {
				t.Error("内部调用不应带客户端身份")
			}
			return &mailpb.SendSystemMailResponse{}, nil
		})
	if _, err := h(context.Background(), &mailpb.SendSystemMailRequest{}); err != nil {
		t.Fatalf("内部调用不该被会话层拒绝: %v", err)
	}
	if !handlerCalled {
		t.Fatal("内部调用必须走到 MailAdmin 的 handler(由令牌鉴权)")
	}
}

// TestBrokenSessionRejectedInChain:坏会话头 fail-closed(Unauthenticated),不能退化成"当内部调用"——
// 否则伪造一个解不开的头就能当作内部调用拨 MailAdmin(只剩令牌一道闸)。
func TestBrokenSessionRejectedInChain(t *testing.T) {
	logtest.Discard(t)
	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: mailpb.MailAdmin_SendPersonalMail_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(killswitch.New(killswitch.Config{})), info,
		func(ctx context.Context, req any) (any, error) {
			handlerCalled = true
			return &mailpb.SendPersonalMailResponse{}, nil
		})
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs(session.MetadataKey, "%%%不是base64"))
	_, err := h(ctx, &mailpb.SendPersonalMailRequest{})
	if handlerCalled {
		t.Fatal("坏会话头走到了 handler")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("期望 Unauthenticated,得到 %v", err)
	}
}

// TestNodeInfoValueMatchesRegistryContract:BuildValue 产物通过 noderegistry 的注册前校验,nodeType 是 MailNodeService(8),
// Endpoint 与 GrpcEndpoint **都填**(只填前者时路由服拨不通,guild 踩过)。
func TestNodeInfoValueMatchesRegistryContract(t *testing.T) {
	build := nodeInfoValueBuilder(2, "10.0.0.9", 50900, 1_700_000_000)
	raw, err := build(7, "uuid-mail-test")
	if err != nil {
		t.Fatalf("BuildValue 失败: %v", err)
	}
	var mirror struct {
		NodeId       uint32 `json:"nodeId"`
		NodeType     uint32 `json:"nodeType"`
		ZoneId       uint32 `json:"zoneId"`
		NodeUuid     string `json:"nodeUuid"`
		ProtocolType uint32 `json:"protocolType"`
		Endpoint     struct {
			Ip   string `json:"ip"`
			Port uint32 `json:"port"`
		} `json:"endpoint"`
		GrpcEndpoint struct {
			Ip   string `json:"ip"`
			Port uint32 `json:"port"`
		} `json:"grpcEndpoint"`
	}
	if err := json.Unmarshal(raw, &mirror); err != nil {
		t.Fatalf("NodeInfo protojson 无法按镜像 struct 解析: %v\n%s", err, raw)
	}
	if mirror.NodeId != 7 || mirror.ZoneId != 2 || mirror.NodeUuid != "uuid-mail-test" {
		t.Fatalf("身份字段不符: %+v", mirror)
	}
	if uint32(base.ENodeType_MailNodeService) != 8 {
		t.Fatalf("MailNodeService 枚举值应为 8,实际 %d", base.ENodeType_MailNodeService)
	}
	if mirror.NodeType != uint32(base.ENodeType_MailNodeService) {
		t.Fatalf("nodeType 应为 MailNodeService(8),实际 %d", mirror.NodeType)
	}
	if mirror.ProtocolType != uint32(base.ENodeProtocolType_PROTOCOL_GRPC) {
		t.Fatalf("protocolType 应为 PROTOCOL_GRPC,实际 %d", mirror.ProtocolType)
	}
	if mirror.Endpoint.Port != 50900 || mirror.GrpcEndpoint.Port != 50900 ||
		mirror.Endpoint.Ip != "10.0.0.9" || mirror.GrpcEndpoint.Ip != "10.0.0.9" {
		t.Fatalf("端点字段不符(Endpoint 与 GrpcEndpoint 必须同时填): %+v", mirror)
	}
	if got := base.ENodeType_name[int32(nodeType)] + ".rpc"; got != "MailNodeService.rpc" {
		t.Fatalf("注册前缀应为 MailNodeService.rpc,实际 %q", got)
	}
}

// TestBannerContract 钉住横幅的三条外部契约(设计 §10 ⑤):
//   - 就绪行逐字是 go_services.ps1 的判据;
//   - mail_admin=enabled / disabled 与令牌状态一致;
//   - 横幅里**没有**密码与令牌值(只给状态)。
func TestBannerContract(t *testing.T) {
	const secretPassword = "pw-should-never-print"
	c := config.Config{}
	c.ListenOn = "127.0.0.1:50900"
	c.Mode = "pro"
	c.MySQL = config.MySQLConf{Host: "127.0.0.1:3306", User: "appuser", Password: secretPassword, DBName: data.DatabaseName}
	c.Sweep = config.SweepConf{Mode: config.SweepModeReportOnly}

	for _, active := range []bool{true, false} {
		var buf bytes.Buffer
		printBanner(&buf, c, bannerInfo{advertise: "127.0.0.1:50900", adminActive: active})
		out := buf.String()
		if !strings.Contains(out, "\n"+readyMarker+"\n") || !strings.Contains(readyMarker, "STARTED SUCCESSFULLY") {
			t.Fatalf("横幅缺少就绪行 %q:\n%s", readyMarker, out)
		}
		want := "\n  mail_admin=disabled\n"
		if active {
			want = "\n  mail_admin=enabled\n"
		}
		if !strings.Contains(out, want) {
			t.Fatalf("令牌状态 %v 时横幅应有恰为 %q 的一行:\n%s", active, strings.TrimSpace(want), out)
		}
		if strings.Contains(out, secretPassword) {
			t.Fatal("横幅打印了 MySQL 密码")
		}
		if !strings.Contains(out, "sweep:         mode=report_only") {
			t.Fatalf("横幅应带 sweep 模式行:\n%s", out)
		}
	}
}

// fakeSchemaRunner 记录调用并返回预设结果,替代 schemamigrate.Up / Plan。
type fakeSchemaRunner struct {
	report schemamigrate.Report
	err    error
	calls  int
	opts   schemamigrate.Options
}

func (f *fakeSchemaRunner) run(_ context.Context, _ *sql.DB, opts schemamigrate.Options) (schemamigrate.Report, error) {
	f.calls++
	f.opts = opts
	return f.report, f.err
}

// missingIndexWarningSample 是 schemamigrate"缺索引"告警的逐字样本(go/schemamigrate/plan.go 的 fmt.Sprintf)。
// 与生产代码的联系只有前缀常量 missingIndexWarningPrefix;schemamigrate 改文案时本测试不会自动变红,
// 机械保障是设计 §10 ⑤ 的真库核对(删一条普通索引后启动必须被拒)。
const missingIndexWarningSample = "缺索引(不会自动建):mail_personal 上没有 proto 声明的索引 idx_mail_personal_0"

// TestMissingIndexWarningIsBlocking:缺索引不是告警,是阻断(M-16)。
func TestMissingIndexWarningIsBlocking(t *testing.T) {
	if !strings.HasPrefix(missingIndexWarningSample, missingIndexWarningPrefix) {
		t.Fatalf("样本告警 %q 不以生产代码的前缀 %q 开头", missingIndexWarningSample, missingIndexWarningPrefix)
	}
	report := schemamigrate.Report{Warnings: []string{"extra column: mail_personal.legacy_col 不在 proto 里", missingIndexWarningSample}}
	got := missingIndexWarnings(report)
	if len(got) != 1 || got[0] != missingIndexWarningSample {
		t.Fatalf("应只挑出那一条缺索引告警,实际 %v", got)
	}
	err := missingIndexError(report)
	if err == nil {
		t.Fatal("缺索引必须返回错误(拒绝启动 / -migrate 返回非 0)")
	}
	if !strings.Contains(err.Error(), missingIndexWarningSample) || !strings.Contains(err.Error(), data.DatabaseName) {
		t.Errorf("错误应带上原始告警与库名,实际: %v", err)
	}
	if missingIndexError(schemamigrate.Report{Warnings: []string{"extra column"}}) != nil {
		t.Error("只有多余列告警时不得拒绝启动")
	}
	if missingIndexError(schemamigrate.Report{}) != nil {
		t.Error("没有任何告警时不得拒绝启动")
	}
}

// TestEnsureSchemaRejectsStartupWhenPlanNotClean 钉住启动期建表策略(D-14 第 4 条)。
func TestEnsureSchemaRejectsStartupWhenPlanNotClean(t *testing.T) {
	logtest.Discard(t)
	yes, no := true, false
	cases := []struct {
		name        string
		autoMigrate *bool
		report      schemamigrate.Report
		err         error
		wantUp      bool
		wantErr     bool
		wantRemedy  bool
	}{
		{name: "没写 Schema 段 = Up,建表成功", report: schemamigrate.Report{Statements: []string{"CREATE TABLE mail_personal ..."}}, wantUp: true},
		{name: "显式 true,库已是最新", autoMigrate: &yes, wantUp: true},
		{name: "Up 只有告警照常启动", autoMigrate: &yes, report: schemamigrate.Report{Warnings: []string{"extra column"}}, wantUp: true},
		{name: "Up 出错拒绝启动", autoMigrate: &yes, err: schemamigrate.ErrLockBusy, wantUp: true, wantErr: true},
		{name: "Up 有需人工项拒绝启动", autoMigrate: &yes, report: schemamigrate.Report{Manual: []string{"type drift"}}, wantUp: true, wantErr: true},
		{name: "Up 缺索引拒绝启动", autoMigrate: &yes, report: schemamigrate.Report{Warnings: []string{missingIndexWarningSample}},
			wantUp: true, wantErr: true},
		{name: "false + 库干净", autoMigrate: &no},
		{name: "false + 只有告警", autoMigrate: &no, report: schemamigrate.Report{Warnings: []string{"extra column"}}},
		{name: "false + 有待执行语句", autoMigrate: &no, report: schemamigrate.Report{Statements: []string{"CREATE TABLE mail_box ..."}},
			wantErr: true, wantRemedy: true},
		{name: "false + 需人工", autoMigrate: &no, report: schemamigrate.Report{Manual: []string{"type drift"}},
			wantErr: true, wantRemedy: true},
		{name: "false + Plan 出错", autoMigrate: &no, err: errors.New("dial tcp: connection refused"),
			wantErr: true, wantRemedy: true},
		// 缺索引落在 Warnings 里,Report.Clean() 不看它;不带补救命令(-migrate 同样不补建索引)。
		{name: "false + 缺索引", autoMigrate: &no, report: schemamigrate.Report{Warnings: []string{missingIndexWarningSample}},
			wantErr: true},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			up := &fakeSchemaRunner{}
			plan := &fakeSchemaRunner{}
			active := plan
			if tc.wantUp {
				active = up
			}
			active.report, active.err = tc.report, tc.err
			c := config.Config{Schema: config.SchemaConf{AutoMigrate: tc.autoMigrate}}

			err := ensureSchema(context.Background(), nil, c, up.run, plan.run)

			if tc.wantUp && (up.calls != 1 || plan.calls != 0) {
				t.Fatalf("应只跑 Up:up=%d plan=%d", up.calls, plan.calls)
			}
			if !tc.wantUp && (plan.calls != 1 || up.calls != 0) {
				t.Fatalf("AutoMigrate=false 应只跑只读 Plan:up=%d plan=%d", up.calls, plan.calls)
			}
			if active.opts.Database != data.DatabaseName {
				t.Errorf("库名断言应为 %q,实际 %q", data.DatabaseName, active.opts.Database)
			}
			if len(active.opts.Tables) != len(data.Tables()) {
				t.Errorf("表清单应来自 data.Tables()(%d 张),实际 %d 张", len(data.Tables()), len(active.opts.Tables))
			}
			if active.opts.AllowModifyColumn {
				t.Error("常驻启动的 Up / Plan 不得获得 MODIFY COLUMN 授权")
			}
			if tc.wantErr != (err != nil) {
				t.Fatalf("wantErr=%v,实际 err=%v", tc.wantErr, err)
			}
			if tc.wantRemedy && !strings.Contains(err.Error(), migrateRemedyCommand) {
				t.Errorf("拒绝启动的错误应带补救命令 %q,实际: %v", migrateRemedyCommand, err)
			}
			if tc.err != nil && !errors.Is(err, tc.err) {
				t.Errorf("错误应包裹原始错误 %v,实际: %v", tc.err, err)
			}
		})
	}
}

// nopConnector 让 runMigration 拿到一个非 nil、但永远不会被连上的 *sql.DB(sql.OpenDB 是惰性的)。
type nopConnector struct{}

func (nopConnector) Connect(context.Context) (driver.Conn, error) {
	return nil, errors.New("测试里的 *sql.DB 不应被连接")
}

func (nopConnector) Driver() driver.Driver { return nopDriver{} }

type nopDriver struct{}

func (nopDriver) Open(string) (driver.Conn, error) {
	return nil, errors.New("测试里的 *sql.DB 不应被连接")
}

// TestRunMigrationExitCodes 钉住 `-migrate` 的退出码(D-14:0 成功 / 1 失败 / 3 锁忙 / 4 需人工);
// K8s 的 mail-migrate Job 只看退出码,最要紧的是"缺索引 → 4"。
func TestRunMigrationExitCodes(t *testing.T) {
	logtest.Discard(t)
	for _, tc := range []struct {
		name   string
		report schemamigrate.Report
		err    error
		want   int
	}{
		{name: "建表成功", report: schemamigrate.Report{Statements: []string{"CREATE TABLE mail_personal ..."}}, want: schemamigrate.ExitOK},
		{name: "只有多余列告警照常成功", report: schemamigrate.Report{Warnings: []string{"extra column"}}, want: schemamigrate.ExitOK},
		{name: "缺索引升级成需人工", report: schemamigrate.Report{Warnings: []string{missingIndexWarningSample}}, want: schemamigrate.ExitManual},
		{name: "需人工项", report: schemamigrate.Report{Manual: []string{"type drift"}}, want: schemamigrate.ExitManual},
		{name: "锁忙可重试", err: schemamigrate.ErrLockBusy, want: schemamigrate.ExitLockBusy},
		{name: "迁移出错", err: errors.New("dial tcp: connection refused"), want: schemamigrate.ExitFailed},
		{name: "出错且缺索引仍按出错", report: schemamigrate.Report{Warnings: []string{missingIndexWarningSample}},
			err: errors.New("ddl failed"), want: schemamigrate.ExitFailed},
	} {
		t.Run(tc.name, func(t *testing.T) {
			up := &fakeSchemaRunner{report: tc.report, err: tc.err}
			openMySQL := func(config.MySQLConf) (*sql.DB, error) { return sql.OpenDB(nopConnector{}), nil }
			if got := runMigration(config.Config{}, false, openMySQL, up.run); got != tc.want {
				t.Fatalf("退出码应为 %d,实际 %d", tc.want, got)
			}
			if up.calls != 1 {
				t.Fatalf("应恰好跑一次 Up,实际 %d 次", up.calls)
			}
		})
	}

	t.Run("连库失败按失败退出且不跑迁移", func(t *testing.T) {
		up := &fakeSchemaRunner{}
		openMySQL := func(config.MySQLConf) (*sql.DB, error) { return nil, errors.New("unknown database") }
		if got := runMigration(config.Config{}, false, openMySQL, up.run); got != schemamigrate.ExitFailed {
			t.Fatalf("连不上库应退出 %d,实际 %d", schemamigrate.ExitFailed, got)
		}
		if up.calls != 0 {
			t.Fatalf("连不上库时不得跑迁移,实际 %d 次", up.calls)
		}
	})

	t.Run("-allow-modify 原样传给 Up", func(t *testing.T) {
		up := &fakeSchemaRunner{}
		openMySQL := func(config.MySQLConf) (*sql.DB, error) { return sql.OpenDB(nopConnector{}), nil }
		runMigration(config.Config{}, true, openMySQL, up.run)
		if !up.opts.AllowModifyColumn {
			t.Fatal("显式 -migrate -allow-modify 时,改列授权必须传到 Up")
		}
	})
}

// TestValidateMigrationFlags:`-allow-modify` 不带 `-migrate` 必须报错(常驻启动继承改列授权 = 滚动更新时可能线上重建大表)。
func TestValidateMigrationFlags(t *testing.T) {
	for _, tc := range []struct {
		name        string
		migrateOnly bool
		allowModify bool
		wantErr     bool
	}{
		{name: "常驻启动"},
		{name: "默认迁移", migrateOnly: true},
		{name: "显式迁移授权改列", migrateOnly: true, allowModify: true},
		{name: "常驻启动不得授权改列", allowModify: true, wantErr: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			err := validateMigrationFlags(tc.migrateOnly, tc.allowModify)
			if (err != nil) != tc.wantErr {
				t.Fatalf("wantErr=%v,实际 err=%v", tc.wantErr, err)
			}
			if tc.wantErr && !strings.Contains(err.Error(), "-migrate") {
				t.Fatalf("拒绝信息必须说明缺少 -migrate: %v", err)
			}
		})
	}
}
