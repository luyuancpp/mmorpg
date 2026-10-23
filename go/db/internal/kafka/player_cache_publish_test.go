package kafka

import (
	"context"
	"db/internal/metrics"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	componentpb "proto/common/component"
	dbpb "proto/db"
	"shared/assetop"
	"shared/savebarrier"
	"testing"
)

func fencePublishFixture(t *testing.T) (*worker, *orderingLease, *dbpb.DBTask) {
	t.Helper()
	w, _, _ := newTestWorker(t, 0)
	task := &dbpb.DBTask{Key: 42, Op: dbpb.DBOp_DB_OP_WRITE, MsgType: "player_database", OwnerEpoch: 7, SaveSeq: 9}
	lease, err := acquireOrderingLock(context.Background(), w, task)
	require.NoError(t, err)
	t.Cleanup(func() { releaseOrderingLock(context.Background(), lease, task) })
	return w, lease, task
}

func TestPlayerCachePublicationCannotOverwriteNewerVersion(t *testing.T) {
	w, lease, task := fencePublishFixture(t)
	rc := w.redisClient
	require.NoError(t, rc.Set(context.Background(), buildCacheKey(task), "newer", 0).Err())
	require.NoError(t, rc.Set(context.Background(), "distributed:lock:"+orderingLockKey(w.topic, task), "successor", 0).Err())
	before := metricCount(t, "db_ordering_lease_lost_total", map[string]string{"reason": metrics.LeaseLostOwnerChanged})
	require.ErrorIs(t, publishDBWriteResult(context.Background(), lease, task, fenceRow(3)), errOrderingLeaseLost)
	raw, err := rc.Get(context.Background(), buildCacheKey(task)).Result()
	require.NoError(t, err)
	require.Equal(t, "newer", raw)
	require.Equal(t, before+1, metricCount(t, "db_ordering_lease_lost_total", map[string]string{"reason": metrics.LeaseLostOwnerChanged}))
}

func TestPlayerCachePublicationEvidenceAndBarrierAreAtomic(t *testing.T) {
	require.NoError(t, SetFencingMode("dual"))
	t.Cleanup(func() { _ = SetFencingMode("legacy") })
	w, lease, task := fencePublishFixture(t)
	row := fenceRow(3)
	row.AssetOpLedger = &componentpb.PlayerAssetOpLedgerComp{}
	require.NoError(t, publishDBWriteResult(context.Background(), lease, task, row))
	proof := assetop.SQLCommitKey(task.Key)
	first, err := w.redisClient.HGet(context.Background(), proof, "data").Bytes()
	require.NoError(t, err)
	require.NoError(t, publishDBWriteResult(context.Background(), lease, task, row), "同版本同证据幂等")
	row.AssetOpLedger.Streams = []*componentpb.AssetOpStreamLedger{{Watermark: 3}}
	rawBefore, err := w.redisClient.Get(context.Background(), buildCacheKey(task)).Bytes()
	require.NoError(t, err)
	require.ErrorIs(t, publishDBWriteResult(context.Background(), lease, task, row), errFenceEvidenceConflict)
	rawAfter, err := w.redisClient.Get(context.Background(), buildCacheKey(task)).Bytes()
	require.NoError(t, err)
	require.Equal(t, rawBefore, rawAfter)
	after, err := w.redisClient.HGet(context.Background(), proof, "data").Bytes()
	require.NoError(t, err)
	require.Equal(t, first, after)
	version, err := w.redisClient.HGet(context.Background(), savebarrier.Key(task.Key), "player_database").Result()
	require.NoError(t, err)
	require.Equal(t, assetop.SQLCommitVersion(7, 9), version)
}

func TestAppliedAdvanceRejectsMalformedEpochBeforeCursorWrite(t *testing.T) {
	w, lease, task := fencePublishFixture(t)
	key := appliedEpochKey(w.topic, 42, task.MsgType)
	require.NoError(t, w.redisClient.Set(context.Background(), key, "01", 0).Err())
	wt := &workerTask{seq: 5, originPartition: 0, hasOriginPartition: true}
	require.Error(t, advanceApplied(context.Background(), lease, w.topic, task, wt))
	require.Zero(t, w.redisClient.Exists(context.Background(), appliedSeqKey(w.topic, 42, task.MsgType)).Val())
}

