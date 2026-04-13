package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"data_service/internal/constants"
	"data_service/internal/store"
	"data_service/internal/svc"
	loginpb "data_service/proto/login"

	"github.com/zeromicro/go-zero/core/logx"
)

const (
	snapshotTypePreRollback uint32 = 3 // matches SnapshotType.SNAPSHOT_PRE_ROLLBACK
	rollbackTypePlayer      uint32 = 1
	rollbackTypeZone        uint32 = 2
	rollbackTypeServer      uint32 = 3
)

// ── RollbackPlayer ─────────────────────────────────────────────

type RollbackPlayerReq struct {
	PlayerID   uint64
	SnapshotID uint64
	TargetTime uint64
	Scope      uint32   // 0=full, 1=partial
	Fields     []string // only for partial
	Reason     string
	Operator   string
}

type RollbackPlayerResp struct {
	ErrorCode             uint32
	SnapshotIDUsed        uint64
	PreRollbackSnapshotID uint64
	FieldsRestored        []string
}

func RollbackPlayer(ctx context.Context, svcCtx *svc.ServiceContext, req *RollbackPlayerReq) (*RollbackPlayerResp, error) {
	if req.PlayerID == 0 {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}
	if req.SnapshotID == 0 && req.TargetTime == 0 {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}

	resp, err := rollbackSinglePlayer(ctx, svcCtx, req)
	if err != nil {
		logx.Errorf("[Rollback] player %d failed: %v", req.PlayerID, err)
		return resp, err
	}

	// Write audit log
	now := uint64(time.Now().Unix())
	_ = svcCtx.SnapshotStore.InsertAuditLog(ctx, &store.AuditLogRow{
		PlayerID:              req.PlayerID,
		RollbackType:          rollbackTypePlayer,
		SnapshotIDUsed:        resp.SnapshotIDUsed,
		PreRollbackSnapshotID: resp.PreRollbackSnapshotID,
		TargetTime:            req.TargetTime,
		PlayersAffected:       1,
		Reason:                req.Reason,
		Operator:              req.Operator,
		CreatedAt:             now,
	})

	logx.Infof("[Rollback] player %d restored from snapshot %d (pre-rollback=%d) by %s: %s",
		req.PlayerID, resp.SnapshotIDUsed, resp.PreRollbackSnapshotID, req.Operator, req.Reason)

	return resp, nil
}

