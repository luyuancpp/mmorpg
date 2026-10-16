package server

import (
	"context"
	"errors"
	"testing"
	"time"

	"player_inbox/internal/constants"

	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx/logtest"
)

type fakeConsumer struct {
	err         error
	sawDeadline bool
}

func (f *fakeConsumer) Fetch(ctx context.Context, req *inboxpb.FetchPlayerInboxRequest) (*inboxpb.FetchPlayerInboxResponse, error) {
	_, f.sawDeadline = ctx.Deadline()
	if f.err != nil {
		return nil, f.err
	}
	return &inboxpb.FetchPlayerInboxResponse{PlayerId: req.GetPlayerId(), Fence: inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_NO_STREAM}, nil
}

func (f *fakeConsumer) Ack(ctx context.Context, req *inboxpb.AckPlayerInboxRequest) (*inboxpb.AckPlayerInboxResponse, error) {
	_, f.sawDeadline = ctx.Deadline()
	if f.err != nil {
		return nil, f.err
	}
	return &inboxpb.AckPlayerInboxResponse{PlayerId: req.GetPlayerId()}, nil
}

// TestConsumerServerFoldsFaultsInBand:存储故障 in-band 回 kPlayerInboxStoreUnavailable 并原样回显关联字段
// (scene 靠 player_id + domain + correlation_id 找回在途状态,设计 §7.11);gRPC 层永不报错。
func TestConsumerServerFoldsFaultsInBand(t *testing.T) {
	logtest.Discard(t)
	store := &fakeConsumer{err: errors.New("db down")}
	s := NewPlayerInboxServer(store, 2500*time.Millisecond)
	fetch, err := s.FetchItems(context.Background(), &inboxpb.FetchPlayerInboxRequest{
		PlayerId: 7, Domain: inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION, CorrelationId: 11,
	})
	if err != nil || fetch.GetErrorMessage().GetId() != constants.ErrStoreUnavailable ||
		fetch.GetPlayerId() != 7 || fetch.GetCorrelationId() != 11 || fetch.GetDomain() == 0 {
		t.Fatalf("FetchItems = %+v, %v", fetch, err)
	}
	if !store.sawDeadline {
		t.Fatal("请求应带整请求预算截止时间")
	}
	ack, err := s.AckItems(context.Background(), &inboxpb.AckPlayerInboxRequest{PlayerId: 7, CorrelationId: 12})
	if err != nil || ack.GetErrorMessage().GetId() != constants.ErrStoreUnavailable || ack.GetCorrelationId() != 12 {
		t.Fatalf("AckItems = %+v, %v", ack, err)
	}

	store.err = nil
	fetch, err = s.FetchItems(context.Background(), &inboxpb.FetchPlayerInboxRequest{PlayerId: 8})
	if err != nil || fetch.GetErrorMessage() != nil || fetch.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_NO_STREAM {
		t.Fatalf("成功路径应原样透传: %+v, %v", fetch, err)
	}
}
