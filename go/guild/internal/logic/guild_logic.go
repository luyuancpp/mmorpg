package logic

import (
	"context"
	"fmt"
	"time"

	"github.com/bwmarrin/snowflake"
	"github.com/zeromicro/go-zero/core/logx"

	"guild/internal/constants"
	"guild/internal/data"
	base "proto/common/base"
	pb "proto/guild"
)

type GuildLogic struct {
	repo      *data.GuildRepo
	snowflake *snowflake.Node
}

func NewGuildLogic(repo *data.GuildRepo, sf *snowflake.Node) *GuildLogic {
	return &GuildLogic{repo: repo, snowflake: sf}
}

func tipErr(id uint32, msg string) *base.TipInfoMessage {
	return &base.TipInfoMessage{Id: id, Parameters: []string{msg}}
}

func (l *GuildLogic) CreateGuild(ctx context.Context, req *pb.CreateGuildRequest) (*pb.CreateGuildResponse, error) {
	// Check if player already in a guild
	existingGuildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, fmt.Errorf("check existing guild: %w", err)
	}
	if existingGuildID > 0 {
		return &pb.CreateGuildResponse{ErrorMessage: tipErr(constants.ErrAlreadyInGuild, "already in a guild")}, nil
	}

	now := time.Now().UnixMilli()
	guildID := uint64(l.snowflake.Generate().Int64())

	guild := &data.GuildData{
		GuildID:      guildID,
		Name:         req.Name,
		LeaderID:     req.PlayerId,
		Level:        constants.DefaultInitLevel,
		CreateTimeMs: now,
		MaxMembers:   constants.DefaultMaxMembers,
		ZoneID:       req.ZoneId,
		Members: []data.MemberData{
			{
				PlayerID:     req.PlayerId,
				Role:         constants.RoleLeader,
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
	// 新公会加入排行榜（初始分数 0）
	if err := l.repo.UpdateGuildScore(ctx, guildID, req.ZoneId, 0); err != nil {
		logx.Errorf("init guild rank score: %v", err)
	}

	return &pb.CreateGuildResponse{Guild: toProtoGuild(guild)}, nil
}

func (l *GuildLogic) GetGuild(ctx context.Context, req *pb.GetGuildRequest) (*pb.GetGuildResponse, error) {
	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.GetGuildResponse{ErrorMessage: tipErr(constants.ErrGuildNotFound, "guild not found")}, nil
	}
	return &pb.GetGuildResponse{Guild: toProtoGuild(guild)}, nil
}

func (l *GuildLogic) GetPlayerGuild(ctx context.Context, req *pb.GetPlayerGuildRequest) (*pb.GetPlayerGuildResponse, error) {
	guildID, err := l.repo.GetPlayerGuildID(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	if guildID == 0 {
		return &pb.GetPlayerGuildResponse{ErrorMessage: tipErr(constants.ErrNotInGuild, "not in any guild")}, nil
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
		return &pb.JoinGuildResponse{ErrorMessage: tipErr(constants.ErrAlreadyInGuild, "already in a guild")}, nil
	}

	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.JoinGuildResponse{ErrorMessage: tipErr(constants.ErrGuildNotFound, "guild not found")}, nil
	}
	if uint32(len(guild.Members)) >= guild.MaxMembers {
		return &pb.JoinGuildResponse{ErrorMessage: tipErr(constants.ErrGuildFull, "guild is full")}, nil
	}

	now := time.Now().UnixMilli()
	guild.Members = append(guild.Members, data.MemberData{
		PlayerID:     req.PlayerId,
		Role:         constants.RoleMember,
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
		return &pb.LeaveGuildResponse{ErrorMessage: tipErr(constants.ErrNotInGuild, "not in any guild")}, nil
	}

	guild, err := l.repo.GetGuild(ctx, guildID)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.LeaveGuildResponse{ErrorMessage: tipErr(constants.ErrGuildNotFound, "guild not found")}, nil
	}
	if guild.LeaderID == req.PlayerId {
		return &pb.LeaveGuildResponse{ErrorMessage: tipErr(constants.ErrLeaderCantLeave, "leader cannot leave, disband instead")}, nil
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
		return &pb.DisbandGuildResponse{ErrorMessage: tipErr(constants.ErrNotInGuild, "not in any guild")}, nil
	}

	guild, err := l.repo.GetGuild(ctx, guildID)
	if err != nil {
		return nil, err
	}
	if guild == nil || guild.LeaderID != req.PlayerId {
		return &pb.DisbandGuildResponse{ErrorMessage: tipErr(constants.ErrNotLeader, "not guild leader")}, nil
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
	// 从排行榜移除
	if err := l.repo.RemoveGuildFromRank(ctx, guildID, guild.ZoneID); err != nil {
		logx.Errorf("remove guild %d from rank: %v", guildID, err)
	}
	return &pb.DisbandGuildResponse{}, nil
}

func (l *GuildLogic) SetAnnouncement(ctx context.Context, req *pb.SetAnnouncementRequest) (*pb.SetAnnouncementResponse, error) {
	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.SetAnnouncementResponse{ErrorMessage: tipErr(constants.ErrGuildNotFound, "guild not found")}, nil
	}

	// Check permission: leader or officer
	authorized := false
	for _, m := range guild.Members {
		if m.PlayerID == req.PlayerId && m.Role >= constants.RoleOfficer {
			authorized = true
			break
		}
	}
	if !authorized {
		return &pb.SetAnnouncementResponse{ErrorMessage: tipErr(constants.ErrNoPermission, "no permission")}, nil
	}

	guild.Announcement = req.Announcement
	if err := l.repo.SaveGuild(ctx, guild); err != nil {
		return nil, err
	}
	return &pb.SetAnnouncementResponse{}, nil
}

// ── 排行榜 ─────────────────────────────────────────────────────

func (l *GuildLogic) UpdateGuildScore(ctx context.Context, req *pb.UpdateGuildScoreRequest) (*pb.UpdateGuildScoreResponse, error) {
	guild, err := l.repo.GetGuild(ctx, req.GuildId)
	if err != nil {
		return nil, err
	}
	if guild == nil {
		return &pb.UpdateGuildScoreResponse{ErrorMessage: tipErr(constants.ErrGuildNotFound, "guild not found")}, nil
	}
	// Use the guild's own zone_id for per-zone ranking
	zoneID := guild.ZoneID
	if req.ZoneId > 0 {
		zoneID = req.ZoneId
	}
	if err := l.repo.UpdateGuildScore(ctx, req.GuildId, zoneID, req.Score); err != nil {
		return nil, fmt.Errorf("update guild score: %w", err)
	}
	return &pb.UpdateGuildScoreResponse{}, nil
}

func (l *GuildLogic) GetGuildRank(ctx context.Context, req *pb.GetGuildRankRequest) (*pb.GetGuildRankResponse, error) {
	pageSize := req.PageSize
	if pageSize == 0 {
		pageSize = 20
	}
	page := req.Page
	if page == 0 {
		page = 1
	}

	entries, total, err := l.repo.GetGuildRankPage(ctx, req.ZoneId, page, pageSize)
	if err != nil {
		return nil, fmt.Errorf("get guild rank page: %w", err)
	}

	pbEntries, err := l.enrichRankEntries(ctx, entries)
	if err != nil {
		return nil, err
	}

	return &pb.GetGuildRankResponse{
		Entries:    pbEntries,
		TotalCount: total,
		Page:       page,
		PageSize:   pageSize,
	}, nil
}

func (l *GuildLogic) GetGuildRankByGuild(ctx context.Context, req *pb.GetGuildRankByGuildRequest) (*pb.GetGuildRankByGuildResponse, error) {
	entry, err := l.repo.GetGuildRank(ctx, req.GuildId, req.ZoneId)
	if err != nil {
		return nil, err
	}
	if entry.Rank == 0 {
		return &pb.GetGuildRankByGuildResponse{ErrorMessage: tipErr(constants.ErrNotRanked, "guild not ranked")}, nil
	}

	guild, err := l.repo.GetGuild(ctx, entry.GuildID)
	if err != nil {
		return nil, err
	}

	pbEntry := &pb.GuildRankEntry{
		GuildId: entry.GuildID,
		Score:   entry.Score,
		Rank:    entry.Rank,
	}
	if guild != nil {
		pbEntry.Name = guild.Name
		pbEntry.LeaderId = guild.LeaderID
		pbEntry.Level = guild.Level
		pbEntry.MemberCount = uint32(len(guild.Members))
	}

	return &pb.GetGuildRankByGuildResponse{Entry: pbEntry}, nil
}

// enrichRankEntries fills in guild name/level/member_count from cache for a page of rank entries.
func (l *GuildLogic) enrichRankEntries(ctx context.Context, entries []data.RankEntry) ([]*pb.GuildRankEntry, error) {
	result := make([]*pb.GuildRankEntry, 0, len(entries))
	for _, e := range entries {
		pbEntry := &pb.GuildRankEntry{
			GuildId: e.GuildID,
			Score:   e.Score,
			Rank:    e.Rank,
		}
		guild, err := l.repo.GetGuild(ctx, e.GuildID)
		if err != nil {
			logx.Errorf("enrich rank entry guild %d: %v", e.GuildID, err)
		} else if guild != nil {
			pbEntry.Name = guild.Name
			pbEntry.LeaderId = guild.LeaderID
			pbEntry.Level = guild.Level
			pbEntry.MemberCount = uint32(len(guild.Members))
		}
		result = append(result, pbEntry)
	}
	return result, nil
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
		ZoneId:       g.ZoneID,
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
