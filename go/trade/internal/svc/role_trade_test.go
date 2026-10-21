package svc

import (
	"context"
	"testing"

	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"

	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
)

type identitySessionReader struct {
	value string
	calls int
}

func (r *identitySessionReader) Get(context.Context, string) *redis.StringCmd {
	r.calls++
	return redis.NewStringResult(r.value, nil)
}

type identityDataClient struct {
	dspb.DataServiceClient
	owned *dspb.AccountRoleOwnership
	calls int
}

func (c *identityDataClient) LookupAccountRole(context.Context, *dspb.LookupAccountRoleRequest, ...grpc.CallOption) (*dspb.AccountRoleOwnership, error) {
	c.calls++
	return proto.Clone(c.owned).(*dspb.AccountRoleOwnership), nil
}
func TestOriginalSellerSessionCannotBorrowBuyerAccountOrRevisionAfterRoleTransfer(t *testing.T) {
	r := &identitySessionReader{value: "seller"}
	rpc := &identityDataClient{owned: &dspb.AccountRoleOwnership{Account: "buyer", OwnerRevision: 2, Role: &base.AccountSimplePlayer{PlayerId: 11}}}
	a := &RmbIdentity{Login: r, Directory: accountdirectory.NewTradeClient(rpc, "12345678901234567890123456789012")}
	in := &base.SessionDetails{PlayerId: 11, SessionId: 1, GateInstanceId: "original-gate", SessionIncarnation: 7, OwnerRevision: 1}
	_, err := a.Resolve(context.Background(), in, 0)
	require.Error(t, err)
	// ABA：账号又回到seller，修订仍必须等于gate发出时原值，不能按当前目录补成3。
	rpc.owned.Account = "seller"
	rpc.owned.OwnerRevision = 3
	_, err = a.Resolve(context.Background(), in, 0)
	require.Error(t, err)
	in.OwnerRevision = 3
	out, err := a.Resolve(context.Background(), in, 0)
	require.NoError(t, err)
	require.EqualValues(t, 3, out.OwnerRevision)
	before := rpc.calls
	in.OwnerRevision = 0
	_, err = a.Resolve(context.Background(), in, 0)
	require.Error(t, err)
	require.Equal(t, before, rpc.calls)
}
