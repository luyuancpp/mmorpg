package server

import (
	"context"

	"guild/internal/logic"
	pb "guild/proto/guild"
)

type GuildServer struct {
	pb.UnimplementedGuildServiceServer
	logic *logic.GuildLogic
}

func NewGuildServer(l *logic.GuildLogic) *GuildServer {
	return &GuildServer{logic: l}
}

func (s *GuildServer) CreateGuild(ctx context.Context, req *pb.CreateGuildRequest) (*pb.CreateGuildResponse, error) {
	return s.logic.CreateGuild(ctx, req)
}

func (s *GuildServer) GetGuild(ctx context.Context, req *pb.GetGuildRequest) (*pb.GetGuildResponse, error) {
	return s.logic.GetGuild(ctx, req)
}

func (s *GuildServer) GetPlayerGuild(ctx context.Context, req *pb.GetPlayerGuildRequest) (*pb.GetPlayerGuildResponse, error) {
	return s.logic.GetPlayerGuild(ctx, req)
}

func (s *GuildServer) JoinGuild(ctx context.Context, req *pb.JoinGuildRequest) (*pb.JoinGuildResponse, error) {
	return s.logic.JoinGuild(ctx, req)
}

func (s *GuildServer) LeaveGuild(ctx context.Context, req *pb.LeaveGuildRequest) (*pb.LeaveGuildResponse, error) {
	return s.logic.LeaveGuild(ctx, req)
}

func (s *GuildServer) DisbandGuild(ctx context.Context, req *pb.DisbandGuildRequest) (*pb.DisbandGuildResponse, error) {
	return s.logic.DisbandGuild(ctx, req)
}

func (s *GuildServer) SetAnnouncement(ctx context.Context, req *pb.SetAnnouncementRequest) (*pb.SetAnnouncementResponse, error) {
	return s.logic.SetAnnouncement(ctx, req)
}

func (s *GuildServer) UpdateGuildScore(ctx context.Context, req *pb.UpdateGuildScoreRequest) (*pb.UpdateGuildScoreResponse, error) {
	return s.logic.UpdateGuildScore(ctx, req)
}

func (s *GuildServer) GetGuildRank(ctx context.Context, req *pb.GetGuildRankRequest) (*pb.GetGuildRankResponse, error) {
	return s.logic.GetGuildRank(ctx, req)
}

func (s *GuildServer) GetGuildRankByGuild(ctx context.Context, req *pb.GetGuildRankByGuildRequest) (*pb.GetGuildRankByGuildResponse, error) {
	return s.logic.GetGuildRankByGuild(ctx, req)
}
