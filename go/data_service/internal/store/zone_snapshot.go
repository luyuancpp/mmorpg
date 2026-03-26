package store

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

// ── Row types for guild/friend tables ──────────────────────────

type GuildRow struct {
	GuildID      uint64 `json:"guild_id"`
	Name         string `json:"name"`
	LeaderID     uint64 `json:"leader_id"`
	Level        uint32 `json:"level"`
	Announcement string `json:"announcement"`
	CreateTimeMs uint64 `json:"create_time_ms"`
	MaxMembers   uint32 `json:"max_members"`
	ZoneID       uint32 `json:"zone_id"`
}

type GuildMemberRow struct {
	GuildID    uint64 `json:"guild_id"`
	PlayerID   uint64 `json:"player_id"`
	Role       uint32 `json:"role"`
	JoinTimeMs uint64 `json:"join_time_ms"`
}

type FriendRow struct {
	PlayerID       uint64 `json:"player_id"`
	FriendPlayerID uint64 `json:"friend_player_id"`
	SinceMs        uint64 `json:"since_ms"`
}

type FriendRequestRow struct {
	FromPlayerID  uint64 `json:"from_player_id"`
	ToPlayerID    uint64 `json:"to_player_id"`
	RequestTimeMs uint64 `json:"request_time_ms"`
	Status        uint32 `json:"status"`
}

// ZoneSnapshotData is the JSON-serialized format stored in zone_snapshot.data.
type ZoneSnapshotData struct {
	Guilds         []GuildRow         `json:"guilds"`
	GuildMembers   []GuildMemberRow   `json:"guild_members"`
	Friends        []FriendRow        `json:"friends"`
	FriendRequests []FriendRequestRow `json:"friend_requests"`
	PlayerIDs      []uint64           `json:"player_ids"` // all zone players at snapshot time
}

// ZoneSnapshotRow represents a row in the zone_snapshot table.
type ZoneSnapshotRow struct {
	ID        uint64
	ZoneID    uint32
	CreatedAt uint64
	Reason    string
	Operator  string
	Data      []byte // JSON-serialized ZoneSnapshotData
}

// ZoneSnapshotMeta is like ZoneSnapshotRow but without the data blob.
type ZoneSnapshotMeta struct {
	ID            uint64
	ZoneID        uint32
	CreatedAt     uint64
	Reason        string
	Operator      string
	DataSizeBytes uint32
}

// ── DDL ────────────────────────────────────────────────────────

func (s *SnapshotStore) ensureZoneSnapshotTable() error {
	_, err := s.db.Exec(`CREATE TABLE IF NOT EXISTS zone_snapshot (
		id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
		zone_id INT UNSIGNED NOT NULL,
		created_at BIGINT UNSIGNED NOT NULL,
		reason VARCHAR(512) NOT NULL DEFAULT '',
		operator VARCHAR(128) NOT NULL DEFAULT '',
		data LONGBLOB,
		INDEX idx_zone_created (zone_id, created_at)
	) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4`)
	return err
}

// ── Create zone snapshot ───────────────────────────────────────

// CaptureZoneSnapshot reads guild/friend tables for a zone and stores a snapshot.
func (s *SnapshotStore) CaptureZoneSnapshot(ctx context.Context, zoneID uint32, playerIDs []uint64, reason, operator string) (uint64, error) {
	data, err := s.readZoneSocialData(ctx, zoneID, playerIDs)
	if err != nil {
		return 0, fmt.Errorf("read zone social data: %w", err)
	}
	data.PlayerIDs = playerIDs

	blob, err := json.Marshal(data)
	if err != nil {
		return 0, fmt.Errorf("marshal zone snapshot: %w", err)
	}

	now := uint64(time.Now().Unix())
	res, err := s.db.ExecContext(ctx,
		`INSERT INTO zone_snapshot (zone_id, created_at, reason, operator, data)
		 VALUES (?, ?, ?, ?, ?)`, zoneID, now, reason, operator, blob)
	if err != nil {
		return 0, err
	}

	id, err := res.LastInsertId()
	if err != nil {
		return 0, err
	}

	logx.Infof("[ZoneSnapshot] created id=%d zone=%d guilds=%d members=%d friends=%d requests=%d players=%d",
		id, zoneID, len(data.Guilds), len(data.GuildMembers),
		len(data.Friends), len(data.FriendRequests), len(data.PlayerIDs))

	return uint64(id), nil
}

