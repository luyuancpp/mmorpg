// Package server 是 player_inbox 的 gRPC 适配层:PlayerInbox(scene 调 Fetch / Ack)与 PlayerInboxAdmin(运维)。
// 只做「套整请求预算 → 调 inbox / logic → 故障折成 in-band kPlayerInboxStoreUnavailable」,不含业务判定。
//
// 错误码规定(设计 §1.4):所有响应只用 TipInfoMessage error_message,不带 uint32 error_code;
// handler 一律 `return resp, nil`,由 serverbase 拦截器按 Tip.xlsx 的 fault 列定性。
// 例外:运维口令不对由 AdminAuthInterceptor 回 gRPC PermissionDenied(设计 §11.4 验收)。
package server

import (
	"context"
	"time"

	"player_inbox/internal/constants"

	base "proto/common/base"
	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx"
)

// ConsumerStore 是 scene 侧协议需要的存储能力(*inbox.Store 实现它)。
type ConsumerStore interface {
	Fetch(ctx context.Context, req *inboxpb.FetchPlayerInboxRequest) (*inboxpb.FetchPlayerInboxResponse, error)
	Ack(ctx context.Context, req *inboxpb.AckPlayerInboxRequest) (*inboxpb.AckPlayerInboxResponse, error)
}

// PlayerInboxServer 实现 inboxpb.PlayerInboxServer。
type PlayerInboxServer struct {
	inboxpb.UnimplementedPlayerInboxServer
	store  ConsumerStore
	budget time.Duration
}

// NewPlayerInboxServer。budget 是整请求 I/O 预算(config.RequestBudget,= Timeout − 回包余量)。
func NewPlayerInboxServer(store ConsumerStore, budget time.Duration) *PlayerInboxServer {
	return &PlayerInboxServer{store: store, budget: budget}
}

func storeUnavailable() *base.TipInfoMessage {
	return &base.TipInfoMessage{Id: constants.ErrStoreUnavailable}
}

// withBudget 给一次请求的全部 I/O 套同一个更早的截止时间:依赖变慢时先在预算内 in-band 回故障码,
// 不让 go-zero 的超时拦截器抢先回 DeadlineExceeded(同 trade P1-9)。
func withBudget(ctx context.Context, budget time.Duration) (context.Context, context.CancelFunc) {
	if budget <= 0 {
		return context.WithCancel(ctx)
	}
	return context.WithTimeout(ctx, budget)
}

// FetchItems 按 seq 升序拉 PENDING 条目并推进围栏(设计 §7.8)。
func (s *PlayerInboxServer) FetchItems(ctx context.Context, in *inboxpb.FetchPlayerInboxRequest) (*inboxpb.FetchPlayerInboxResponse, error) {
	ctx, cancel := withBudget(ctx, s.budget)
	defer cancel()
	resp, err := s.store.Fetch(ctx, in)
	if err != nil {
		logx.WithContext(ctx).Errorf("[player_inbox] FetchItems failed player=%d domain=%d epoch=%d scene_node=%d: %v",
			in.GetPlayerId(), in.GetDomain(), in.GetOwnerEpoch(), in.GetSceneNodeId(), err)
		return &inboxpb.FetchPlayerInboxResponse{
			ErrorMessage: storeUnavailable(),
			PlayerId:     in.GetPlayerId(), Domain: in.GetDomain(), CorrelationId: in.GetCorrelationId(),
		}, nil
	}
	return resp, nil
}

// AckItems 登记消费方已落盘的结局(设计 §7.7)。
func (s *PlayerInboxServer) AckItems(ctx context.Context, in *inboxpb.AckPlayerInboxRequest) (*inboxpb.AckPlayerInboxResponse, error) {
	ctx, cancel := withBudget(ctx, s.budget)
	defer cancel()
	resp, err := s.store.Ack(ctx, in)
	if err != nil {
		logx.WithContext(ctx).Errorf("[player_inbox] AckItems failed player=%d domain=%d epoch=%d decisions=%d: %v",
			in.GetPlayerId(), in.GetDomain(), in.GetOwnerEpoch(), len(in.GetDecisions()), err)
		return &inboxpb.AckPlayerInboxResponse{
			ErrorMessage: storeUnavailable(),
			PlayerId:     in.GetPlayerId(), Domain: in.GetDomain(), CorrelationId: in.GetCorrelationId(),
		}, nil
	}
	return resp, nil
}
