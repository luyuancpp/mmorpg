package server

import (
	"context"

	dspb "proto/data_service"
	"shared/accountdirectory"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/emptypb"
)

func (s *DataServiceServer) admitRoleAdmission(ctx context.Context, method string, in proto.Message) error {
	if s.svcCtx.RoleAdmissions == nil || s.svcCtx.RoleAdmissionAuth == nil {
		return status.Error(codes.Unavailable, "角色入场登记未就绪")
	}
	caller, err := s.svcCtx.RoleAdmissionAuth.Verify(ctx, accountdirectory.MethodPrefix+method, in)
	if err != nil || caller != "scene" {
		return status.Error(codes.PermissionDenied, "角色入场调用证明无效")
	}
	return nil
}
func (s *DataServiceServer) RegisterRoleAdmission(ctx context.Context, in *dspb.RegisterRoleAdmissionRequest) (*dspb.RegisterRoleAdmissionResponse, error) {
	if err := s.admitRoleAdmission(ctx, "RegisterRoleAdmission", in); err != nil {
		return nil, err
	}
	out, err := s.svcCtx.RoleAdmissions.Register(ctx, in)
	if err != nil {
		return nil, directoryError(err)
	}
	return out, nil
}
func (s *DataServiceServer) CloseRoleAdmissions(ctx context.Context, in *dspb.CloseRoleAdmissionsRequest) (*dspb.CloseRoleAdmissionsResponse, error) {
	if err := s.admitRoleAdmission(ctx, "CloseRoleAdmissions", in); err != nil {
		return nil, err
	}
	out, err := s.svcCtx.RoleAdmissions.CloseAdmissions(ctx, in)
	if err != nil {
		return nil, directoryError(err)
	}
	return out, nil
}
func (s *DataServiceServer) AckRoleTradeDrain(ctx context.Context, in *dspb.AckRoleTradeDrainRequest) (*emptypb.Empty, error) {
	if err := s.admitRoleAdmission(ctx, "AckRoleTradeDrain", in); err != nil {
		return nil, err
	}
	if err := s.svcCtx.RoleAdmissions.AckDrain(ctx, in); err != nil {
		return nil, directoryError(err)
	}
	return &emptypb.Empty{}, nil
}
