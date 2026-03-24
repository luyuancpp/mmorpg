package logic

import (
	"context"
	"fmt"
	"time"

	"github.com/bwmarrin/snowflake"
	"github.com/zeromicro/go-zero/core/logx"

	"guild/internal/data"
	base "guild/proto/common/base"
	pb "guild/proto/guild"
)

type GuildLogic struct {
	repo      *data.GuildRepo
	snowflake *snowflake.Node
}

func NewGuildLogic(repo *data.GuildRepo, sf *snowflake.Node) *GuildLogic {
	return &GuildLogic{repo: repo, snowflake: sf}
}

func (l *GuildLogic) CreateGuild(ctx context.Context, req *pb.CreateGuildRequest) (*pb.CreateGuildResponse, error) {
	// Check if player already in a guild
	existingGuildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, fmt.Errorf("check existing guild: %w", err)
	}
	if existingGuildID > 0 {
		return &pb.CreateGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 1, Parameters: []string{"already in a guild"}},
		}, nil
	}

	now := time.Now().UnixMilli()
	guildID := uint64(l.snowflake.Generate().Int64())

	guild := &data.GuildData{
		GuildID:      guildID,
		Name:         req.Name,
		LeaderID:     req.PlayerId,
		Level:        1,
		CreateTimeMs: now,
		MaxMembers:   50,
		Members: []data.MemberData{
			{
				PlayerID:     req.PlayerId,
				Role:         3, // leader
				JoinTimeMs:   now,
				LastActiveMs: now,
			},
		},
	}

	if err := l.repo.SaveGuild(ctx, guild); err != nil {
		return nil, fmt.Errorf("save guild: %w", err)
	}
	if err := l.repo.SetPlayerGuild(ctx, req.PlayerId, guildID); err != nil {
		logx.Errorf("set player guild mapping: %v", err)
	}

	return &pb.CreateGuildResponse{Guild: toProtoGuild(guild)}, nil
}

func (l *GuildLogic) GetGuild(ctx context.Context, req *pb.GetGuildRequest) (*pb.GetGuildResponse, error) {
	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.GetGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 2, Parameters: []string{"guild not found"}},
		}, nil
	}
	return &pb.GetGuildResponse{Guild: toProtoGuild(guild)}, nil
}

func (l *GuildLogic) GetPlayerGuild(ctx context.Context, req *pb.GetPlayerGuildRequest) (*pb.GetPlayerGuildResponse, error) {
	guildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	if guildID == 0 {
		return &pb.GetPlayerGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 3, Parameters: []string{"not in any guild"}},
		}, nil
	}

	guild, err := l.repo.GetGuild(ctx, guildID)
	if err != nil {
		return nil, err
	}
	return &pb.GetPlayerGuildResponse{Guild: toProtoGuild(guild)}, nil
}

func (l *GuildLogic) JoinGuild(ctx context.Context, req *pb.JoinGuildRequest) (*pb.JoinGuildResponse, error) {
	existingGuildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	if existingGuildID > 0 {
		return &pb.JoinGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 1, Parameters: []string{"already in a guild"}},
		}, nil
	}

	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.JoinGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 2, Parameters: []string{"guild not found"}},
		}, nil
	}
	if uint32(len(guild.Members)) >= guild.MaxMembers {
		return &pb.JoinGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 4, Parameters: []string{"guild is full"}},
		}, nil
	}

	now := time.Now().UnixMilli()
	guild.Members = append(guild.Members, data.MemberData{
		PlayerID:     req.PlayerId,
		Role:         0,
		JoinTimeMs:   now,
		LastActiveMs: now,
	})

	if err := l.repo.SaveGuild(ctx, guild); err != nil {
		return nil, err
	}
	if err := l.repo.SetPlayerGuild(ctx, req.PlayerId, req.GuildId); err != nil {
		logx.Errorf("set player guild mapping: %v", err)
	}
	return &pb.JoinGuildResponse{}, nil
}

