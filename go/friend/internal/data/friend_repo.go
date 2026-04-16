package data

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"shared/cache"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

var (
	ErrSenderFriendsFull   = errors.New("sender friend list full")
	ErrAcceptorFriendsFull = errors.New("acceptor friend list full")
)

type FriendEntry struct {
	FriendPlayerID uint64 `json:"friend_player_id"`
	SinceMs        int64  `json:"since_ms"`
	LastActiveMs   int64  `json:"last_active_ms"`
}

type FriendRequestEntry struct {
	FromPlayerID  uint64 `json:"from_player_id"`
	ToPlayerID    uint64 `json:"to_player_id"`
	RequestTimeMs int64  `json:"request_time_ms"`
	Status        int32  `json:"status"` // 1=pending, 2=accepted, 3=rejected
}

// FriendRepo provides cache-aside access to friend data.
type FriendRepo struct {
	rdb        *redis.Client
	db         *sql.DB
	defaultTTL time.Duration
}

func NewFriendRepo(rdb *redis.Client, db *sql.DB, defaultTTL time.Duration) *FriendRepo {
	return &FriendRepo{
		rdb:        rdb,
		db:         db,
		defaultTTL: defaultTTL,
	}
}

// ── Redis key helpers ──────────────────────────────────────────

func friendListKey(playerID uint64) string {
	return fmt.Sprintf("friends:%d", playerID)
}

func pendingRequestsKey(playerID uint64) string {
	return fmt.Sprintf("friend_req:%d", playerID)
}

func onlineKey(playerID uint64) string {
	return fmt.Sprintf("friend:online:%d", playerID)
}

const onlineTTL = 60 * time.Second

// ── Read (cache-aside + singleflight) ──────────────────────────

func (r *FriendRepo) GetFriendList(ctx context.Context, playerID uint64) ([]FriendEntry, error) {
	return cache.LoadOrCache[[]FriendEntry](
		ctx, r.rdb,
		friendListKey(playerID),
		fmt.Sprintf("fl:%d", playerID),
		r.defaultTTL,
		func(ctx context.Context) ([]FriendEntry, error) {
			return r.loadFriendListFromMySQL(ctx, playerID)
		},
	)
}

func (r *FriendRepo) GetPendingRequests(ctx context.Context, playerID uint64) ([]FriendRequestEntry, error) {
	return cache.LoadOrCache[[]FriendRequestEntry](
		ctx, r.rdb,
		pendingRequestsKey(playerID),
		fmt.Sprintf("pr:%d", playerID),
		r.defaultTTL,
		func(ctx context.Context) ([]FriendRequestEntry, error) {
			return r.loadPendingRequestsFromMySQL(ctx, playerID)
		},
	)
}

// ── Write operations ───────────────────────────────────────────

func (r *FriendRepo) AddFriendRequest(ctx context.Context, fromPlayerID, toPlayerID uint64) error {
	now := time.Now().UnixMilli()
	_, err := r.db.ExecContext(ctx,
		`INSERT INTO friend_request (from_player_id, to_player_id, request_time_ms, status)
		 VALUES (?, ?, ?, 1)
		 ON DUPLICATE KEY UPDATE status=1, request_time_ms=VALUES(request_time_ms)`,
		fromPlayerID, toPlayerID, now)
	if err != nil {
		return err
	}
	// Invalidate target's pending cache
	r.rdb.Del(ctx, pendingRequestsKey(toPlayerID))
	return nil
}

func (r *FriendRepo) AcceptFriend(ctx context.Context, fromPlayerID, toPlayerID uint64, maxFriends uint32) error {
	tx, err := r.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	// Atomic check: sender's friend count (FOR UPDATE prevents concurrent accept exceeding limit)
	var fromCount int
	if err := tx.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM friend WHERE player_id = ? FOR UPDATE", fromPlayerID).Scan(&fromCount); err != nil {
		return err
	}
	if uint32(fromCount) >= maxFriends {
		return ErrSenderFriendsFull
	}

	// Atomic check: acceptor's friend count
	var toCount int
	if err := tx.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM friend WHERE player_id = ? FOR UPDATE", toPlayerID).Scan(&toCount); err != nil {
		return err
	}
	if uint32(toCount) >= maxFriends {
		return ErrAcceptorFriendsFull
	}

	// Update request status
	if _, err := tx.ExecContext(ctx,
		"UPDATE friend_request SET status=2 WHERE from_player_id=? AND to_player_id=? AND status=1",
		fromPlayerID, toPlayerID); err != nil {
		return err
	}

	// Insert bidirectional friendship
	now := time.Now().UnixMilli()
	if _, err := tx.ExecContext(ctx,
		"INSERT IGNORE INTO friend (player_id, friend_player_id, since_ms) VALUES (?, ?, ?), (?, ?, ?)",
		fromPlayerID, toPlayerID, now, toPlayerID, fromPlayerID, now); err != nil {
		return err
	}

	if err := tx.Commit(); err != nil {
		return err
	}

	// Invalidate caches for both players
	r.rdb.Del(ctx, friendListKey(fromPlayerID), friendListKey(toPlayerID), pendingRequestsKey(toPlayerID))
	return nil
}

