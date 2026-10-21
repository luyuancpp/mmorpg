package logic

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/require"
	"match/internal/config"
	"match/internal/discovery"
	"match/internal/svc"
	pb "proto/match"
)

func TestIndexedGlobalGroupReadsLocalPeersOutsideAnchorPage(t *testing.T) {
	s := &svc.ServiceContext{Config: config.Config{GlobalMatch: config.GlobalMatchConf{HardLatencyMs: 180, CrossRegionWaitSeconds: 30}}}
	n := uint64(100000)
	a, b, c := globalCandidate(1, 0, n, 20, 100), globalCandidate(2, 0, n, 100, 20), globalCandidate(3, 99000, n, 30, 120)
	nodes := []discovery.NodeEntry{{NodeId: 1, NodeUuid: "a", PhysicalRegion: 1}, {NodeId: 2, NodeUuid: "b", PhysicalRegion: 2}}
	choice := fixtureIndexedChoice(t, s, []*pb.MatchAuthorityTicketRecord{a, b}, []*pb.MatchAuthorityTicketRecord{a, b, c}, nodes, n, true)
	require.NotNil(t, choice)
	require.Equal(t, c.PlayerId, choice.group[1].PlayerId)
	require.Equal(t, a, choice.scannedUntil, "本区提前成局只能推进实际尝试锚点，不能跳掉后段")
	require.Nil(t, fixtureIndexedChoice(t, s, []*pb.MatchAuthorityTicketRecord{a, b}, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n, false), "初次投影修复结束前不能假报本区不足扩区")
}

func TestIndexedGlobalGroupDoesNotReassignPreferredRegionWhenNodeDisappears(t *testing.T) {
	s := &svc.ServiceContext{Config: config.Config{GlobalMatch: config.GlobalMatchConf{HardLatencyMs: 180, CrossRegionWaitSeconds: 30}}}
	n := uint64(100000)
	a, b := globalCandidate(1, 99000, n, 20, 100), globalCandidate(2, 99000, n, 30, 120)
	nodes := []discovery.NodeEntry{{NodeId: 2, NodeUuid: "b", PhysicalRegion: 2}}
	require.Nil(t, fixtureIndexedChoice(t, s, []*pb.MatchAuthorityTicketRecord{a, b}, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n, true))
	a.EnqueuedMs, b.EnqueuedMs = 0, 0
	choice := fixtureIndexedChoice(t, s, []*pb.MatchAuthorityTicketRecord{a, b}, []*pb.MatchAuthorityTicketRecord{a, b}, nodes, n, true)
	require.NotNil(t, choice)
	require.Equal(t, uint32(2), choice.region)
	require.Equal(t, b, choice.scannedUntil, "扩区之前已完成整批锚点的本区尝试")
}

func TestIndexedGlobalGroupReturnsPeerQueryFailure(t *testing.T) {
	s := &svc.ServiceContext{Config: config.Config{GlobalMatch: config.GlobalMatchConf{HardLatencyMs: 180, CrossRegionWaitSeconds: 30}}}
	a := globalCandidate(1, 0, 100000, 20, 100)
	failure := errors.New("fixture storage failure")
	choice, err := pickIndexedGlobalGroup(context.Background(), s, []*pb.MatchAuthorityTicketRecord{a}, []discovery.NodeEntry{{NodeId: 1, NodeUuid: "a", PhysicalRegion: 1}}, 100000, true, func(context.Context, *pb.MatchAuthorityTicketRecord, uint32, bool, uint64, float64) ([]*pb.MatchAuthorityTicketRecord, error) {
		return nil, failure
	})
	require.Nil(t, choice)
	require.ErrorIs(t, err, failure)
}
