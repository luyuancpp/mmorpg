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

func TestSceneManagerProofOnlyGrantsOriginalRoleLookup(t *testing.T) {
	const sceneManagerSecret = "scene-manager-read-only-test-secret-0001"
	verifier, err := serviceauth.New(map[string]string{"scene_manager": sceneManagerSecret})
	require.NoError(t, err)
	fake := &serverDirectoryFake{ownership: &dspb.AccountRoleOwnership{Account: "original-owner", OwnerRevision: 7, Role: &base.AccountSimplePlayer{PlayerId: 11}}}
	s := NewDataServiceServer(&svc.ServiceContext{AccountDirectory: fake, AccountDirectoryAuth: directoryVerifier(t), SceneManagerDirectoryAuth: verifier, AccountRoleTradeAuth: roleTradeVerifier(t), AccountRoleTrades: &roleTradesFake{}, RoleAdmissionAuth: roleAdmissionVerifier(t), RoleAdmissions: &roleAdmissionsFake{}})
	for _, mode := range []string{"valid", "wrong-purpose", "changed-body", "wrong-caller", "expired", "missing-key"} {
		t.Run(mode, func(t *testing.T) {
			fake.calls = nil
			in := &dspb.LookupAccountRoleRequest{PlayerId: 11}
			method, caller, stamp := "LookupAccountRole", "scene_manager", uint64(time.Now().UnixMilli())
			if mode == "wrong-purpose" {
				method = "CreateAccountRole"
			}
			if mode == "wrong-caller" {
				caller = "login"
			}
			if mode == "expired" {
				stamp = 1
			}
			require.NoError(t, serviceauth.Sign(accountdirectory.MethodPrefix+method, in, caller, sceneManagerSecret, stamp))
			if mode == "changed-body" {
				in.PlayerId++
			}
			if mode == "missing-key" {
				s.svcCtx.SceneManagerDirectoryAuth = nil
			} else {
				s.svcCtx.SceneManagerDirectoryAuth = verifier
			}
			out, err := s.LookupAccountRole(context.Background(), in)
			if mode == "valid" {
				require.NoError(t, err)
				require.Equal(t, uint64(7), out.OwnerRevision)
				require.Equal(t, []string{"Lookup"}, fake.calls)
			} else {
				require.Equal(t, codes.PermissionDenied, status.Code(err))
				require.Nil(t, out)
				require.Empty(t, fake.calls)
			}
		})
	}
	s.svcCtx.SceneManagerDirectoryAuth = verifier
	for _, rpc := range directoryRPCCases() {
		if rpc.name == "LookupAccountRole" {
			continue
		}
		t.Run("no-directory-write/"+rpc.name, func(t *testing.T) {
			in := rpc.request()
			require.NoError(t, serviceauth.Sign(accountdirectory.MethodPrefix+rpc.name, in, "scene_manager", sceneManagerSecret, uint64(time.Now().UnixMilli())))
			_, err := rpc.invoke(context.Background(), s, in)
			require.Equal(t, codes.PermissionDenied, status.Code(err))
		})
	}
	for _, rpc := range append(roleTradeRPCs(), roleAdmissionRPCs()...) {
		t.Run("no-role-write/"+rpc.method, func(t *testing.T) {
			in := rpc.request()
			require.NoError(t, serviceauth.Sign(accountdirectory.MethodPrefix+rpc.method, in, "scene_manager", sceneManagerSecret, uint64(time.Now().UnixMilli())))
			_, err := rpc.call(context.Background(), s, in)
			require.Equal(t, codes.PermissionDenied, status.Code(err))
		})
	}
}
