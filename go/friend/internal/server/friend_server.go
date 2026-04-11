package server

import (
	"context"

	"friend/internal/logic"
	pb "proto/friend"
)

type FriendServer struct {
	pb.UnimplementedFriendServiceServer
	logic *logic.FriendLogic
}

func NewFriendServer(l *logic.FriendLogic) *FriendServer {
	return &FriendServer{logic: l}
}

func (s *FriendServer) AddFriend(ctx context.Context, req *pb.AddFriendRequest) (*pb.AddFriendResponse, error) {
	return s.logic.AddFriend(ctx, req)
}

func (s *FriendServer) AcceptFriend(ctx context.Context, req *pb.AcceptFriendRequest) (*pb.AcceptFriendResponse, error) {
	return s.logic.AcceptFriend(ctx, req)
}

func (s *FriendServer) RejectFriend(ctx context.Context, req *pb.RejectFriendRequest) (*pb.RejectFriendResponse, error) {
	return s.logic.RejectFriend(ctx, req)
}

func (s *FriendServer) RemoveFriend(ctx context.Context, req *pb.RemoveFriendRequest) (*pb.RemoveFriendResponse, error) {
	return s.logic.RemoveFriend(ctx, req)
}

func (s *FriendServer) GetFriendList(ctx context.Context, req *pb.GetFriendListRequest) (*pb.GetFriendListResponse, error) {
	return s.logic.GetFriendList(ctx, req)
}

func (s *FriendServer) GetPendingRequests(ctx context.Context, req *pb.GetPendingRequestsRequest) (*pb.GetPendingRequestsResponse, error) {
	return s.logic.GetPendingRequests(ctx, req)
}

func (s *FriendServer) NotifyOnline(ctx context.Context, req *pb.NotifyOnlineRequest) (*pb.NotifyOnlineResponse, error) {
	return s.logic.NotifyOnline(ctx, req)
}

func (s *FriendServer) NotifyOffline(ctx context.Context, req *pb.NotifyOfflineRequest) (*pb.NotifyOfflineResponse, error) {
	return s.logic.NotifyOffline(ctx, req)
}