func rollbackSinglePlayer(ctx context.Context, svcCtx *svc.ServiceContext, req *RollbackPlayerReq) (*RollbackPlayerResp, error) {
	// 1. Resolve target snapshot
	snap, err := resolveSnapshot(ctx, svcCtx, req.PlayerID, req.SnapshotID, req.TargetTime)
	if err != nil {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeSnapshotDBError}, err
	}
	if snap == nil {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeSnapshotNotFound}, nil
	}

	// 2. Deserialize snapshot data
	var sd snapshotData
	if err := json.Unmarshal(snap.Data, &sd); err != nil {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeSnapshotDBError},
			fmt.Errorf("unmarshal snapshot %d: %w", snap.ID, err)
	}

	// 3. Create pre-rollback safety snapshot (so we can undo the rollback if needed)
	preSnap, err := CreatePlayerSnapshot(ctx, svcCtx, &CreateSnapshotReq{
		PlayerID:     req.PlayerID,
		SnapshotType: snapshotTypePreRollback,
		Reason:       fmt.Sprintf("pre-rollback safety snapshot (target snapshot=%d)", snap.ID),
		Operator:     req.Operator,
	})
	if err != nil {
		logx.Errorf("[Rollback] failed to create pre-rollback snapshot for player %d: %v", req.PlayerID, err)
		// Non-fatal — continue with rollback
	}
	var preRollbackID uint64
	if preSnap != nil && preSnap.ErrorCode == constants.ErrCodeOK {
		preRollbackID = preSnap.SnapshotID
	}

	// 4. Determine which fields to restore
	fieldsToRestore := sd.Fields
	if req.Scope == 1 && len(req.Fields) > 0 {
		// Partial rollback — only restore requested fields
		filtered := make(map[string][]byte, len(req.Fields))
		for _, f := range req.Fields {
			if val, ok := sd.Fields[f]; ok {
				filtered[f] = val
			}
		}
		fieldsToRestore = filtered
	}

	if len(fieldsToRestore) == 0 {
		return &RollbackPlayerResp{
			ErrorCode:             constants.ErrCodeSnapshotNotFound,
			SnapshotIDUsed:        snap.ID,
			PreRollbackSnapshotID: preRollbackID,
		}, nil
	}

	// 5. Write snapshot data to Redis (overwrite current player data)
	saveResp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{
		PlayerID:        req.PlayerID,
		Data:            fieldsToRestore,
		ExpectedVersion: 0, // skip version check — this is an admin override
	})
	if err != nil {
		return &RollbackPlayerResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if saveResp.ErrorCode != constants.ErrCodeOK {
		return &RollbackPlayerResp{ErrorCode: saveResp.ErrorCode}, nil
	}

	// 6. Collect field names
	restoredFields := make([]string, 0, len(fieldsToRestore))
	for f := range fieldsToRestore {
		restoredFields = append(restoredFields, f)
	}

	return &RollbackPlayerResp{
		SnapshotIDUsed:        snap.ID,
		PreRollbackSnapshotID: preRollbackID,
		FieldsRestored:        restoredFields,
	}, nil
}

// ── RollbackZone ───────────────────────────────────────────────

type RollbackZoneReq struct {
	ZoneID     uint32
	TargetTime uint64
	Reason     string
	Operator   string
}

type RollbackZoneResp struct {
	ErrorCode        uint32
	PlayersAffected  uint32
	PlayersFailed    uint32
	FailedPlayerIDs  []uint64
	OrphanPlayerIDs  []uint64 // characters created after target_time, cleaned up
	OrphansCleaned   uint32
}

