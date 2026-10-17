package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"strings"
	"testing"

	"rank/internal/config"
	"rank/internal/constants"
	"rank/internal/session"

	base "proto/common/base"
	rankpb "proto/rank"

	"shared/killswitch"
	"shared/serverbase"

	"github.com/zeromicro/go-zero/core/logx/logtest"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

// chainUnary 按 gRPC 的语义把一组一元拦截器串成一个 handler:切片第一个是最外层(与 grpc.ChainUnaryInterceptor 一致)。
// grpc-go 的 chainUnaryInterceptors 是包内私有函数,这里复刻一份(与 mail / friend / chat 的同名测试逐字相同)。
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

// TestBuildUnaryInterceptorsOrder 钉住链长与顺序:grpcstats → killswitch → session → serverbase(设计 §4.4)。
// 按可观测行为逐个探测(函数值不能比较):killswitch 是唯一不调 handler、回 Unavailable 的;session 是唯一让 handler
// 取到会话身份的;serverbase 是唯一把 fault 码打成 rpc_inband_fault 日志的;剩下的位置是 grpcstats。
func TestBuildUnaryInterceptorsOrder(t *testing.T) {
	ks := killswitch.New(killswitch.Config{})
	ks.SetRules(map[string]killswitch.Rule{
		strings.TrimPrefix(rankpb.ClientPlayerRank_GetRankPage_FullMethodName, "/"): {Deny: true, Reason: "单测:顺序探针"},
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

	info := &grpc.UnaryServerInfo{FullMethod: rankpb.ClientPlayerRank_GetRankPage_FullMethodName}
	// kServiceUnavailable 是 rank 唯一的 fault 码:用它当探针才看得出 serverbase 那一层在不在。
	faultResp := &rankpb.GetRankPageResponse{ErrorMessage: &base.TipInfoMessage{Id: constants.ErrServiceUnavailable}}

	for i, w := range want {
		t.Run(w.name, func(t *testing.T) {
			logs := logtest.NewCollector(t)
			var got observed
			_, err := chain[i](incomingSessionContext(t, 42), &rankpb.GetRankPageRequest{}, info,
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
	// 模式从生成常量取:运维的 etcdctl 示例(etc/rank.yaml)写的就是 rankpb.ClientPlayerRank/GetRankPage。
	ks.SetRules(map[string]killswitch.Rule{
		strings.TrimPrefix(rankpb.ClientPlayerRank_GetRankPage_FullMethodName, "/"): {Deny: true, Reason: "单测:榜 Redis 过载"},
	})
	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: rankpb.ClientPlayerRank_GetRankPage_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(ks), info, func(ctx context.Context, req any) (any, error) {
		handlerCalled = true
		return &rankpb.GetRankPageResponse{}, nil
	})
	resp, err := h(incomingSessionContext(t, 42), &rankpb.GetRankPageRequest{})
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
	if !strings.Contains(st.Message(), "GetRankPage") || !strings.Contains(st.Message(), "榜 Redis 过载") {
		t.Fatalf("错误文本缺少方法名/原因: %q", st.Message())
	}
}

// TestKillSwitchFailOpenWithoutRules:没有任何规则(etcd 没配 / 连不上)时链完全透明,handler 能取到会话身份。
func TestKillSwitchFailOpenWithoutRules(t *testing.T) {
	ks := killswitch.New(killswitch.Config{})
	ks.Start(context.Background(), nil)

	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: rankpb.ClientPlayerRank_GetRankPage_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(ks), info, func(ctx context.Context, req any) (any, error) {
		handlerCalled = true
		if _, ok := session.ClientPlayerID(ctx); !ok {
			t.Error("整条链走完后 handler 应能取到会话身份(D-9)")
		}
		return &rankpb.GetRankPageResponse{}, nil
	})
	if _, err := h(incomingSessionContext(t, 42), &rankpb.GetRankPageRequest{}); err != nil {
		t.Fatalf("无规则时必须放行: %v", err)
	}
	if !handlerCalled {
		t.Fatal("无规则时 handler 必须被调用(fail-open)")
	}
}

// TestBrokenSessionRejectedInChain(T7b):坏会话头 fail-closed(Unauthenticated),handler 不被调用。
func TestBrokenSessionRejectedInChain(t *testing.T) {
	logtest.Discard(t)
	handlerCalled := false
	info := &grpc.UnaryServerInfo{FullMethod: rankpb.ClientPlayerRank_GetRankPage_FullMethodName}
	h := chainUnary(buildUnaryInterceptors(killswitch.New(killswitch.Config{})), info,
		func(ctx context.Context, req any) (any, error) {
			handlerCalled = true
			return &rankpb.GetRankPageResponse{}, nil
		})
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs(session.MetadataKey, "%%%不是base64"))
	_, err := h(ctx, &rankpb.GetRankPageRequest{})
	if handlerCalled {
		t.Fatal("坏会话头走到了 handler")
	}
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("期望 Unauthenticated,得到 %v", err)
	}
}

// TestNodeInfoValueMatchesRegistryContract:nodeType 是 RankNodeService(13),Endpoint 与 GrpcEndpoint **都填**
// (只填前者时路由服拨不通,guild 踩过),注册前缀由枚举名派生为 RankNodeService.rpc。
func TestNodeInfoValueMatchesRegistryContract(t *testing.T) {
	build := nodeInfoValueBuilder(2, "10.0.0.9", 51100, 1_700_000_000)
	raw, err := build(7, "uuid-rank-test")
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
	if mirror.NodeId != 7 || mirror.ZoneId != 2 || mirror.NodeUuid != "uuid-rank-test" {
		t.Fatalf("身份字段不符: %+v", mirror)
	}
	if uint32(base.ENodeType_RankNodeService) != 13 {
		t.Fatalf("RankNodeService 枚举值应为 13,实际 %d", base.ENodeType_RankNodeService)
	}
	if mirror.NodeType != uint32(base.ENodeType_RankNodeService) {
		t.Fatalf("nodeType 应为 RankNodeService(13),实际 %d", mirror.NodeType)
	}
	if mirror.ProtocolType != uint32(base.ENodeProtocolType_PROTOCOL_GRPC) {
		t.Fatalf("protocolType 应为 PROTOCOL_GRPC,实际 %d", mirror.ProtocolType)
	}
	if mirror.Endpoint.Port != 51100 || mirror.GrpcEndpoint.Port != 51100 ||
		mirror.Endpoint.Ip != "10.0.0.9" || mirror.GrpcEndpoint.Ip != "10.0.0.9" {
		t.Fatalf("端点字段不符(Endpoint 与 GrpcEndpoint 必须同时填): %+v", mirror)
	}
	if got := base.ENodeType_name[int32(nodeType)] + ".rpc"; got != "RankNodeService.rpc" {
		t.Fatalf("注册前缀应为 RankNodeService.rpc,实际 %q", got)
	}
}

// TestBannerContract 钉住横幅的外部契约:就绪行逐字是 go_services.ps1 的判据;回落共享 Redis 时写明"只适合本地";
// 横幅里**没有** Redis 口令。
func TestBannerContract(t *testing.T) {
	const secretPassword = "redis-pw-should-never-print"
	c := config.Config{}
	c.ListenOn = "127.0.0.1:51100"
	c.Mode = "pro"
	c.Redis.RedisConf = redis.RedisConf{Host: "127.0.0.1:6379", Type: "node", Pass: secretPassword}
	c.DataServiceRpc.Etcd.Key = "dataservice.rpc"

	var buf bytes.Buffer
	printBanner(&buf, c, bannerInfo{advertise: "127.0.0.1:51100", consumer: "topic=match-rating-snapshot partitions=3 group=rank-rating"})
	out := buf.String()
	if !strings.Contains(out, "\n"+readyMarker+"\n") || !strings.Contains(readyMarker, "STARTED SUCCESSFULLY") {
		t.Fatalf("横幅缺少就绪行 %q:\n%s", readyMarker, out)
	}
	if !strings.Contains(out, "fallback to shared Redis; local development only") {
		t.Fatalf("RankRedis 没配时横幅应写明回落共享库:\n%s", out)
	}
	if !strings.Contains(out, "consumer:      topic=match-rating-snapshot") {
		t.Fatalf("横幅应带消费者状态行:\n%s", out)
	}
	if strings.Contains(out, secretPassword) {
		t.Fatal("横幅打印了 Redis 口令")
	}

	c.RankRedis = redis.RedisConf{Host: "rank-redis:6379", Type: "node", Pass: secretPassword}
	buf.Reset()
	printBanner(&buf, c, bannerInfo{consumer: "disabled"})
	if !strings.Contains(buf.String(), "rank-redis:6379 type=node (dedicated)") || strings.Contains(buf.String(), secretPassword) {
		t.Fatalf("独立 RankRedis 时横幅应写地址与 dedicated、不写口令:\n%s", buf.String())
	}
}
