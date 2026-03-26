package logic

import (
	"context"

	"data_service/internal/constants"
	"data_service/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// ── CreateZoneSnapshot ─────────────────────────────────────────

type CreateZoneSnapshotReq struct {
	ZoneID   uint32
	Reason   string
	Operator string
}

type CreateZoneSnapshotResp struct {
	ErrorCode  uint32
	SnapshotID uint64
	CreatedAt  uint64
}

// CreateZoneSnapshot captures guild/friend data + player list for a zone.
// Should be called AFTER Kafka is drained (all player writes flushed to MySQL)
// to ensure cross-service consistency.
func CreateZoneSnapshot(ctx context.Context, svcCtx *svc.ServiceContext, req *CreateZoneSnapshotReq) (*CreateZoneSnapshotResp, error) {
	if req.ZoneID == 0 {
		return &CreateZoneSnapshotResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}
	if svcCtx.SnapshotStore == nil {
		return &CreateZoneSnapshotResp{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}

	// 1. Get all players currently mapped to this zone
	playerIDs, err := svcCtx.Router.GetAllPlayerIDsInZone(ctx, req.ZoneID)
	if err != nil {
		logx.Errorf("[ZoneSnapshot] failed to get players for zone %d: %v", req.ZoneID, err)
		return &CreateZoneSnapshotResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	logx.Infof("[ZoneSnapshot] zone %d: found %d players, capturing guild/friend state",
		req.ZoneID, len(playerIDs))

	// 2. Capture guild/friend tables + player list → zone_snapshot
	id, err := svcCtx.SnapshotStore.CaptureZoneSnapshot(ctx, req.ZoneID, playerIDs, req.Reason, req.Operator)
	if err != nil {
		logx.Errorf("[ZoneSnapshot] zone %d: capture failed: %v", req.ZoneID, err)
		return &CreateZoneSnapshotResp{ErrorCode: constants.ErrCodeSnapshotDBError}, err
	}

	return &CreateZoneSnapshotResp{SnapshotID: id}, nil
}

// ── ListZoneSnapshots ──────────────────────────────────────────

type ListZoneSnapshotsReq struct {
	ZoneID uint32
	Limit  uint32
}

type ZoneSnapshotInfoItem struct {
	SnapshotID    uint64
	ZoneID        uint32
	CreatedAt     uint64
	Reason        string
	Operator      string
	DataSizeBytes uint32
}

type ListZoneSnapshotsResp struct {
	ErrorCode uint32
	Snapshots []*ZoneSnapshotInfoItem
}

func ListZoneSnapshots(ctx context.Context, svcCtx *svc.ServiceContext, req *ListZoneSnapshotsReq) (*ListZoneSnapshotsResp, error) {
	if req.ZoneID == 0 {
		return &ListZoneSnapshotsResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}
	if svcCtx.SnapshotStore == nil {
		return &ListZoneSnapshotsResp{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}

	metas, err := svcCtx.SnapshotStore.ListZoneSnapshotsMeta(ctx, req.ZoneID, req.Limit)
	if err != nil {
		logx.Errorf("[ZoneSnapshot] list zone %d: %v", req.ZoneID, err)
		return &ListZoneSnapshotsResp{ErrorCode: constants.ErrCodeSnapshotDBError}, err
	}

	items := make([]*ZoneSnapshotInfoItem, 0, len(metas))
	for _, m := range metas {
		items = append(items, &ZoneSnapshotInfoItem{
			SnapshotID:    m.ID,
			ZoneID:        m.ZoneID,
			CreatedAt:     m.CreatedAt,
			Reason:        m.Reason,
			Operator:      m.Operator,
			DataSizeBytes: m.DataSizeBytes,
		})
	}

	return &ListZoneSnapshotsResp{Snapshots: items}, nil
}