// readZoneSocialData reads guild, guild_member, friend, friend_request tables for a zone.
func (s *SnapshotStore) readZoneSocialData(ctx context.Context, zoneID uint32, playerIDs []uint64) (*ZoneSnapshotData, error) {
	data := &ZoneSnapshotData{}

	// 1. Guilds by zone
	guilds, err := s.readGuilds(ctx, zoneID)
	if err != nil {
		return nil, fmt.Errorf("read guilds: %w", err)
	}
	data.Guilds = guilds

	// 2. Guild members for these guilds
	guildIDs := make([]uint64, len(guilds))
	for i, g := range guilds {
		guildIDs[i] = g.GuildID
	}
	if len(guildIDs) > 0 {
		members, err := s.readGuildMembers(ctx, guildIDs)
		if err != nil {
			return nil, fmt.Errorf("read guild members: %w", err)
		}
		data.GuildMembers = members
	}

	// 3. Friends for zone players
	if len(playerIDs) > 0 {
		friends, err := s.readFriends(ctx, playerIDs)
		if err != nil {
			return nil, fmt.Errorf("read friends: %w", err)
		}
		data.Friends = friends

		requests, err := s.readFriendRequests(ctx, playerIDs)
		if err != nil {
			return nil, fmt.Errorf("read friend requests: %w", err)
		}
		data.FriendRequests = requests
	}

	return data, nil
}

