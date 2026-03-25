package server

import (
	"context"

	"proto/data_service"
	"data_service/internal/constants"
	"data_service/internal/logic"
	"data_service/internal/svc"

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
