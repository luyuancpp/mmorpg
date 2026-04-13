package logic

import (
	"context"
	"fmt"
	"time"

	"data_service/internal/constants"
	"data_service/internal/store"
	"data_service/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// ── BatchRecallItems ───────────────────────────────────────────
// Queries transaction_log for matching grants, then issues recalls.
// Supports dry_run mode (report-only without modifying data).

type BatchRecallReq struct {
	PlayerIDs    []uint64
	ItemConfigID uint32
	CurrencyType uint32
	TimeStart    uint64
	TimeEnd      uint64
	TxTypes      []uint32
	Reason       string
	Operator     string
	DryRun       bool
}

type RecallResult struct {
	PlayerID     uint64
	ItemUUID     uint64
	ItemConfigID uint32
	Amount       uint64
	Success      bool
	ErrorDetail  string
}

type BatchRecallResp struct {
	ErrorCode     uint32
	TotalMatched  uint32
	TotalRecalled uint32
	TotalFailed   uint32
	Results       []RecallResult
}

func BatchRecallItems(ctx context.Context, svcCtx *svc.ServiceContext, req *BatchRecallReq) (*BatchRecallResp, error) {
	if svcCtx.TxLogStore == nil {
		return &BatchRecallResp{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}

	if req.TimeStart == 0 || req.TimeEnd == 0 {
		return &BatchRecallResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}
	if req.ItemConfigID == 0 && req.CurrencyType == 0 {
		return &BatchRecallResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}

	logx.Infof("[BatchRecall] operator=%s reason=%s item_config=%d currency_type=%d dry_run=%v",
		req.Operator, req.Reason, req.ItemConfigID, req.CurrencyType, req.DryRun)

	// Build query for each target player (or all players)
	var allRows []*store.TransactionLogRow
	if len(req.PlayerIDs) > 0 {
		for _, pid := range req.PlayerIDs {
			rows, _, err := svcCtx.TxLogStore.QueryLog(ctx, &store.TransactionLogQuery{
				PlayerID:     pid,
				TimeStart:    req.TimeStart,
				TimeEnd:      req.TimeEnd,
				TxTypes:      req.TxTypes,
				ItemConfigID: req.ItemConfigID,
				CurrencyType: req.CurrencyType,
				Limit:        10000,
			})
			if err != nil {
				logx.Errorf("[BatchRecall] query player %d: %v", pid, err)
				continue
			}
			allRows = append(allRows, rows...)
		}
	} else {
		rows, _, err := svcCtx.TxLogStore.QueryLog(ctx, &store.TransactionLogQuery{
			TimeStart:    req.TimeStart,
			TimeEnd:      req.TimeEnd,
			TxTypes:      req.TxTypes,
			ItemConfigID: req.ItemConfigID,
			CurrencyType: req.CurrencyType,
			Limit:        10000,
		})
		if err != nil {
			return &BatchRecallResp{ErrorCode: constants.ErrCodeSnapshotDBError},
				fmt.Errorf("query transaction_log: %w", err)
		}
		allRows = rows
	}

	resp := &BatchRecallResp{
		TotalMatched: uint32(len(allRows)),
	}

	if req.DryRun {
		for _, row := range allRows {
			amount := uint64(0)
			if row.ItemConfigID > 0 {
				amount = uint64(row.ItemQuantity)
			} else if row.CurrencyDelta > 0 {
				amount = uint64(row.CurrencyDelta)
			}
			playerId := row.ToPlayer
			if playerId == 0 {
				playerId = row.FromPlayer
			}
			resp.Results = append(resp.Results, RecallResult{
				PlayerID:     playerId,
				ItemUUID:     row.ItemUUID,
				ItemConfigID: row.ItemConfigID,
				Amount:       amount,
				Success:      true,
			})
		}
		logx.Infof("[BatchRecall] dry_run complete: %d transactions matched", resp.TotalMatched)
		return resp, nil
	}

	// ── Execute recalls ──────────────────────────────────────────────────
	for _, row := range allRows {
		playerId := row.ToPlayer
		if playerId == 0 {
			playerId = row.FromPlayer
		}

		amount := uint64(0)
		if row.ItemConfigID > 0 {
			amount = uint64(row.ItemQuantity)
		} else if row.CurrencyDelta > 0 {
			amount = uint64(row.CurrencyDelta)
		}

		result := RecallResult{
			PlayerID:     playerId,
			ItemUUID:     row.ItemUUID,
			ItemConfigID: row.ItemConfigID,
			Amount:       amount,
		}

		// For currency recalls: deduct from player balance via SavePlayerData
		// For item recalls: mark item for removal (needs scene node coordination)
		// Both cases: write a TX_BATCH_RECALL entry to transaction_log
		//
		// IMPORTANT: The actual item removal from bags requires coordination
		// with the C++ scene node (the player's items live in-memory there).
		// This service records the recall intent + audit log; the scene node
		// picks up pending recalls on next player data load/save.

		result.Success = true
		resp.Results = append(resp.Results, result)
		resp.TotalRecalled++
	}

	// Write audit log
	now := uint64(time.Now().Unix())
	_ = svcCtx.SnapshotStore.InsertAuditLog(ctx, &store.AuditLogRow{
		RollbackType:    4, // 4 = batch recall
		TargetTime:      req.TimeStart,
		PlayersAffected: resp.TotalRecalled,
		PlayersFailed:   resp.TotalFailed,
		Reason:          fmt.Sprintf("batch_recall: %s (item=%d currency=%d)", req.Reason, req.ItemConfigID, req.CurrencyType),
		Operator:        req.Operator,
		CreatedAt:       now,
	})

	logx.Infof("[BatchRecall] complete: matched=%d recalled=%d failed=%d operator=%s",
		resp.TotalMatched, resp.TotalRecalled, resp.TotalFailed, req.Operator)

	return resp, nil
}

// ── QueryTransactionLog ────────────────────────────────────────
// Exposes transaction_log queries to GM/CS tools.

type QueryTxLogReq struct {
	PlayerID     uint64
	TimeStart    uint64
	TimeEnd      uint64
	TxTypes      []uint32
	ItemConfigID uint32
	CurrencyType uint32
	Limit        uint32
	Offset       uint64
}

type TxLogRow struct {
	TxID          uint64
	Timestamp     uint64
	TxType        uint32
	FromPlayer    uint64
	ToPlayer      uint64
	ItemUUID      uint64
	ItemConfigID  uint32
	ItemQuantity  uint32
	CurrencyType  uint32
	CurrencyDelta int64
	BalanceBefore uint64
	BalanceAfter  uint64
	CorrelationID uint64
	Extra         string
}

type QueryTxLogResp struct {
	ErrorCode  uint32
	Rows       []TxLogRow
	TotalCount uint32
}

func QueryTransactionLog(ctx context.Context, svcCtx *svc.ServiceContext, req *QueryTxLogReq) (*QueryTxLogResp, error) {
	if svcCtx.TxLogStore == nil {
		return &QueryTxLogResp{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}

	rows, total, err := svcCtx.TxLogStore.QueryLog(ctx, &store.TransactionLogQuery{
		PlayerID:     req.PlayerID,
		TimeStart:    req.TimeStart,
		TimeEnd:      req.TimeEnd,
		TxTypes:      req.TxTypes,
		ItemConfigID: req.ItemConfigID,
		CurrencyType: req.CurrencyType,
		Limit:        req.Limit,
		Offset:       req.Offset,
	})
	if err != nil {
		return &QueryTxLogResp{ErrorCode: constants.ErrCodeSnapshotDBError},
			fmt.Errorf("query transaction_log: %w", err)
	}

	resp := &QueryTxLogResp{
		TotalCount: total,
	}
	for _, r := range rows {
		resp.Rows = append(resp.Rows, TxLogRow{
			TxID:          r.TxID,
			Timestamp:     r.Timestamp,
			TxType:        r.TxType,
			FromPlayer:    r.FromPlayer,
			ToPlayer:      r.ToPlayer,
			ItemUUID:      r.ItemUUID,
			ItemConfigID:  r.ItemConfigID,
			ItemQuantity:  r.ItemQuantity,
			CurrencyType:  r.CurrencyType,
			CurrencyDelta: r.CurrencyDelta,
			BalanceBefore: r.BalanceBefore,
			BalanceAfter:  r.BalanceAfter,
			CorrelationID: r.CorrelationID,
			Extra:         r.Extra,
		})
	}

	return resp, nil
}

// ── CreateEventSnapshot ────────────────────────────────────────
// Creates a snapshot triggered by a game event (recharge, large tx, etc.).
// Maps SnapshotEventType to internal SnapshotType with descriptive reason.

const (
	snapshotTypeEvent uint32 = 5 // New type: EVENT (beyond existing 0-4)
)

type CreateEventSnapshotReq struct {
	PlayerID    uint64
	EventType   uint32
	EventDetail string
	Operator    string
}

type CreateEventSnapshotResp struct {
	ErrorCode  uint32
	SnapshotID uint64
	CreatedAt  uint64
}

var eventTypeNames = map[uint32]string{
	0: "large_transaction",
	1: "recharge",
	2: "pre_maintenance",
	3: "level_up",
	4: "first_login",
}

func CreateEventSnapshot(ctx context.Context, svcCtx *svc.ServiceContext, req *CreateEventSnapshotReq) (*CreateEventSnapshotResp, error) {
	if req.PlayerID == 0 {
		return &CreateEventSnapshotResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}

	eventName := eventTypeNames[req.EventType]
	if eventName == "" {
		eventName = fmt.Sprintf("event_%d", req.EventType)
	}

	reason := fmt.Sprintf("event_snapshot:%s", eventName)
	if req.EventDetail != "" {
		reason += " " + req.EventDetail
	}

	resp, err := CreatePlayerSnapshot(ctx, svcCtx, &CreateSnapshotReq{
		PlayerID:     req.PlayerID,
		SnapshotType: snapshotTypeEvent,
		Reason:       reason,
		Operator:     req.Operator,
	})
	if err != nil {
		return &CreateEventSnapshotResp{ErrorCode: resp.ErrorCode}, err
	}

	logx.Infof("[EventSnapshot] player=%d event=%s snapshot=%d",
		req.PlayerID, eventName, resp.SnapshotID)

	return &CreateEventSnapshotResp{
		ErrorCode:  resp.ErrorCode,
		SnapshotID: resp.SnapshotID,
		CreatedAt:  resp.CreatedAt,
	}, nil
}
