package inbox

import (
	"context"
	"fmt"

	"player_inbox/internal/metrics"

	inboxpb "proto/player_inbox"
)

// 后台循环用到的存储操作(设计 §6.3、§9.1、§9.3、§9.4)。全部在事务之外逐条 autocommit,
// 不持流行锁:清理只删终态行(Ack 不改终态行,两者不会反向抢锁);提醒清零是 next_seq CAS。

// SweepTerminal 删除保留期外的终态行,本轮最多 budget 行(设计 §9.1:每轮最多 500 行)。
// 按分片走 (shard, state, updated_ms) 索引普通读出候选,再按主键逐行带条件删除;
// **永不删除** PENDING 行、流行与审计行。返回本轮删除的行数。
func (s *Store) SweepTerminal(ctx context.Context, cutoffMs uint64, budget int) (int, error) {
	deleted := 0
	for shard := uint32(0); shard < ShardCount && deleted < budget; shard++ {
		if err := ctx.Err(); err != nil {
			return deleted, err
		}
		rows, err := s.db.QueryContext(ctx, "SELECT `player_id`, `domain`, `seq` FROM "+itemTable+
			" WHERE `shard` = ? AND `state` IN "+terminalStatesSQL+" AND `updated_ms` < ? LIMIT ?",
			shard, cutoffMs, budget-deleted)
		if err != nil {
			return deleted, fmt.Errorf("inbox: sweep scan shard=%d: %w", shard, err)
		}
		type key struct {
			playerID uint64
			domain   uint32
			seq      uint64
		}
		var keys []key
		for rows.Next() {
			var k key
			if err := rows.Scan(&k.playerID, &k.domain, &k.seq); err != nil {
				rows.Close()
				return deleted, err
			}
			keys = append(keys, k)
		}
		if err := rows.Close(); err != nil {
			return deleted, err
		}
		for _, k := range keys {
			res, err := s.db.ExecContext(ctx, "DELETE FROM "+itemTable+" WHERE "+itemPK+
				" AND `state` IN "+terminalStatesSQL+" AND `updated_ms` < ?",
				shard, k.playerID, k.domain, k.seq, cutoffMs)
			if err != nil {
				return deleted, fmt.Errorf("inbox: sweep delete player=%d seq=%d: %w", k.playerID, k.seq, err)
			}
			if n, err := res.RowsAffected(); err == nil && n == 1 {
				deleted++
			}
		}
	}
	metrics.AddSweepDeleted(deleted)
	return deleted, nil
}

// NotifyDue 是一条待补发的提醒:读出当时的 next_seq 作 CAS 令牌。
type NotifyDue struct {
	PlayerID uint64
	Domain   inboxpb.PlayerInboxDomain
	NextSeq  uint64
}

// DueNotifications 读一个分片里到期的提醒:notify_due_ms ∈ [1, dueBeforeMs],最多 limit 条
// (走 (shard, notify_due_ms) 索引,设计 §6.3)。
func (s *Store) DueNotifications(ctx context.Context, shard uint32, dueBeforeMs uint64, limit int) ([]NotifyDue, error) {
	rows, err := s.db.QueryContext(ctx, "SELECT `player_id`, `domain`, `next_seq` FROM "+streamTable+
		" WHERE `shard` = ? AND `notify_due_ms` BETWEEN 1 AND ? LIMIT ?", shard, dueBeforeMs, limit)
	if err != nil {
		return nil, fmt.Errorf("inbox: scan due notifications shard=%d: %w", shard, err)
	}
	defer rows.Close()
	var out []NotifyDue
	for rows.Next() {
		var d NotifyDue
		var domain uint32
		if err := rows.Scan(&d.PlayerID, &domain, &d.NextSeq); err != nil {
			return nil, err
		}
		if RowShard(d.PlayerID) != shard {
			return nil, fmt.Errorf("inbox: corrupt stream row shard=%d player_id=%d", shard, d.PlayerID)
		}
		d.Domain = inboxpb.PlayerInboxDomain(domain)
		out = append(out, d)
	}
	return out, rows.Err()
}

// ResendNotify 给一条到期提醒补发一次;落定(送达 / 离线 / 等待落点)后按读出时的 next_seq 做 CAS 置 0,
// 失败就保持原值,下一轮再试(设计 §6.3)。
func (s *Store) ResendNotify(ctx context.Context, d NotifyDue) NotifyResult {
	return s.notifyAndClear(ctx, d.PlayerID, d.Domain, d.NextSeq)
}