func (l *GuildLogic) LeaveGuild(ctx context.Context, req *pb.LeaveGuildRequest) (*pb.LeaveGuildResponse, error) {
	guildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	if guildID == 0 {
		return &pb.LeaveGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 3, Parameters: []string{"not in any guild"}},
		}, nil
	}

	guild, err := l.repo.GetGuild(ctx, guildID)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.LeaveGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 2, Parameters: []string{"guild not found"}},
		}, nil
	}
	if guild.LeaderID == req.PlayerId {
		return &pb.LeaveGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 5, Parameters: []string{"leader cannot leave, disband instead"}},
		}, nil
	}

	// Remove member
	newMembers := make([]data.MemberData, 0, len(guild.Members))
	for _, m := range guild.Members {
		if m.PlayerID != req.PlayerId {
			newMembers = append(newMembers, m)
		}
	}
	guild.Members = newMembers

	if err := l.repo.SaveGuild(ctx, guild); err != nil {
		return nil, err
	}
	if err := l.repo.RemovePlayerGuild(ctx, req.PlayerId); err != nil {
		logx.Errorf("remove player guild mapping: %v", err)
	}
	return &pb.LeaveGuildResponse{}, nil
}

func (l *GuildLogic) DisbandGuild(ctx context.Context, req *pb.DisbandGuildRequest) (*pb.DisbandGuildResponse, error) {
	guildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	if guildID == 0 {
		return &pb.DisbandGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 3, Parameters: []string{"not in any guild"}},
		}, nil
	}

	guild, err := l.repo.GetGuild(ctx, guildID)
	if err != nil {
		return nil, err
	}
	if guild == nil || guild.LeaderID != req.PlayerId {
		return &pb.DisbandGuildResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 6, Parameters: []string{"not guild leader"}},
		}, nil
	}

	// Remove all member mappings
	for _, m := range guild.Members {
		if err := l.repo.RemovePlayerGuild(ctx, m.PlayerID); err != nil {
			logx.Errorf("remove member %d guild mapping: %v", m.PlayerID, err)
		}
	}

	if err := l.repo.DeleteGuild(ctx, guildID); err != nil {
		return nil, err
	}
	return &pb.DisbandGuildResponse{}, nil
}

func (l *GuildLogic) SetAnnouncement(ctx context.Context, req *pb.SetAnnouncementRequest) (*pb.SetAnnouncementResponse, error) {
	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.SetAnnouncementResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 2, Parameters: []string{"guild not found"}},
		}, nil
	}

	// Check permission: leader or officer
	authorized := false
	for _, m := range guild.Members {
		if m.PlayerID == req.PlayerId && m.Role >= 1 {
			authorized = true
			break
		}
	}
	if !authorized {
		return &pb.SetAnnouncementResponse{
			ErrorMessage: &base.TipInfoMessage{Id: 7, Parameters: []string{"no permission"}},
		}, nil
	}

	guild.Announcement = req.Announcement
	if err := l.repo.SaveGuild(ctx, guild); err != nil {
		return nil, err
	}
	return &pb.SetAnnouncementResponse{}, nil
}

// ── Proto conversion ───────────────────────────────────────────

func toProtoGuild(g *data.GuildData) *pb.GuildInfo {
	if g == nil {
		return nil
	}
	info := &pb.GuildInfo{
		GuildId:      g.GuildID,
		Name:         g.Name,
		LeaderId:     g.LeaderID,
		Level:        g.Level,
		Announcement: g.Announcement,
		CreateTimeMs: g.CreateTimeMs,
		MaxMembers:   g.MaxMembers,
	}
	for _, m := range g.Members {
		info.Members = append(info.Members, &pb.GuildMember{
			PlayerId:     m.PlayerID,
			Role:         m.Role,
			JoinTimeMs:   m.JoinTimeMs,
			LastActiveMs: m.LastActiveMs,
			Contribution: m.Contribution,
		})
	}
	return info
}