func RollbackZone(ctx context.Context, svcCtx *svc.ServiceContext, req *RollbackZoneReq) (*RollbackZoneResp, error) {
	if req.ZoneID == 0 || req.TargetTime == 0 {
		return &RollbackZoneResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}

	logx.Infof("[Rollback] zone=%d target_time=%d operator=%s reason=%s",
		req.ZoneID, req.TargetTime, req.Operator, req.Reason)

	// NOTE: Guild/friend data is NOT rolled back.
	// Player Redis blob does not contain guild_id or friend references;
	// guild/friend state is owned by separate Go services and queried via gRPC.
	// Any minor inconsistencies (e.g. stale guild membership) are self-healing
	// through normal guild/friend operations.

	// ── Phase 1: Rollback player data from individual snapshots ─
	// 1. Get all players in this zone that have snapshots
	playerIDs, err := svcCtx.SnapshotStore.GetSnapshotPlayerIDsByZone(ctx, req.ZoneID, req.TargetTime)
	if err != nil {
		logx.Errorf("[Rollback] zone %d: failed to list players: %v", req.ZoneID, err)
		return &RollbackZoneResp{ErrorCode: constants.ErrCodeSnapshotDBError}, err
	}

	if len(playerIDs) == 0 {
		return &RollbackZoneResp{ErrorCode: constants.ErrCodeZoneNotFound}, nil
	}

	logx.Infof("[Rollback] zone %d: found %d players to rollback", req.ZoneID, len(playerIDs))

	// Build set for fast lookup
	snapshotPlayerSet := make(map[uint64]bool, len(playerIDs))
	for _, pid := range playerIDs {
		snapshotPlayerSet[pid] = true
	}

	// 2. Rollback each player
	var affected, failed uint32
	var failedIDs []uint64

	for _, pid := range playerIDs {
		resp, err := rollbackSinglePlayer(ctx, svcCtx, &RollbackPlayerReq{
			PlayerID:   pid,
			TargetTime: req.TargetTime,
			Reason:     fmt.Sprintf("zone %d rollback: %s", req.ZoneID, req.Reason),
			Operator:   req.Operator,
		})
		if err != nil || resp.ErrorCode != constants.ErrCodeOK {
			failed++
			failedIDs = append(failedIDs, pid)
			logx.Errorf("[Rollback] zone %d player %d failed: err=%v code=%d",
				req.ZoneID, pid, err, resp.ErrorCode)
			continue
		}
		affected++
	}

	// ── Phase 2: Clean up orphan characters ────────────────────
	// Characters created after target_time have no snapshot.
	// Delete their zone mapping so login doesn't route to empty data.
	orphanIDs, orphansCleaned := cleanupOrphanCharacters(ctx, svcCtx, req.ZoneID, snapshotPlayerSet)

	// ── Phase 2b: Remove orphans from login accounts ───────────
	// Call login admin to strip orphan player IDs from account records.
	if len(orphanIDs) > 0 && svcCtx.LoginAdminClient != nil {
		removeOrphansFromLoginAccounts(ctx, svcCtx, req.ZoneID, orphanIDs)
	} else if len(orphanIDs) > 0 {
		logx.Infof("[Rollback] zone %d: %d orphans found but LoginAdminClient not configured — account cleanup skipped (orphan IDs returned in response for external handling)",
			req.ZoneID, len(orphanIDs))
	}

	// 3. Audit log
	now := uint64(time.Now().Unix())
	_ = svcCtx.SnapshotStore.InsertAuditLog(ctx, &store.AuditLogRow{
		ZoneID:          req.ZoneID,
		RollbackType:    rollbackTypeZone,
		TargetTime:      req.TargetTime,
		PlayersAffected: affected,
		PlayersFailed:   failed,
		OrphansCleaned:  orphansCleaned,
		Reason:          req.Reason,
		Operator:        req.Operator,
		CreatedAt:       now,
	})

	logx.Infof("[Rollback] zone %d complete: affected=%d failed=%d orphans_cleaned=%d",
		req.ZoneID, affected, failed, orphansCleaned)

	return &RollbackZoneResp{
		PlayersAffected:  affected,
		PlayersFailed:    failed,
		FailedPlayerIDs:  failedIDs,
		OrphanPlayerIDs:  orphanIDs,
		OrphansCleaned:   orphansCleaned,
	}, nil
}

// cleanupOrphanCharacters removes Redis data and zone mappings for characters
// that were created after the rollback target time (they have no snapshot).
// Returns the list of orphan player IDs and the count of successfully cleaned.
// The orphan list is needed for cross-service cleanup (e.g. login account removal).
func cleanupOrphanCharacters(ctx context.Context, svcCtx *svc.ServiceContext, zoneID uint32, snapshotPlayerSet map[uint64]bool) ([]uint64, uint32) {
	currentPlayers, err := svcCtx.Router.GetAllPlayerIDsInZone(ctx, zoneID)
	if err != nil {
		logx.Errorf("[Rollback] zone %d: failed to scan current players for orphan cleanup: %v", zoneID, err)
		return nil, 0
	}

	var orphanIDs []uint64
	var cleaned uint32
	for _, pid := range currentPlayers {
		if snapshotPlayerSet[pid] {
			continue // existed at snapshot time — not an orphan
		}

		orphanIDs = append(orphanIDs, pid)

		// This player was created after the snapshot time → orphan
		logx.Infof("[Rollback] zone %d: cleaning up orphan character %d", zoneID, pid)

		// Delete player Redis data + zone mapping
		delResp, err := DeletePlayerData(ctx, svcCtx, &DeletePlayerDataReq{
			PlayerID:          pid,
			DeleteZoneMapping: true,
		})
		if err != nil {
			logx.Errorf("[Rollback] zone %d: failed to delete orphan %d: %v", zoneID, pid, err)
			continue
		}
		logx.Infof("[Rollback] zone %d: orphan %d cleaned — keys_deleted=%d", zoneID, pid, delResp.KeysDeleted)
		cleaned++
	}

	if cleaned > 0 {
		logx.Infof("[Rollback] zone %d: cleaned %d orphan characters (total orphans=%d)", zoneID, cleaned, len(orphanIDs))
	}
	return orphanIDs, cleaned
}