// ScanGauges 扫一遍全部分片,算出扫描型 gauge(设计 §9.4,只由领导者调用)。
// notifyOverdueBeforeMs:notify_due_ms 早于它的流计入 notify_backlog(提醒积压超过 5 分钟)。
func (s *Store) ScanGauges(ctx context.Context, nowMs, notifyOverdueBeforeMs uint64) (metrics.ScanGauges, error) {
	var g metrics.ScanGauges
	var oldestCreated, oldestDelivered uint64
	for shard := uint32(0); shard < ShardCount; shard++ {
		if err := ctx.Err(); err != nil {
			return g, err
		}
		var open, delivered, maxDelivery, minCreated, minDelivered uint64
		err := s.db.QueryRowContext(ctx, "SELECT COUNT(*),"+
			" COALESCE(SUM(CASE WHEN `delivery_count` > 0 THEN 1 ELSE 0 END), 0),"+
			" COALESCE(MAX(`delivery_count`), 0), COALESCE(MIN(`created_ms`), 0),"+
			" COALESCE(MIN(CASE WHEN `delivery_count` > 0 THEN `last_delivered_ms` END), 0)"+
			" FROM "+itemTable+" WHERE `shard` = ? AND `state` = ?",
			shard, uint32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING)).
			Scan(&open, &delivered, &maxDelivery, &minCreated, &minDelivered)
		if err != nil {
			return g, fmt.Errorf("inbox: gauge scan shard=%d: %w", shard, err)
		}
		g.OpenItems += open
		g.DeliveredUnacked += delivered
		g.MaxDeliveryCount = max(g.MaxDeliveryCount, maxDelivery)
		if minCreated != 0 && (oldestCreated == 0 || minCreated < oldestCreated) {
			oldestCreated = minCreated
		}
		if minDelivered != 0 && (oldestDelivered == 0 || minDelivered < oldestDelivered) {
			oldestDelivered = minDelivered
		}

		var backlog uint64
		if err := s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM "+streamTable+
			" WHERE `shard` = ? AND `notify_due_ms` BETWEEN 1 AND ?", shard, notifyOverdueBeforeMs).Scan(&backlog); err != nil {
			return g, fmt.Errorf("inbox: notify backlog scan shard=%d: %w", shard, err)
		}
		g.NotifyBacklog += backlog
	}
	g.OldestOpenAgeSeconds = ageSeconds(nowMs, oldestCreated)
	// 已投递未确认的「最老」按最后一次投递时刻算:被反复重投的行会刷新它,崩溃循环由 max_delivery_count 另行告警;
	// 消费方 halt 之后不再拉取,它就一直变老 —— 这正是要抓的「卡住」信号(设计 §9.4)。
	g.DeliveredUnackedOldestSeconds = ageSeconds(nowMs, oldestDelivered)
	return g, nil
}

func ageSeconds(nowMs, sinceMs uint64) float64 {
	if sinceMs == 0 || sinceMs >= nowMs {
		return 0
	}
	return float64(nowMs-sinceMs) / 1000
}

// FinalizedRef 是对账作业要核对的一条「由消费方终结」的行。
type FinalizedRef struct {
	PlayerID  uint64
	Domain    inboxpb.PlayerInboxDomain
	Seq       uint64
	State     inboxpb.PlayerInboxItemState
	RequestID uint64
}

// RecentlyFinalized 读一个分片里 updated_ms ≥ sinceMs、由消费方终结的终态行,最多 limit 条
// (走 (shard, state, updated_ms) 索引,设计 §9.3)。
func (s *Store) RecentlyFinalized(ctx context.Context, shard uint32, sinceMs uint64, limit int) ([]FinalizedRef, error) {
	rows, err := s.db.QueryContext(ctx, "SELECT `player_id`, `domain`, `seq`, `state`, `request_id` FROM "+itemTable+
		" WHERE `shard` = ? AND `state` IN "+terminalStatesSQL+" AND `updated_ms` >= ? AND `finalized_by` = ? LIMIT ?",
		shard, sinceMs, uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_CONSUMER), limit)
	if err != nil {
		return nil, fmt.Errorf("inbox: scan finalized shard=%d: %w", shard, err)
	}
	defer rows.Close()
	var out []FinalizedRef
	for rows.Next() {
		var f FinalizedRef
		var domain, state uint32
		if err := rows.Scan(&f.PlayerID, &domain, &f.Seq, &state, &f.RequestID); err != nil {
			return nil, err
		}
		f.Domain, f.State = inboxpb.PlayerInboxDomain(domain), inboxpb.PlayerInboxItemState(state)
		out = append(out, f)
	}
	return out, rows.Err()
}

// playerItemsLimit 是 ReconcilePlayer 单个玩家读出的行数上限:单条流在保留期内不超过数百行(设计 §4.2 量级假设)。
const playerItemsLimit = 5000

// playerItems 读一名玩家(可选一个域)的全部条目行,按 (domain, seq) 升序;超过上限时 truncated=true。
func (s *Store) playerItems(ctx context.Context, playerID uint64, domain inboxpb.PlayerInboxDomain) (rows []*inboxpb.PlayerInboxItemRow, truncated bool, err error) {
	query := "SELECT " + itemColumns + " FROM " + itemTable + " WHERE `shard` = ? AND `player_id` = ?"
	args := []any{RowShard(playerID), playerID}
	if domain != inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_UNSPECIFIED {
		query += " AND `domain` = ?"
		args = append(args, uint32(domain))
	}
	query += " ORDER BY `domain` ASC, `seq` ASC LIMIT ?"
	args = append(args, playerItemsLimit+1)
	rows, err = queryItems(ctx, s.db, query, args...)
	if err != nil {
		return nil, false, fmt.Errorf("inbox: read player items player=%d: %w", playerID, err)
	}
	if len(rows) > playerItemsLimit {
		return rows[:playerItemsLimit], true, nil
	}
	return rows, false, nil
}
