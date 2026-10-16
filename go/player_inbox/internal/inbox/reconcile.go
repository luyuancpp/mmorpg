package inbox

import (
	"context"
	"fmt"

	"player_inbox/internal/constants"
	"player_inbox/internal/metrics"

	componentpb "proto/common/component"
	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx"
)

// 对账(设计 §9.3):收件箱的终结记录 vs 玩家**已落盘**的消费账本(player_database.inbox_ledger)。
//
// 账本经 data_service.GetPlayerInboxLedger 读(它读 scene 存盘写的那份 blob,不回退读 MySQL,
// 语义同 GetPlayerAssetOpLedger)。读不到存档(found=false)计为 unknown,不告警;读失败计 error。

// LedgerReader 读一名玩家已落盘的收件箱账本。found=false = zone Redis 里没有这名玩家的 blob;
// found=true 时账本非 nil(玩家从未消费过收件箱时是空消息)。
type LedgerReader interface {
	ReadInboxLedger(ctx context.Context, playerID uint64) (ledger *componentpb.PlayerInboxLedgerComp, found bool, err error)
}

// DecidedThrough 返回账本里某个域的水位;账本里没有该域时为 0。按 domain 字段查找,不靠下标(AGENTS §11.6)。
func DecidedThrough(ledger *componentpb.PlayerInboxLedgerComp, domain inboxpb.PlayerInboxDomain) uint64 {
	for _, c := range ledger.GetCursors() {
		if c.GetDomain() == uint32(domain) {
			return c.GetDecidedThroughSeq()
		}
	}
	return 0
}

// ReconcilePlayer 是运维手工对账入口:按单个玩家比对收件箱与已落盘账本,报告两类差异
// (FINAL_NOT_IN_LEDGER / PENDING_BUT_DECIDED,见 reconcileDiff)。只读,不改任何状态。
func (s *Store) ReconcilePlayer(ctx context.Context, req *inboxpb.ReconcilePlayerInboxRequest, ledgers LedgerReader) (*inboxpb.ReconcilePlayerInboxResponse, error) {
	resp := &inboxpb.ReconcilePlayerInboxResponse{}
	domain := req.GetDomain()
	if req.GetPlayerId() == 0 {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	if domain != inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_UNSPECIFIED {
		if _, ok := s.registry.Domain(domain); !ok {
			resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
			return resp, nil
		}
	}
	if ledgers == nil {
		return nil, fmt.Errorf("inbox: ReconcilePlayer has no ledger reader")
	}
	readCtx, cancel := context.WithTimeout(ctx, constants.LedgerReadTimeout)
	ledger, found, err := ledgers.ReadInboxLedger(readCtx, req.GetPlayerId())
	cancel()
	if err != nil {
		return nil, fmt.Errorf("inbox: read persisted ledger player=%d: %w", req.GetPlayerId(), err)
	}
	resp.LedgerFound = found
	if !found {
		return resp, nil // unknown:读不到存档,不比对
	}

	rows, truncated, err := s.playerItems(ctx, req.GetPlayerId(), domain)
	if err != nil {
		return nil, err
	}
	if truncated {
		logx.WithContext(ctx).Errorf("[player_inbox] ReconcilePlayer read only the first %d rows of player=%d; result is partial",
			playerItemsLimit, req.GetPlayerId())
	}

	shard := RowShard(req.GetPlayerId())
	domains := []inboxpb.PlayerInboxDomain{domain}
	if domain == inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_UNSPECIFIED {
		domains = s.registry.Domains()
	}
	for _, d := range domains {
		st, err := readStream(ctx, s.db, shard, req.GetPlayerId(), d)
		if err != nil {
			return nil, err
		}
		dt := DecidedThrough(ledger, d)
		if st == nil && dt == 0 {
			continue
		}
		resp.Cursors = append(resp.Cursors, &inboxpb.PlayerInboxReconcileCursor{
			Domain: d, LedgerDecidedThrough: dt, InboxNextSeq: st.GetNextSeq(),
		})
	}
	for _, row := range rows {
		d := inboxpb.PlayerInboxDomain(row.GetDomain())
		kind := reconcileDiff(row, DecidedThrough(ledger, d))
		if kind == inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_DIFF_UNSPECIFIED {
			continue
		}
		resp.Diffs = append(resp.Diffs, &inboxpb.PlayerInboxReconcileDiff{
			Domain:    d,
			Seq:       row.GetSeq(),
			Kind:      kind,
			State:     inboxpb.PlayerInboxItemState(row.GetState()),
			RequestId: row.GetRequestId(),
		})
	}
	return resp, nil
}

// reconcilePerShardLimit / reconcileMaxPlayers 限定定时对账一轮的工作量(GM 域量很小,设计 §9.3)。
const (
	reconcilePerShardLimit = 500
	reconcileMaxPlayers    = 2000
)

// ReconcileRecent 是定时对账作业的一轮(只在领导者上跑):取 sinceMs 之后由消费方终结的行,
// 按玩家读已落盘账本,检查 decided_through ≥ seq。不满足记 player_inbox_reconcile_mismatch_total
// 并写一行带 player_id 的 ERROR(player_id 只进日志)。返回本轮的不一致条数。
func (s *Store) ReconcileRecent(ctx context.Context, sinceMs uint64, ledgers LedgerReader) (int, error) {
	if ledgers == nil {
		return 0, fmt.Errorf("inbox: ReconcileRecent has no ledger reader")
	}
	byPlayer := map[uint64][]FinalizedRef{}
	for shard := uint32(0); shard < ShardCount && len(byPlayer) < reconcileMaxPlayers; shard++ {
		refs, err := s.RecentlyFinalized(ctx, shard, sinceMs, reconcilePerShardLimit)
		if err != nil {
			return 0, err
		}
		for _, f := range refs {
			byPlayer[f.PlayerID] = append(byPlayer[f.PlayerID], f)
		}
	}
	log := logx.WithContext(ctx)
	mismatches := 0
	for playerID, refs := range byPlayer {
		if err := ctx.Err(); err != nil {
			return mismatches, err
		}
		readCtx, cancel := context.WithTimeout(ctx, constants.LedgerReadTimeout)
		ledger, found, err := ledgers.ReadInboxLedger(readCtx, playerID)
		cancel()
		switch {
		case err != nil:
			metrics.ObserveReconcileRead("error")
			log.Errorf("[player_inbox] Reconcile could not read persisted ledger player=%d: %v", playerID, err)
			continue
		case !found:
			metrics.ObserveReconcileRead("absent")
			continue
		}
		metrics.ObserveReconcileRead("found")
		for _, f := range refs {
			dt := DecidedThrough(ledger, f.Domain)
			if dt >= f.Seq {
				continue
			}
			mismatches++
			metrics.IncReconcileMismatch()
			log.Errorf("[player_inbox] Reconcile mismatch player=%d domain=%d seq=%d state=%d request_id=%d ledger_decided_through=%d: "+
				"inbox finalized the item but the persisted ledger has not reached it (rollback or Redis data loss); "+
				"manual re-issue may be needed", playerID, f.Domain, f.Seq, f.State, f.RequestID, dt)
		}
	}
	return mismatches, nil
}
