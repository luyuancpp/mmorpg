package roleidentity

import (
	"context"
	"github.com/stretchr/testify/require"
	base "proto/common/base"
	dspb "proto/data_service"
	"testing"
)

func TestCapturedOwnershipDoesNotChangeWhenCallerMutatesReturnedOrOriginalProto(t *testing.T) {
	v := &dspb.AccountRoleOwnership{Account: "seller", OwnerRevision: 7, Role: &base.AccountSimplePlayer{PlayerId: 11}}
	ctx, err := Capture(context.Background(), v)
	require.NoError(t, err)
	v.Account = "buyer"
	v.OwnerRevision = 9
	first, ok := Original(ctx)
	require.True(t, ok)
	require.Equal(t, "seller", first.Account)
	require.EqualValues(t, 7, first.OwnerRevision)
	first.OwnerRevision = 10
	second, _ := Original(ctx)
	require.EqualValues(t, 7, second.OwnerRevision)
}
func TestCaptureRejectsZeroRevisionAndMissingAccount(t *testing.T) {
	for _, v := range []*dspb.AccountRoleOwnership{nil, {Account: "seller", Role: &base.AccountSimplePlayer{PlayerId: 11}}, {OwnerRevision: 1, Role: &base.AccountSimplePlayer{PlayerId: 11}}} {
		_, err := Capture(context.Background(), v)
		require.Error(t, err)
	}
}