// removeOrphansFromLoginAccounts calls the login admin RPC to remove orphan
// player IDs from their parent account records. This is non-fatal — if it fails,
// the orphan IDs are still returned in the response for external retry.
func removeOrphansFromLoginAccounts(ctx context.Context, svcCtx *svc.ServiceContext, zoneID uint32, orphanIDs []uint64) {
	logx.Infof("[Rollback] zone %d: calling login RemovePlayersFromAccounts for %d orphans", zoneID, len(orphanIDs))

	resp, err := svcCtx.LoginAdminClient.RemovePlayersFromAccounts(ctx, &loginpb.RemovePlayersFromAccountsRequest{
		PlayerIds: orphanIDs,
	})
	if err != nil {
		logx.Errorf("[Rollback] zone %d: login RemovePlayersFromAccounts RPC failed: %v (orphan IDs returned in response for manual retry)", zoneID, err)
		return
	}

	logx.Infof("[Rollback] zone %d: login orphan account cleanup: removed=%d not_found=%d failed=%d",
		zoneID, resp.RemovedCount, resp.NotFoundCount, resp.FailedCount)
}

// ── RollbackAll (full server) ──────────────────────────────────

type RollbackAllReq struct {
	TargetTime uint64
	Reason     string
	Operator   string
}

type RollbackAllResp struct {
	ErrorCode       uint32
	ZonesProcessed  uint32
	PlayersAffected uint32
	PlayersFailed   uint32
}

func RollbackAll(ctx context.Context, svcCtx *svc.ServiceContext, req *RollbackAllReq) (*RollbackAllResp, error) {
	if req.TargetTime == 0 {
		return &RollbackAllResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}

	logx.Infof("[Rollback] FULL SERVER rollback target_time=%d operator=%s reason=%s",
		req.TargetTime, req.Operator, req.Reason)

	// Get all zones from config
	zoneIDs := svcCtx.Router.AllZoneIDs()

	var totalAffected, totalFailed, zonesProcessed uint32

	for _, zoneID := range zoneIDs {
		resp, err := RollbackZone(ctx, svcCtx, &RollbackZoneReq{
			ZoneID:     zoneID,
			TargetTime: req.TargetTime,
			Reason:     fmt.Sprintf("server rollback: %s", req.Reason),
			Operator:   req.Operator,
		})
		if err != nil {
			logx.Errorf("[Rollback] zone %d error during server rollback: %v", zoneID, err)
			continue
		}
		zonesProcessed++
		totalAffected += resp.PlayersAffected
		totalFailed += resp.PlayersFailed
	}

	// Audit log
	now := uint64(time.Now().Unix())
	_ = svcCtx.SnapshotStore.InsertAuditLog(ctx, &store.AuditLogRow{
		RollbackType:    rollbackTypeServer,
		TargetTime:      req.TargetTime,
		PlayersAffected: totalAffected,
		PlayersFailed:   totalFailed,
		Reason:          req.Reason,
		Operator:        req.Operator,
		CreatedAt:       now,
	})

	logx.Infof("[Rollback] FULL SERVER complete: zones=%d affected=%d failed=%d",
		zonesProcessed, totalAffected, totalFailed)

	return &RollbackAllResp{
		ZonesProcessed:  zonesProcessed,
		PlayersAffected: totalAffected,
		PlayersFailed:   totalFailed,
	}, nil
}
