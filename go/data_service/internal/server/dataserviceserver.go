package server

import (
	"context"

	"data_service/internal/constants"
	"data_service/internal/logic"
	"data_service/internal/svc"
	"proto/data_service"

	"google.golang.org/protobuf/types/known/emptypb"
)

type DataServiceServer struct {
	svcCtx *svc.ServiceContext
	data_service.UnimplementedDataServiceServer
}

func NewDataServiceServer(svcCtx *svc.ServiceContext) *DataServiceServer {
	return &DataServiceServer{svcCtx: svcCtx}
}

func (s *DataServiceServer) LoadPlayerData(ctx context.Context, req *data_service.LoadPlayerDataRequest) (*data_service.LoadPlayerDataResponse, error) {
	resp, err := logic.LoadPlayerData(ctx, s.svcCtx, &logic.LoadPlayerDataReq{
		PlayerID: req.PlayerId,
		Fields:   req.Fields,
	})
	if err != nil {
		return &data_service.LoadPlayerDataResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.LoadPlayerDataResponse{
		ErrorCode: resp.ErrorCode,
		Data:      resp.Data,
		Version:   resp.Version,
	}, nil
}

func (s *DataServiceServer) SavePlayerData(ctx context.Context, req *data_service.SavePlayerDataRequest) (*data_service.SavePlayerDataResponse, error) {
	resp, err := logic.SavePlayerData(ctx, s.svcCtx, &logic.SavePlayerDataReq{
		PlayerID:        req.PlayerId,
		Data:            req.Data,
		ExpectedVersion: req.ExpectedVersion,
	})
	if err != nil {
		return &data_service.SavePlayerDataResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.SavePlayerDataResponse{
		ErrorCode:  resp.ErrorCode,
		NewVersion: resp.NewVersion,
	}, nil
}

func (s *DataServiceServer) GetPlayerField(ctx context.Context, req *data_service.GetPlayerFieldRequest) (*data_service.GetPlayerFieldResponse, error) {
	val, err := logic.GetPlayerField(ctx, s.svcCtx, req.PlayerId, req.Field)
	if err != nil {
		return &data_service.GetPlayerFieldResponse{ErrorCode: constants.ErrCodeRedis}, nil
	}
	return &data_service.GetPlayerFieldResponse{Value: val}, nil
}

func (s *DataServiceServer) SetPlayerField(ctx context.Context, req *data_service.SetPlayerFieldRequest) (*data_service.SetPlayerFieldResponse, error) {
	resp, err := logic.SetPlayerField(ctx, s.svcCtx, req.PlayerId, req.Field, req.Value, req.ExpectedVersion)
	if err != nil {
		return &data_service.SetPlayerFieldResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.SetPlayerFieldResponse{
		ErrorCode:  resp.ErrorCode,
		NewVersion: resp.NewVersion,
	}, nil
}

func (s *DataServiceServer) RegisterPlayerZone(ctx context.Context, req *data_service.RegisterPlayerZoneRequest) (*emptypb.Empty, error) {
	err := s.svcCtx.Router.RegisterPlayerZone(ctx, req.PlayerId, req.HomeZoneId)
	if err != nil {
		return nil, err
	}
	return &emptypb.Empty{}, nil
}

func (s *DataServiceServer) GetPlayerHomeZone(ctx context.Context, req *data_service.GetPlayerHomeZoneRequest) (*data_service.GetPlayerHomeZoneResponse, error) {
	zoneID, err := s.svcCtx.Router.GetPlayerHomeZone(ctx, req.PlayerId)
	if err != nil {
		return nil, err
	}
	return &data_service.GetPlayerHomeZoneResponse{HomeZoneId: zoneID}, nil
}

func (s *DataServiceServer) BatchGetPlayerHomeZone(ctx context.Context, req *data_service.BatchGetPlayerHomeZoneRequest) (*data_service.BatchGetPlayerHomeZoneResponse, error) {
	mapping, err := s.svcCtx.Router.BatchGetPlayerHomeZone(ctx, req.PlayerIds)
	if err != nil {
		return nil, err
	}
	return &data_service.BatchGetPlayerHomeZoneResponse{PlayerZoneMap: mapping}, nil
}

func (s *DataServiceServer) DeletePlayerData(ctx context.Context, req *data_service.DeletePlayerDataRequest) (*data_service.DeletePlayerDataResponse, error) {
	resp, err := logic.DeletePlayerData(ctx, s.svcCtx, &logic.DeletePlayerDataReq{
		PlayerID:           req.PlayerId,
		DeleteZoneMapping:  req.DeleteZoneMapping,
	})
	if err != nil {
		return &data_service.DeletePlayerDataResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.DeletePlayerDataResponse{
		ErrorCode:   resp.ErrorCode,
		KeysDeleted: resp.KeysDeleted,
	}, nil
}

// ── Snapshot / Rollback Handlers ───────────────────────────────

func (s *DataServiceServer) CreatePlayerSnapshot(ctx context.Context, req *data_service.CreatePlayerSnapshotRequest) (*data_service.CreatePlayerSnapshotResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.CreatePlayerSnapshotResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.CreatePlayerSnapshot(ctx, s.svcCtx, &logic.CreateSnapshotReq{
		PlayerID:     req.PlayerId,
		SnapshotType: uint32(req.Type),
		Reason:       req.Reason,
		Operator:     req.Operator,
	})
	if err != nil {
		return &data_service.CreatePlayerSnapshotResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.CreatePlayerSnapshotResponse{
		ErrorCode:  resp.ErrorCode,
		SnapshotId: resp.SnapshotID,
		CreatedAt:  resp.CreatedAt,
	}, nil
}

func (s *DataServiceServer) ListPlayerSnapshots(ctx context.Context, req *data_service.ListPlayerSnapshotsRequest) (*data_service.ListPlayerSnapshotsResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.ListPlayerSnapshotsResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.ListPlayerSnapshots(ctx, s.svcCtx, &logic.ListSnapshotsReq{
		PlayerID:   req.PlayerId,
		BeforeTime: req.BeforeTime,
		Limit:      req.Limit,
	})
	if err != nil {
		return &data_service.ListPlayerSnapshotsResponse{ErrorCode: resp.ErrorCode}, nil
	}

	infos := make([]*data_service.SnapshotInfo, 0, len(resp.Snapshots))
	for _, item := range resp.Snapshots {
		infos = append(infos, &data_service.SnapshotInfo{
			SnapshotId:    item.SnapshotID,
			PlayerId:      item.PlayerID,
			ZoneId:        item.ZoneID,
			Type:          data_service.SnapshotType(item.SnapshotType),
			CreatedAt:     item.CreatedAt,
			Reason:        item.Reason,
			Operator:      item.Operator,
			DataSizeBytes: item.DataSizeBytes,
		})
	}
	return &data_service.ListPlayerSnapshotsResponse{
		ErrorCode: resp.ErrorCode,
		Snapshots: infos,
	}, nil
}

func (s *DataServiceServer) GetPlayerSnapshotDiff(ctx context.Context, req *data_service.GetPlayerSnapshotDiffRequest) (*data_service.GetPlayerSnapshotDiffResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.GetPlayerSnapshotDiffResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.GetPlayerSnapshotDiff(ctx, s.svcCtx, &logic.SnapshotDiffReq{
		PlayerID:   req.PlayerId,
		SnapshotID: req.SnapshotId,
		TargetTime: req.TargetTime,
	})
	if err != nil {
		return &data_service.GetPlayerSnapshotDiffResponse{ErrorCode: resp.ErrorCode}, nil
	}

	diffs := make([]*data_service.FieldDiff, 0, len(resp.Diffs))
	for _, d := range resp.Diffs {
		diffs = append(diffs, &data_service.FieldDiff{
			Field:          d.Field,
			SnapshotValue:  d.SnapshotValue,
			CurrentValue:   d.CurrentValue,
			OnlyInSnapshot: d.OnlyInSnapshot,
			OnlyInCurrent:  d.OnlyInCurrent,
		})
	}
	return &data_service.GetPlayerSnapshotDiffResponse{
		ErrorCode:      resp.ErrorCode,
		SnapshotIdUsed: resp.SnapshotIDUsed,
		SnapshotTime:   resp.SnapshotTime,
		Diffs:          diffs,
	}, nil
}

func (s *DataServiceServer) RollbackPlayer(ctx context.Context, req *data_service.RollbackPlayerRequest) (*data_service.RollbackPlayerResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.RollbackPlayerResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.RollbackPlayer(ctx, s.svcCtx, &logic.RollbackPlayerReq{
		PlayerID:   req.PlayerId,
		SnapshotID: req.SnapshotId,
		TargetTime: req.TargetTime,
		Scope:      uint32(req.Scope),
		Fields:     req.Fields,
		Reason:     req.Reason,
		Operator:   req.Operator,
	})
	if err != nil {
		return &data_service.RollbackPlayerResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.RollbackPlayerResponse{
		ErrorCode:             resp.ErrorCode,
		SnapshotIdUsed:        resp.SnapshotIDUsed,
		PreRollbackSnapshotId: resp.PreRollbackSnapshotID,
		FieldsRestored:        resp.FieldsRestored,
	}, nil
}

func (s *DataServiceServer) RollbackZone(ctx context.Context, req *data_service.RollbackZoneRequest) (*data_service.RollbackZoneResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.RollbackZoneResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.RollbackZone(ctx, s.svcCtx, &logic.RollbackZoneReq{
		ZoneID:     req.ZoneId,
		TargetTime: req.TargetTime,
		Reason:     req.Reason,
		Operator:   req.Operator,
	})
	if err != nil {
		return &data_service.RollbackZoneResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.RollbackZoneResponse{
		ErrorCode:       resp.ErrorCode,
		PlayersAffected: resp.PlayersAffected,
		PlayersFailed:   resp.PlayersFailed,
		FailedPlayerIds: resp.FailedPlayerIDs,
	}, nil
}

func (s *DataServiceServer) RollbackAll(ctx context.Context, req *data_service.RollbackAllRequest) (*data_service.RollbackAllResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.RollbackAllResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.RollbackAll(ctx, s.svcCtx, &logic.RollbackAllReq{
		TargetTime: req.TargetTime,
		Reason:     req.Reason,
		Operator:   req.Operator,
	})
	if err != nil {
		return &data_service.RollbackAllResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.RollbackAllResponse{
		ErrorCode:       resp.ErrorCode,
		ZonesProcessed:  resp.ZonesProcessed,
		PlayersAffected: resp.PlayersAffected,
		PlayersFailed:   resp.PlayersFailed,
	}, nil
}

func (s *DataServiceServer) CreateZoneSnapshot(ctx context.Context, req *data_service.CreateZoneSnapshotRequest) (*data_service.CreateZoneSnapshotResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.CreateZoneSnapshotResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.CreateZoneSnapshot(ctx, s.svcCtx, &logic.CreateZoneSnapshotReq{
		ZoneID:   req.ZoneId,
		Reason:   req.Reason,
		Operator: req.Operator,
	})
	if err != nil {
		return &data_service.CreateZoneSnapshotResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.CreateZoneSnapshotResponse{
		ErrorCode:  resp.ErrorCode,
		SnapshotId: resp.SnapshotID,
		CreatedAt:  resp.CreatedAt,
	}, nil
}

func (s *DataServiceServer) ListZoneSnapshots(ctx context.Context, req *data_service.ListZoneSnapshotsRequest) (*data_service.ListZoneSnapshotsResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.ListZoneSnapshotsResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.ListZoneSnapshots(ctx, s.svcCtx, &logic.ListZoneSnapshotsReq{
		ZoneID: req.ZoneId,
		Limit:  req.Limit,
	})
	if err != nil {
		return &data_service.ListZoneSnapshotsResponse{ErrorCode: resp.ErrorCode}, nil
	}

	infos := make([]*data_service.ZoneSnapshotInfo, 0, len(resp.Snapshots))
	for _, item := range resp.Snapshots {
		infos = append(infos, &data_service.ZoneSnapshotInfo{
			SnapshotId:    item.SnapshotID,
			ZoneId:        item.ZoneID,
			CreatedAt:     item.CreatedAt,
			Reason:        item.Reason,
			Operator:      item.Operator,
			DataSizeBytes: item.DataSizeBytes,
		})
	}
	return &data_service.ListZoneSnapshotsResponse{
		ErrorCode: resp.ErrorCode,
		Snapshots: infos,
	}, nil
}

// ── Batch Recall / Transaction Log Query / Event Snapshot ──────

func (s *DataServiceServer) BatchRecallItems(ctx context.Context, req *data_service.BatchRecallItemsRequest) (*data_service.BatchRecallItemsResponse, error) {
	resp, err := logic.BatchRecallItems(ctx, s.svcCtx, &logic.BatchRecallReq{
		PlayerIDs:    req.PlayerIds,
		ItemConfigID: req.ItemConfigId,
		CurrencyType: req.CurrencyType,
		TimeStart:    req.TimeStart,
		TimeEnd:      req.TimeEnd,
		TxTypes:      req.TxTypes,
		Reason:       req.Reason,
		Operator:     req.Operator,
		DryRun:       req.DryRun,
	})
	if err != nil {
		return &data_service.BatchRecallItemsResponse{ErrorCode: resp.ErrorCode}, nil
	}

	results := make([]*data_service.RecallResult, 0, len(resp.Results))
	for _, r := range resp.Results {
		results = append(results, &data_service.RecallResult{
			PlayerId:     r.PlayerID,
			ItemUuid:     r.ItemUUID,
			ItemConfigId: r.ItemConfigID,
			Amount:       r.Amount,
			Success:      r.Success,
			ErrorDetail:  r.ErrorDetail,
		})
	}
	return &data_service.BatchRecallItemsResponse{
		ErrorCode:     resp.ErrorCode,
		TotalMatched:  resp.TotalMatched,
		TotalRecalled: resp.TotalRecalled,
		TotalFailed:   resp.TotalFailed,
		Results:       results,
	}, nil
}

func (s *DataServiceServer) QueryTransactionLog(ctx context.Context, req *data_service.QueryTransactionLogRequest) (*data_service.QueryTransactionLogResponse, error) {
	resp, err := logic.QueryTransactionLog(ctx, s.svcCtx, &logic.QueryTxLogReq{
		PlayerID:     req.PlayerId,
		TimeStart:    req.TimeStart,
		TimeEnd:      req.TimeEnd,
		TxTypes:      req.TxTypes,
		ItemConfigID: req.ItemConfigId,
		CurrencyType: req.CurrencyType,
		Limit:        req.Limit,
		Offset:       req.Offset,
	})
	if err != nil {
		return &data_service.QueryTransactionLogResponse{ErrorCode: resp.ErrorCode}, nil
	}

	rows := make([]*data_service.TransactionLogRow, 0, len(resp.Rows))
	for _, r := range resp.Rows {
		rows = append(rows, &data_service.TransactionLogRow{
			TxId:          r.TxID,
			Timestamp:     r.Timestamp,
			TxType:        r.TxType,
			FromPlayer:    r.FromPlayer,
			ToPlayer:      r.ToPlayer,
			ItemUuid:      r.ItemUUID,
			ItemConfigId:  r.ItemConfigID,
			ItemQuantity:  r.ItemQuantity,
			CurrencyType:  r.CurrencyType,
			CurrencyDelta: r.CurrencyDelta,
			BalanceBefore: r.BalanceBefore,
			BalanceAfter:  r.BalanceAfter,
			CorrelationId: r.CorrelationID,
			Extra:         r.Extra,
		})
	}
	return &data_service.QueryTransactionLogResponse{
		ErrorCode:  resp.ErrorCode,
		Rows:       rows,
		TotalCount: resp.TotalCount,
	}, nil
}

func (s *DataServiceServer) CreateEventSnapshot(ctx context.Context, req *data_service.CreateEventSnapshotRequest) (*data_service.CreateEventSnapshotResponse, error) {
	if s.svcCtx.SnapshotStore == nil {
		return &data_service.CreateEventSnapshotResponse{ErrorCode: constants.ErrCodeSnapshotDBError}, nil
	}
	resp, err := logic.CreateEventSnapshot(ctx, s.svcCtx, &logic.CreateEventSnapshotReq{
		PlayerID:    req.PlayerId,
		EventType:   uint32(req.EventType),
		EventDetail: req.EventDetail,
		Operator:    req.Operator,
	})
	if err != nil {
		return &data_service.CreateEventSnapshotResponse{ErrorCode: resp.ErrorCode}, nil
	}
	return &data_service.CreateEventSnapshotResponse{
		ErrorCode:  resp.ErrorCode,
		SnapshotId: resp.SnapshotID,
		CreatedAt:  resp.CreatedAt,
	}, nil
}