func TestReadCachePublicationNXPreservesConcurrentWrite(t *testing.T) {
	w, _, _ := newTestWorker(t, 0)
	task := &dbpb.DBTask{Key: 42, Op: dbpb.DBOp_DB_OP_READ, MsgType: "player_database", TaskId: "read-nx"}
	row := fenceRow(9)
	raw, err := proto.Marshal(row)
	require.NoError(t, err)
	require.NoError(t, w.redisClient.Set(context.Background(), buildCacheKey(task), raw, 0).Err())
	require.NoError(t, publishDBReadResult(context.Background(), w.redisClient, task, fenceRow(1)))
	actual, err := w.redisClient.Get(context.Background(), buildCacheKey(task)).Bytes()
	require.NoError(t, err)
	require.Equal(t, raw, actual)
	result, ok := readTaskResult(t, w, task.TaskId)
	require.True(t, ok)
	read := fenceRow(0)
	require.NoError(t, proto.Unmarshal(result.Data, read))
	require.Equal(t, uint64(1), read.Currency.Values[1], "SELECT应答不得换成NX赢家的缓存")
}

func TestPlayerCachePublicationAheadKeepsProofAndMonotoneBarrier(t *testing.T) {
	require.NoError(t, SetFencingMode("dual"))
	t.Cleanup(func() { _ = SetFencingMode("legacy") })
	w, lease, task := fencePublishFixture(t)
	proof := assetop.SQLCommitKey(task.Key)
	require.NoError(t, w.redisClient.HSet(w.ctx, proof, "version", savebarrier.Version(7, 10), "data", "newer-proof").Err())
	require.NoError(t, w.redisClient.HSet(w.ctx, savebarrier.Key(task.Key), "pending", savebarrier.Version(7, 10), "player_database", savebarrier.Version(7, 1)).Err())
	require.NoError(t, publishDBWriteResult(w.ctx, lease, task, fenceRow(3)))
	require.Equal(t, "newer-proof", w.redisClient.HGet(w.ctx, proof, "data").Val())
	require.Equal(t, savebarrier.Version(7, 10), w.redisClient.HGet(w.ctx, proof, "version").Val())
	require.Equal(t, savebarrier.Version(7, 9), w.redisClient.HGet(w.ctx, savebarrier.Key(task.Key), "player_database").Val())
	require.Equal(t, savebarrier.Version(7, 10), w.redisClient.HGet(w.ctx, savebarrier.Key(task.Key), "pending").Val())
	require.Equal(t, int64(1), w.redisClient.Exists(w.ctx, buildCacheKey(task)).Val())
}

func TestPlayerCachePublicationMalformedBarrierCannotPartiallyWrite(t *testing.T) {
	require.NoError(t, SetFencingMode("dual"))
	t.Cleanup(func() { _ = SetFencingMode("legacy") })
	w, lease, task := fencePublishFixture(t)
	require.NoError(t, w.redisClient.HSet(w.ctx, savebarrier.Key(task.Key), task.MsgType, "18446744073709551616:00000000000000000001").Err())
	require.Error(t, publishDBWriteResult(w.ctx, lease, task, fenceRow(3)))
	require.Zero(t, w.redisClient.Exists(w.ctx, buildCacheKey(task)).Val())
	require.Zero(t, w.redisClient.Exists(w.ctx, assetop.SQLCommitKey(task.Key)).Val())
	require.NoError(t, lease.lost(), "数据格式错误不能伪判令牌易主")
}

func TestAppliedAdvanceUsesExactUint64MaxWithoutEpochRegression(t *testing.T) {
	w, lease, task := fencePublishFixture(t)
	task.OwnerEpoch = ^uint64(0)
	require.NoError(t, advanceApplied(w.ctx, lease, w.topic, task, &workerTask{seq: 12, hasOriginPartition: true, originPartition: 0}))
	require.Equal(t, "18446744073709551615", w.redisClient.Get(w.ctx, appliedEpochKey(w.topic, task.Key, task.MsgType)).Val())
	task.OwnerEpoch--
	require.NoError(t, advanceApplied(w.ctx, lease, w.topic, task, &workerTask{seq: 13, hasOriginPartition: true, originPartition: 0}))
	require.Equal(t, "18446744073709551615", w.redisClient.Get(w.ctx, appliedEpochKey(w.topic, task.Key, task.MsgType)).Val())
}
