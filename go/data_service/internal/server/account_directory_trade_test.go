package server

import (
	"context"
	"testing"
	"time"

	"data_service/internal/svc"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"
	"shared/serviceauth"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

func TestTradeProofOnlyGrantsReadOnlyAccountRoleLookup(t *testing.T) {
	fake := &serverDirectoryFake{ownership: &dspb.AccountRoleOwnership{Account: "原账号", OwnerRevision: 7, Role: &base.AccountSimplePlayer{PlayerId: 11}}}
	s := NewDataServiceServer(&svc.ServiceContext{AccountDirectory: fake, AccountDirectoryAuth: roleTradeVerifier(t), AccountRoleTrades: &roleTradesFake{}, AccountRoleTradeAuth: roleTradeVerifier(t)})
	lookup := &dspb.LookupAccountRoleRequest{PlayerId: 11}
	require.NoError(t, serviceauth.Sign(accountdirectory.MethodPrefix+"LookupAccountRole", lookup, "trade", directoryTestSecret, uint64(time.Now().UnixMilli())))
	out, err := s.LookupAccountRole(context.Background(), lookup)
	require.NoError(t, err)
	require.Equal(t, "原账号", out.Account)
	require.Equal(t, []string{"Lookup"}, fake.calls)
	create := &dspb.CreateAccountRoleRequest{Account: "原账号", Role: &base.AccountSimplePlayer{PlayerId: 12}}
	require.NoError(t, serviceauth.Sign(accountdirectory.MethodPrefix+"CreateAccountRole", create, "trade", directoryTestSecret, uint64(time.Now().UnixMilli())))
	_, err = s.CreateAccountRole(context.Background(), create)
	require.Equal(t, codes.PermissionDenied, status.Code(err))
	require.Equal(t, []string{"Lookup"}, fake.calls)
	s.svcCtx.AccountDirectory = nil
	_, err = s.LookupAccountRole(context.Background(), lookup)
	require.Equal(t, codes.Unavailable, status.Code(err))
}
