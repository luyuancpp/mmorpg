package server

import (
	"context"
	"testing"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/grpc"

	"mail/internal/constants"
	"mail/internal/metrics"

	base "proto/common/base"
	mailpb "proto/mail"

	"shared/grpcstats"
	"shared/serverbase"
)

// counterValue 读默认 registry 里某个 counter 在给定 label 组合下的当前值;序列不存在返回 0。
// mail 自己的指标是懒注册的,先调 metrics.Register(幂等);serverbase 的 rpc_inband_* 首次观测时自行注册。
func counterValue(t *testing.T, name string, labels map[string]string) float64 {
	t.Helper()
	metrics.Register()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather 指标失败: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != name {
			continue
		}
	metricLoop:
		for _, m := range mf.GetMetric() {
			got := make(map[string]string, len(m.GetLabel()))
			for _, l := range m.GetLabel() {
				got[l.GetName()] = l.GetValue()
			}
			for k, v := range labels {
				if got[k] != v {
					continue metricLoop
				}
			}
			return m.GetCounter().GetValue()
		}
	}
	return 0
}

// TestInbandInterceptorClassifiesMailCodes 跑生产里真正挂上去的那条链(serverbase + constants.TipClassifier()):
// mail 段的码全是业务拒绝(fault 计数不动、reject +1、不进 unknown_code);唯一的 fault 是 ErrStorage。
// 一个 mail 码若被判成 unknown_code,说明导表产物里没有 17000 段(Tip.xlsx 被整表覆盖过,设计 §10 ① 的假绿陷阱)。
func TestInbandInterceptorClassifiesMailCodes(t *testing.T) {
	logtest.Discard(t)
	const fullMethod = mailpb.ClientPlayerMail_ReadMail_FullMethodName
	labels := map[string]string{"method": grpcstats.ShortMethod(fullMethod), "source": "tip_info"}
	itc := serverbase.UnaryInterceptor(serverbase.Options{TipClassifier: constants.TipClassifier()})
	info := &grpc.UnaryServerInfo{FullMethod: fullMethod}

	call := func(code uint32) {
		t.Helper()
		resp, err := itc(context.Background(), &mailpb.ReadMailRequest{}, info, func(ctx context.Context, req any) (any, error) {
			return &mailpb.ReadMailResponse{ErrorMessage: &base.TipInfoMessage{Id: code}}, nil
		})
		if err != nil {
			t.Fatalf("拦截器不该改变 handler 的 error 返回: %v", err)
		}
		if got := resp.(*mailpb.ReadMailResponse).GetErrorMessage().GetId(); got != code {
			t.Fatalf("响应里的 tip id 被改写: %d", got)
		}
	}

	for _, code := range []uint32{
		constants.ErrClaimNotOpen, constants.ErrNotFound, constants.ErrExpired, constants.ErrRecipientBoxFull,
		constants.ErrAttachmentNotOpen, constants.ErrAttachmentInvalid, constants.ErrRequestConflict, constants.ErrSystemMailLimit,
		constants.ErrInvalidParameter,
	} {
		fault := counterValue(t, "rpc_inband_fault_total", labels)
		reject := counterValue(t, "rpc_inband_reject_total", labels)
		unknown := counterValue(t, "rpc_inband_unknown_code_total", labels)
		call(code)
		if got := counterValue(t, "rpc_inband_reject_total", labels); got != reject+1 {
			t.Errorf("码 %d 应计 reject +1,实际 %v -> %v", code, reject, got)
		}
		if got := counterValue(t, "rpc_inband_fault_total", labels); got != fault {
			t.Errorf("码 %d 是业务拒绝,不该计 fault", code)
		}
		if got := counterValue(t, "rpc_inband_unknown_code_total", labels); got != unknown {
			t.Errorf("码 %d 必须被 TipClassifier 认领,不该计 unknown_code", code)
		}
	}

	fault := counterValue(t, "rpc_inband_fault_total", labels)
	call(constants.ErrStorage)
	if got := counterValue(t, "rpc_inband_fault_total", labels); got != fault+1 {
		t.Errorf("ErrStorage 是 mail 唯一的 fault 码,应计 fault +1,实际 %v -> %v", fault, got)
	}
}
