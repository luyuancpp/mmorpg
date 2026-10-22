package svc

import (
	"context"
	"testing"
	"time"

	dspb "proto/data_service"
	"scene_manager/internal/config"
	"shared/accountdirectory"
	"shared/serviceauth"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
)

type roleLookupRPC struct {
	dspb.DataServiceClient
	lookup func(context.Context, *dspb.LookupAccountRoleRequest) (*dspb.AccountRoleOwnership, error)
}

func (r roleLookupRPC) LookupAccountRole(ctx context.Context, in *dspb.LookupAccountRoleRequest, _ ...grpc.CallOption) (*dspb.AccountRoleOwnership, error) {
	return r.lookup(ctx, in)
}

func TestRoleDirectoryClientOnlySignsLookupWithIndependentCaller(t *testing.T) {
	const secret = "scene-manager-independent-test-key-0001"
	verify, err := serviceauth.New(map[string]string{"scene_manager": secret})
	require.NoError(t, err)
	client := &roleDirectoryClient{secret: secret, rpc: roleLookupRPC{lookup: func(ctx context.Context, in *dspb.LookupAccountRoleRequest) (*dspb.AccountRoleOwnership, error) {
		caller, err := verify.Verify(ctx, accountdirectory.MethodPrefix+"LookupAccountRole", in)
		require.NoError(t, err)
		require.Equal(t, "scene_manager", caller)
		require.Equal(t, ^uint64(0), in.PlayerId)
		deadline, ok := ctx.Deadline()
		require.True(t, ok)
		require.LessOrEqual(t, time.Until(deadline), 1500*time.Millisecond)
		_, err = verify.Verify(ctx, accountdirectory.MethodPrefix+"CreateAccountRole", in)
		require.Error(t, err)
		return &dspb.AccountRoleOwnership{OwnerRevision: 17}, nil
	}}}
	out, err := client.Lookup(context.Background(), ^uint64(0))
	require.NoError(t, err)
	require.Equal(t, uint64(17), out.OwnerRevision)
}

func TestRoleDirectoryConfigurationCannotUseHomeZoneFallbackOrOtherCallerSecret(t *testing.T) {
	c := config.Config{}
	c.AllowGateZoneAsHomeZone = true
	_, err := roleDirectorySecret(c, func(string) string { return "scene-manager-independent-test-key-0001" })
	require.Error(t, err)
	c.DataServiceRpc.Endpoints = []string{"127.0.0.1:9000"}
	_, err = roleDirectorySecret(c, func(name string) string {
		if name == accountdirectory.SecretEnv {
			return "login-only-test-key-0000000000000000"
		}
		return ""
	})
	require.Error(t, err)
	value, err := roleDirectorySecret(c, func(name string) string {
		require.Equal(t, RoleDirectorySecretEnv, name)
		return "scene-manager-independent-test-key-0001"
	})
	require.NoError(t, err)
	require.NotEmpty(t, value)
}