func (s *SnapshotStore) readGuilds(ctx context.Context, zoneID uint32) ([]GuildRow, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT guild_id, name, leader_id, level, COALESCE(announcement,''),
		        create_time_ms, max_members, zone_id
		 FROM guild WHERE zone_id = ?`, zoneID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var result []GuildRow
	for rows.Next() {
		var r GuildRow
		if err := rows.Scan(&r.GuildID, &r.Name, &r.LeaderID, &r.Level,
			&r.Announcement, &r.CreateTimeMs, &r.MaxMembers, &r.ZoneID); err != nil {
			return nil, err
		}
		result = append(result, r)
	}
	return result, rows.Err()
}

func (s *SnapshotStore) readGuildMembers(ctx context.Context, guildIDs []uint64) ([]GuildMemberRow, error) {
	if len(guildIDs) == 0 {
		return nil, nil
	}

	query := `SELECT guild_id, player_id, role, join_time_ms FROM guild_member WHERE guild_id IN (`
	args := make([]interface{}, len(guildIDs))
	for i, id := range guildIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args[i] = id
	}
	query += ")"

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var result []GuildMemberRow
	for rows.Next() {
		var r GuildMemberRow
		if err := rows.Scan(&r.GuildID, &r.PlayerID, &r.Role, &r.JoinTimeMs); err != nil {
			return nil, err
		}
		result = append(result, r)
	}
	return result, rows.Err()
}

func (s *SnapshotStore) readFriends(ctx context.Context, playerIDs []uint64) ([]FriendRow, error) {
	if len(playerIDs) == 0 {
		return nil, nil
	}

	query := `SELECT player_id, friend_player_id, since_ms FROM friend WHERE player_id IN (`
	args := make([]interface{}, len(playerIDs))
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args[i] = id
	}
	query += ")"

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var result []FriendRow
	for rows.Next() {
		var r FriendRow
		if err := rows.Scan(&r.PlayerID, &r.FriendPlayerID, &r.SinceMs); err != nil {
			return nil, err
		}
		result = append(result, r)
	}
	return result, rows.Err()
}

func (s *SnapshotStore) readFriendRequests(ctx context.Context, playerIDs []uint64) ([]FriendRequestRow, error) {
	if len(playerIDs) == 0 {
		return nil, nil
	}

	query := `SELECT from_player_id, to_player_id, request_time_ms, status
	          FROM friend_request WHERE from_player_id IN (`
	args := make([]interface{}, 0, len(playerIDs)*2)
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args = append(args, id)
	}
	query += ") OR to_player_id IN ("
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args = append(args, id)
	}
	query += ")"

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var result []FriendRequestRow
	for rows.Next() {
		var r FriendRequestRow
		if err := rows.Scan(&r.FromPlayerID, &r.ToPlayerID, &r.RequestTimeMs, &r.Status); err != nil {
			return nil, err
		}
		result = append(result, r)
	}
	return result, rows.Err()
}

// ── Read zone snapshot ─────────────────────────────────────────

// GetLatestZoneSnapshotBefore returns the most recent zone snapshot before the given time.
func (s *SnapshotStore) GetLatestZoneSnapshotBefore(ctx context.Context, zoneID uint32, beforeTime uint64) (*ZoneSnapshotRow, error) {
	row := &ZoneSnapshotRow{}
	err := s.db.QueryRowContext(ctx,
		`SELECT id, zone_id, created_at, reason, operator, data
		 FROM zone_snapshot
		 WHERE zone_id = ? AND created_at <= ?
		 ORDER BY created_at DESC LIMIT 1`, zoneID, beforeTime,
	).Scan(&row.ID, &row.ZoneID, &row.CreatedAt, &row.Reason, &row.Operator, &row.Data)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	return row, err
}

// ListZoneSnapshotsMeta returns metadata for a zone's snapshots.
func (s *SnapshotStore) ListZoneSnapshotsMeta(ctx context.Context, zoneID uint32, limit uint32) ([]*ZoneSnapshotMeta, error) {
	if limit == 0 {
		limit = 20
	}

	rows, err := s.db.QueryContext(ctx,
		`SELECT id, zone_id, created_at, reason, operator, COALESCE(LENGTH(data),0)
		 FROM zone_snapshot
		 WHERE zone_id = ?
		 ORDER BY created_at DESC LIMIT ?`, zoneID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var result []*ZoneSnapshotMeta
	for rows.Next() {
		m := &ZoneSnapshotMeta{}
		if err := rows.Scan(&m.ID, &m.ZoneID, &m.CreatedAt, &m.Reason, &m.Operator, &m.DataSizeBytes); err != nil {
			return nil, err
		}
		result = append(result, m)
	}
	return result, rows.Err()
}

// ── Restore zone snapshot ──────────────────────────────────────

// RestoreZoneSnapshot replaces guild/friend data for a zone from a snapshot.
// Runs within a single transaction for atomicity.
func (s *SnapshotStore) RestoreZoneSnapshot(ctx context.Context, snap *ZoneSnapshotRow) error {
	var data ZoneSnapshotData
	if err := json.Unmarshal(snap.Data, &data); err != nil {
		return fmt.Errorf("unmarshal zone snapshot: %w", err)
	}

	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	// 1. Collect guild IDs from snapshot for scoped delete
	guildIDs := make([]uint64, len(data.Guilds))
	for i, g := range data.Guilds {
		guildIDs[i] = g.GuildID
	}

	// 2. Delete current guild data for this zone
	if _, err := tx.ExecContext(ctx, `DELETE FROM guild WHERE zone_id = ?`, snap.ZoneID); err != nil {
		return fmt.Errorf("delete guilds: %w", err)
	}
	if len(guildIDs) > 0 {
		if err := deleteGuildMembersInTx(ctx, tx, guildIDs); err != nil {
			return fmt.Errorf("delete guild members: %w", err)
		}
	}

	// 3. Delete current friend data for zone players
	if len(data.PlayerIDs) > 0 {
		if err := deleteFriendsInTx(ctx, tx, data.PlayerIDs); err != nil {
			return fmt.Errorf("delete friends: %w", err)
		}
		if err := deleteFriendRequestsInTx(ctx, tx, data.PlayerIDs); err != nil {
			return fmt.Errorf("delete friend requests: %w", err)
		}
	}

	// 4. Insert snapshot data
	for _, g := range data.Guilds {
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO guild (guild_id, name, leader_id, level, announcement, create_time_ms, max_members, zone_id)
			 VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
			g.GuildID, g.Name, g.LeaderID, g.Level, g.Announcement, g.CreateTimeMs, g.MaxMembers, g.ZoneID,
		); err != nil {
			return fmt.Errorf("insert guild %d: %w", g.GuildID, err)
		}
	}

	for _, m := range data.GuildMembers {
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO guild_member (guild_id, player_id, role, join_time_ms)
			 VALUES (?, ?, ?, ?)`,
			m.GuildID, m.PlayerID, m.Role, m.JoinTimeMs,
		); err != nil {
			return fmt.Errorf("insert guild member %d/%d: %w", m.GuildID, m.PlayerID, err)
		}
	}

	for _, f := range data.Friends {
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO friend (player_id, friend_player_id, since_ms)
			 VALUES (?, ?, ?)`,
			f.PlayerID, f.FriendPlayerID, f.SinceMs,
		); err != nil {
			return fmt.Errorf("insert friend %d/%d: %w", f.PlayerID, f.FriendPlayerID, err)
		}
	}

	for _, r := range data.FriendRequests {
		if _, err := tx.ExecContext(ctx,
			`INSERT INTO friend_request (from_player_id, to_player_id, request_time_ms, status)
			 VALUES (?, ?, ?, ?)`,
			r.FromPlayerID, r.ToPlayerID, r.RequestTimeMs, r.Status,
		); err != nil {
			return fmt.Errorf("insert friend request %d→%d: %w", r.FromPlayerID, r.ToPlayerID, err)
		}
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("commit: %w", err)
	}

	logx.Infof("[ZoneSnapshot] restored zone=%d from snapshot=%d guilds=%d members=%d friends=%d requests=%d",
		snap.ZoneID, snap.ID, len(data.Guilds), len(data.GuildMembers),
		len(data.Friends), len(data.FriendRequests))

	return nil
}

// GetZoneSnapshotPlayerIDs returns the player IDs recorded in a zone snapshot.
func (s *SnapshotStore) GetZoneSnapshotPlayerIDs(ctx context.Context, snapshotID uint64) ([]uint64, error) {
	var blob []byte
	err := s.db.QueryRowContext(ctx,
		`SELECT data FROM zone_snapshot WHERE id = ?`, snapshotID,
	).Scan(&blob)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}

	var data ZoneSnapshotData
	if err := json.Unmarshal(blob, &data); err != nil {
		return nil, fmt.Errorf("unmarshal: %w", err)
	}
	return data.PlayerIDs, nil
}

// ── Transaction helpers ────────────────────────────────────────

func deleteGuildMembersInTx(ctx context.Context, tx *sql.Tx, guildIDs []uint64) error {
	if len(guildIDs) == 0 {
		return nil
	}
	query := `DELETE FROM guild_member WHERE guild_id IN (`
	args := make([]interface{}, len(guildIDs))
	for i, id := range guildIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args[i] = id
	}
	query += ")"
	_, err := tx.ExecContext(ctx, query, args...)
	return err
}

func deleteFriendsInTx(ctx context.Context, tx *sql.Tx, playerIDs []uint64) error {
	if len(playerIDs) == 0 {
		return nil
	}
	query := `DELETE FROM friend WHERE player_id IN (`
	args := make([]interface{}, len(playerIDs))
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args[i] = id
	}
	query += ")"
	_, err := tx.ExecContext(ctx, query, args...)
	return err
}

func deleteFriendRequestsInTx(ctx context.Context, tx *sql.Tx, playerIDs []uint64) error {
	if len(playerIDs) == 0 {
		return nil
	}
	// Delete requests where the player is either sender or receiver
	query := `DELETE FROM friend_request WHERE from_player_id IN (`
	args := make([]interface{}, 0, len(playerIDs)*2)
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args = append(args, id)
	}
	query += ") OR to_player_id IN ("
	for i, id := range playerIDs {
		if i > 0 {
			query += ","
		}
		query += "?"
		args = append(args, id)
	}
	query += ")"
	_, err := tx.ExecContext(ctx, query, args...)
	return err
}
