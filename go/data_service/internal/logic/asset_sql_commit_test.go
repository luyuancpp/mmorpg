package logic

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	dbpb "proto/common/database"
	"shared/assetop"
)

func TestSQLAssetProofNeverFallsBackToRedisSnapshot(t *testing.T) {
	svcCtx, server := newTestSvcCtx(t)
	want := sampleLedger()
	putPlayerBlob(t, server, ledgerTestKey(ledgerTestPlayerID), &dbpb.PlayerAllData{PlayerDatabaseData: &dbpb.PlayerDatabase{PlayerId: ledgerTestPlayerID, AssetOpLedger: want}})
	got, found, err := GetSQLCommittedAssetLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	require.False(t, found)
	require.Nil(t, got)
	ledger, err := proto.Marshal(want)
	require.NoError(t, err)
	commit := &assetpb.PlayerAssetSQLCommit{PlayerId: ledgerTestPlayerID, OwnerEpoch: 10, SaveSeq: 1, LedgerProto: ledger}
	raw, err := proto.Marshal(commit)
	require.NoError(t, err)
	server.HSet(assetop.SQLCommitKey(ledgerTestPlayerID), "data", string(raw))
	got, found, err = GetSQLCommittedAssetLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.NoError(t, err)
	require.True(t, found)
	require.True(t, proto.Equal(want, got))
	commit.PlayerId++
	raw, err = proto.Marshal(commit)
	require.NoError(t, err)
	server.HSet(assetop.SQLCommitKey(ledgerTestPlayerID), "data", string(raw))
	_, _, err = GetSQLCommittedAssetLedger(context.Background(), svcCtx, ledgerTestPlayerID)
	require.Equal(t, codes.Internal, status.Code(err))
}
