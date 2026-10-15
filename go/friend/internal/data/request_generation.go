package data

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
)

// RequestChange 报告本次提交的可见变化。重放成功没有变化，不能再次推送或抬摘要。
type RequestChange struct {
	RequestID    uint64
	FromPlayerID uint64
	ToPlayerID   uint64
	Changed      bool
}

var errRequestIDRequired = errors.New("好友申请需要新代次")

// runRequestWrite 先在守卫内确定是否需要新号；需要时回滚纯判定，事务外领号，再完整复核。
// 号段续领可能调用 data_service，不能持双方容量锁等待网络。已存在非零 pending 重放不领号。
func (r *FriendRepo) runRequestWrite(ctx context.Context, from, to uint64,
	body func(context.Context, *sql.Tx, map[uint64]uint32, uint64) error) error {
	var candidate uint64
	for attempt := 0; attempt < 2; attempt++ {
		err := r.runGuardedWrite(ctx, from, to, func(ctx context.Context, tx *sql.Tx, counts map[uint64]uint32) error {
			return body(ctx, tx, counts, candidate)
		})
		if !errors.Is(err, errRequestIDRequired) {
			return err
		}
		if r.nextRequestID == nil {
			return errors.New("好友申请号段未装配")
		}
		candidate, err = r.nextRequestID(ctx)
		if err != nil {
			return fmt.Errorf("领取好友申请代次失败: %w", err)
		}
		if candidate == 0 {
			return errors.New("好友申请号源返回零代次")
		}
	}
	return errors.New("已领取好友申请代次但事务仍要求领号")
}

// findRequest 只用索引定位完整主键，不提前取锁。普通索引允许历史0共存；非零重复
// 是号源或数据损坏，不能任选一行继续。请求体里的 from_player_id 不参与定位或授权。
func (r *FriendRepo) findRequest(ctx context.Context, requestID, accepterID uint64) (RequestChange, error) {
	if requestID == 0 || accepterID == 0 {
		return RequestChange{}, ErrRequestNotFound
	}
	rows, err := r.db.QueryContext(ctx,
		"SELECT from_player_id, to_player_id FROM friend_request WHERE request_id=? LIMIT 2", requestID)
	if err != nil {
		return RequestChange{}, err
	}
	defer rows.Close()
	change := RequestChange{RequestID: requestID}
	found := false
	for rows.Next() {
		if found {
			return RequestChange{}, fmt.Errorf("好友申请代次 %d 对应多条记录，拒绝处理", requestID)
		}
		if err := rows.Scan(&change.FromPlayerID, &change.ToPlayerID); err != nil {
			return RequestChange{}, err
		}
		found = true
	}
	if err := rows.Err(); err != nil {
		return RequestChange{}, err
	}
	if !found || change.ToPlayerID != accepterID || change.FromPlayerID == 0 || change.FromPlayerID == accepterID {
		return RequestChange{}, ErrRequestNotFound
	}
	return change, nil
}

func backfillPendingRequestID(ctx context.Context, tx *sql.Tx, from, to, requestID uint64) error {
	result, err := tx.ExecContext(ctx,
		"UPDATE friend_request SET request_id=? WHERE from_player_id=? AND to_player_id=? AND status=? AND request_id=0",
		requestID, from, to, requestStatusPending)
	if err != nil {
		return err
	}
	n, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if n != 1 {
		return errors.New("好友申请历史代次回填未命中锁定记录")
	}
	return nil
}

// ensurePendingRequest 只回填仍待处理的0代次，不改时间、不轮换已发出的非零代次。
// 与重发、接受、拒绝、拉黑使用同一容量守卫；预读后状态变化以锁内完整记录为准。
func (r *FriendRepo) ensurePendingRequest(ctx context.Context, from, to uint64) (FriendRequestEntry, bool, error) {
	var entry FriendRequestEntry
	var pending bool
	err := r.runRequestWrite(ctx, from, to, func(ctx context.Context, tx *sql.Tx, _ map[uint64]uint32, candidate uint64) error {
		entry = FriendRequestEntry{FromPlayerID: from, ToPlayerID: to}
		pending = false
		err := tx.QueryRowContext(ctx,
			"SELECT request_time_ms, status, request_id FROM friend_request WHERE from_player_id=? AND to_player_id=? FOR UPDATE",
			from, to).Scan(&entry.RequestTimeMs, &entry.Status, &entry.RequestID)
		if errors.Is(err, sql.ErrNoRows) {
			return nil
		}
		if err != nil {
			return err
		}
		if entry.Status != requestStatusPending {
			return nil
		}
		if entry.RequestID == 0 {
			if candidate == 0 {
				return errRequestIDRequired
			}
			if err := backfillPendingRequestID(ctx, tx, from, to, candidate); err != nil {
				return err
			}
			entry.RequestID = candidate
		}
		pending = true
		return nil
	})
	return entry, pending, err
}

// CheckRequestIDLookupIndex 只读校验迁移结果。schemamigrate 对旧表缺普通索引仅给 warning，
// 但申请号定位若全表扫描就没有全服容量上界，因此服务接流前必须拒绝未补索引的旧库。
func CheckRequestIDLookupIndex(ctx context.Context, db *sql.DB) error {
	var indexes int
	if err := db.QueryRowContext(ctx, `SELECT COUNT(*) FROM information_schema.statistics
		WHERE table_schema=DATABASE() AND table_name='friend_request' AND column_name='request_id' AND seq_in_index=1`).Scan(&indexes); err != nil {
		return fmt.Errorf("检查好友申请代次索引失败: %w", err)
	}
	if indexes == 0 {
		return errors.New("friend_request 缺 request_id 首列普通索引；先按 friend -migrate 输出的 schema 计划补齐索引，再启动接流")
	}
	return nil
}