func (r *FriendRepo) RejectFriend(ctx context.Context, fromPlayerID, toPlayerID uint64) error {
	_, err := r.db.ExecContext(ctx,
		"UPDATE friend_request SET status=3 WHERE from_player_id=? AND to_player_id=? AND status=1",
		fromPlayerID, toPlayerID)
	if err != nil {
		return err
	}
	r.rdb.Del(ctx, pendingRequestsKey(toPlayerID))
	return nil
}

func (r *FriendRepo) RemoveFriend(ctx context.Context, playerID, targetPlayerID uint64) error {
	_, err := r.db.ExecContext(ctx,
		"DELETE FROM friend WHERE (player_id=? AND friend_player_id=?) OR (player_id=? AND friend_player_id=?)",
		playerID, targetPlayerID, targetPlayerID, playerID)
	if err != nil {
		return err
	}
	r.rdb.Del(ctx, friendListKey(playerID), friendListKey(targetPlayerID))
	return nil
}

// AreFriends checks if two players are friends.
func (r *FriendRepo) AreFriends(ctx context.Context, playerID, targetID uint64) (bool, error) {
	friends, err := r.GetFriendList(ctx, playerID)
	if err != nil {
		return false, err
	}
	for _, f := range friends {
		if f.FriendPlayerID == targetID {
			return true, nil
		}
	}
	return false, nil
}

// HasPendingRequest checks if there's already a pending request between two players.
func (r *FriendRepo) HasPendingRequest(ctx context.Context, fromID, toID uint64) (bool, error) {
	var count int
	err := r.db.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM friend_request WHERE from_player_id=? AND to_player_id=? AND status=1",
		fromID, toID).Scan(&count)
	if err != nil {
		return false, err
	}
	return count > 0, nil
}

// ── Online status (Redis key with TTL) ─────────────────────────

func (r *FriendRepo) SetPlayerOnline(ctx context.Context, playerID uint64, gateNodeID uint32) error {
	return r.rdb.Set(ctx, onlineKey(playerID), gateNodeID, onlineTTL).Err()
}

func (r *FriendRepo) SetPlayerOffline(ctx context.Context, playerID uint64) error {
	return r.rdb.Del(ctx, onlineKey(playerID)).Err()
}

func (r *FriendRepo) BatchGetOnlineStatus(ctx context.Context, playerIDs []uint64) (map[uint64]bool, error) {
	if len(playerIDs) == 0 {
		return map[uint64]bool{}, nil
	}
	pipe := r.rdb.Pipeline()
	cmds := make(map[uint64]*redis.IntCmd, len(playerIDs))
	for _, id := range playerIDs {
		cmds[id] = pipe.Exists(ctx, onlineKey(id))
	}
	if _, err := pipe.Exec(ctx); err != nil && err != redis.Nil {
		return nil, fmt.Errorf("batch online status: %w", err)
	}
	result := make(map[uint64]bool, len(playerIDs))
	for id, cmd := range cmds {
		result[id] = cmd.Val() > 0
	}
	return result, nil
}

// ── Cache helpers ──────────────────────────────────────────────

func (r *FriendRepo) cacheFriendList(ctx context.Context, playerID uint64, friends []FriendEntry) {
	data, err := json.Marshal(friends)
	if err != nil {
		logx.Errorf("marshal friend list %d: %v", playerID, err)
		return
	}
	r.rdb.Set(ctx, friendListKey(playerID), data, r.defaultTTL)
}

func (r *FriendRepo) cachePendingRequests(ctx context.Context, playerID uint64, requests []FriendRequestEntry) {
	data, err := json.Marshal(requests)
	if err != nil {
		logx.Errorf("marshal pending requests %d: %v", playerID, err)
		return
	}
	r.rdb.Set(ctx, pendingRequestsKey(playerID), data, r.defaultTTL)
}

// ── MySQL queries ──────────────────────────────────────────────

func (r *FriendRepo) loadFriendListFromMySQL(ctx context.Context, playerID uint64) ([]FriendEntry, error) {
	rows, err := r.db.QueryContext(ctx,
		"SELECT friend_player_id, since_ms FROM friend WHERE player_id = ?", playerID)
	if err != nil {
		return nil, fmt.Errorf("query friends %d: %w", playerID, err)
	}
	defer rows.Close()

	var friends []FriendEntry
	for rows.Next() {
		var f FriendEntry
		if err := rows.Scan(&f.FriendPlayerID, &f.SinceMs); err != nil {
			return nil, fmt.Errorf("scan friend: %w", err)
		}
		friends = append(friends, f)
	}
	if friends == nil {
		friends = []FriendEntry{}
	}
	return friends, nil
}

func (r *FriendRepo) loadPendingRequestsFromMySQL(ctx context.Context, playerID uint64) ([]FriendRequestEntry, error) {
	rows, err := r.db.QueryContext(ctx,
		"SELECT from_player_id, to_player_id, request_time_ms, status FROM friend_request WHERE to_player_id = ? AND status = 1",
		playerID)
	if err != nil {
		return nil, fmt.Errorf("query pending requests %d: %w", playerID, err)
	}
	defer rows.Close()

	var requests []FriendRequestEntry
	for rows.Next() {
		var req FriendRequestEntry
		if err := rows.Scan(&req.FromPlayerID, &req.ToPlayerID, &req.RequestTimeMs, &req.Status); err != nil {
			return nil, fmt.Errorf("scan friend request: %w", err)
		}
		requests = append(requests, req)
	}
	if requests == nil {
		requests = []FriendRequestEntry{}
	}
	return requests, nil
}
