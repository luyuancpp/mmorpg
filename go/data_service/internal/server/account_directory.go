package server

import (
	"context"
	"errors"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/emptypb"
	dspb "proto/data_service"
	"shared/accountdirectory"
)

func (s *DataServiceServer) admitDirectory(ctx context.Context, method string, req proto.Message) error {
	if s.svcCtx.AccountDirectoryAuth == nil || s.svcCtx.AccountDirectory == nil {
		return status.Error(codes.Unavailable, "全局账号目录未就绪")
	}
	caller, err := s.svcCtx.AccountDirectoryAuth.Verify(ctx, accountdirectory.MethodPrefix+method, req)
	if err != nil || caller != "login" {
		return status.Error(codes.PermissionDenied, "账号目录调用证明无效")
	}
	return nil
}
func directoryError(err error) error {
	if err == nil {
		return nil
	}
	switch {
	case errors.Is(err, accountdirectory.ErrInvalid):
		return status.Error(codes.InvalidArgument, "账号目录参数非法")
	case errors.Is(err, accountdirectory.ErrMissing):
		return status.Error(codes.NotFound, "账号角色目录不存在")
	case errors.Is(err, accountdirectory.ErrFull):
		return status.Error(codes.ResourceExhausted, "账号角色数量已达全局上限")
	case errors.Is(err, accountdirectory.ErrConflict):
		return status.Error(codes.FailedPrecondition, "账号角色归属或身份已变化")
	default:
		logx.Errorf("[account-directory] SQL依赖失败: %v", err)
		return status.Error(codes.Unavailable, "全局账号目录暂不可用")
	}
}
func (s *DataServiceServer) ReadAccountDirectory(ctx context.Context, in *dspb.ReadAccountDirectoryRequest) (*dspb.AccountDirectoryResponse, error) {
	if err := s.admitDirectory(ctx, "ReadAccountDirectory", in); err != nil {
		return nil, err
	}
	out, err := s.svcCtx.AccountDirectory.Read(ctx, in.Account, in.CreateIfMissing)
	if err != nil {
		return nil, directoryError(err)
	}
	return &dspb.AccountDirectoryResponse{Players: out}, nil
}
func (s *DataServiceServer) CreateAccountRole(ctx context.Context, in *dspb.CreateAccountRoleRequest) (*dspb.AccountDirectoryResponse, error) {
	if err := s.admitDirectory(ctx, "CreateAccountRole", in); err != nil {
		return nil, err
	}
	out, err := s.svcCtx.AccountDirectory.Create(ctx, in.Account, in.Role)
	if err != nil {
		return nil, directoryError(err)
	}
	return &dspb.AccountDirectoryResponse{Players: out}, nil
}
func (s *DataServiceServer) LookupAccountRole(ctx context.Context, in *dspb.LookupAccountRoleRequest) (*dspb.AccountRoleOwnership, error) {
	if s.svcCtx.AccountDirectory == nil {
		return nil, status.Error(codes.Unavailable, "全局账号目录未就绪")
	}
	if err := s.admitDirectory(ctx, "LookupAccountRole", in); err != nil {
		// 仅此只读方法允许trade独立密钥。Create/Repair/Remove仍只认login，不扩大写权限。
		accepted := false
		if auth := s.svcCtx.SceneManagerDirectoryAuth; auth != nil {
			caller, verifyErr := auth.Verify(ctx, accountdirectory.MethodPrefix+"LookupAccountRole", in)
			accepted = verifyErr == nil && caller == "scene_manager"
		}
		if !accepted {
			if s.svcCtx.AccountRoleTradeAuth == nil || s.svcCtx.AccountRoleTrades == nil {
				return nil, err
			}
			if tradeErr := s.admitRoleTrade(ctx, "LookupAccountRole", in); tradeErr != nil {
				return nil, tradeErr
			}
		}
	}
	out, err := s.svcCtx.AccountDirectory.Lookup(ctx, in.PlayerId)
	return out, directoryError(err)
}
func (s *DataServiceServer) RepairAccountRole(ctx context.Context, in *dspb.RepairAccountRoleRequest) (*dspb.AccountDirectoryResponse, error) {
	if err := s.admitDirectory(ctx, "RepairAccountRole", in); err != nil {
		return nil, err
	}
	out, err := s.svcCtx.AccountDirectory.Repair(ctx, in.Account, in.Role, in.ExpectedOwnerRevision)
	if err != nil {
		return nil, directoryError(err)
	}
	return &dspb.AccountDirectoryResponse{Players: out}, nil
}
func (s *DataServiceServer) RemoveAccountRoles(ctx context.Context, in *dspb.RemoveAccountRolesRequest) (*emptypb.Empty, error) {
	if err := s.admitDirectory(ctx, "RemoveAccountRoles", in); err != nil {
		return nil, err
	}
	if err := s.svcCtx.AccountDirectory.Remove(ctx, in.Account, in.Roles); err != nil {
		return nil, directoryError(err)
	}
	return &emptypb.Empty{}, nil
}
