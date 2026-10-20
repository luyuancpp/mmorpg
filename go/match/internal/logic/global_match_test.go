package logic

import (
	"context"
	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"match/internal/authority"
	"match/internal/config"
	"match/internal/discovery"
	"match/internal/svc"
	pb "proto/match"
	"testing"
)

func globalCandidate(pid, queued, now uint64, first, second uint32) *pb.MatchAuthorityTicketRecord {
	p := &pb.MatchNetworkProfile{PlanId: "p", SessionBinding: "s"}
	for region, rtt := range map[uint32]uint32{1: first, 2: second} {
		p.Receipts = append(p.Receipts, &pb.NetworkProbeReceipt{Grant: &pb.NetworkProbeGrant{PlayerId: pid, PhysicalRegion: region, ExpiresAtMs: now + 1000}, RttMs: rtt, SampleCount: 3, MeasuredAtMs: now, ExpiresAtMs: now + 1000})
	}
	b, _ := proto.Marshal(p)
	return &pb.MatchAuthorityTicketRecord{PlayerId: pid, Mode: 3, ConfigId: 1, TeamSize: 2, State: 1, EnqueuedMs: queued, NetworkProfile: b, RatingCenti: 150000}
}
func TestGlobalRegionPriorityAndHardMemberLatency(t *testing.T) {
	s := &svc.ServiceContext{Config: config.Config{GlobalMatch: config.GlobalMatchConf{HardLatencyMs: 180, CrossRegionWaitSeconds: 30}}}
	nodes := []discovery.NodeEntry{{NodeId: 1, NodeUuid: "a", PhysicalRegion: 1}, {NodeId: 2, NodeUuid: "b", PhysicalRegion: 2}}
	n := uint64(100000)
	a := globalCandidate(1, 0, n, 20, 100)
	b := globalCandidate(2, 0, n, 100, 20)
	c := globalCandidate(3, 90000, n, 30, 120)
	choice := pickGlobalGroup(s, []*pb.MatchAuthorityTicketRecord{a, b, c}, nodes, n)
	require.NotNil(t, choice)
	require.Equal(t, uint32(1), choice.region)
	require.Equal(t, uint64(3), choice.group[1].PlayerId, "本区够人，即使旧票已过扩区时限也先本区")
	choice = pickGlobalGroup(s, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n)
	require.NotNil(t, choice, "双方久等后可进入共同可用机房")
	b.EnqueuedMs = 90000
	require.Nil(t, pickGlobalGroup(s, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n), "年轻候选不能被队长等待时间强行扩区")
	b = globalCandidate(2, 0, n, 250, 20)
	a = globalCandidate(1, 0, n, 20, 250)
	require.Nil(t, pickGlobalGroup(s, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n), "没有全员低于硬上限的共同机房就继续等待")
}
func TestGlobalSoloDoesNotCollectSecondCandidate(t *testing.T) {
	s := &svc.ServiceContext{Config: config.Config{GlobalMatch: config.GlobalMatchConf{HardLatencyMs: 180, CrossRegionWaitSeconds: 30}}}
	a, b := globalCandidate(1, 0, 100000, 20, 100), globalCandidate(2, 0, 100000, 20, 100)
	a.TeamSize = 1
	b.TeamSize = 1
	choice := pickGlobalGroup(s, []*pb.MatchAuthorityTicketRecord{a, b}, []discovery.NodeEntry{{NodeId: 1, NodeUuid: "a", PhysicalRegion: 1}}, 100000)
	require.NotNil(t, choice)
	require.Len(t, choice.group, 1)
}

func TestGlobalJoinRejectsMalformedIdentityBeforeAnySQLRead(t *testing.T) {
	s := &svc.ServiceContext{Authority: &authority.Store{}}
	l := NewJoinQueueLogic(context.Background(), s)
	for _, request := range []string{"", "bad", uuid.Nil.String(), "fF000000-0000-0000-0000-000000000001"} {
		_, e := globalJoin(l, &pb.JoinQueueRequest{PlayerId: 1, RequestId: request}, 2, 1)
		require.Equal(t, codes.InvalidArgument, status.Code(e))
	}
}
