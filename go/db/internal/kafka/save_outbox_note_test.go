package kafka

import (
	"context"
	"errors"
	"fmt"
	"testing"

	db_config "db/internal/config"
	"db/internal/dbguard"
	"db/internal/metrics"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	playerpb "proto/common/database"
	"shared/savebarrier"
)

// 消费侧给补投器的提示(save_outbox_note.go):走真实的 deferFailedTask / quarantineOrderingConflict 入口,
// 确定性拒写给当前意图打终态,可自愈的延后只记在途重试;legacy、旧版本、非屏障表一律不碰屏障。

func strictPlayerWriteTask(t *testing.T, key, epoch, seq uint64) *workerTask {
	t.Helper()
	wt := makeWriteTask(t, key, playerDatabaseMsgType, 10)
	body, err := proto.Marshal(&playerpb.PlayerDatabase{PlayerId: key})
	require.NoError(t, err)
	wt.dbTask.Body, wt.dbTask.OwnerEpoch, wt.dbTask.SaveSeq = body, epoch, seq
	return wt
}

func seedPendingIntent(t *testing.T, w *worker, key, epoch, seq uint64) {
	t.Helper()
	v := savebarrier.Version(epoch, seq)
	require.NoError(t, w.redisClient.HSet(context.Background(), savebarrier.Key(key), "pending", v, "outbox_version", v,
		"outbox_topic", "db_task_zone_1", "outbox_player_database", "a", "outbox_player_database_1", "b").Err())
}

func barrierField(t *testing.T, w *worker, key uint64, field string) (string, bool) {
	t.Helper()
	v, err := w.redisClient.HGet(context.Background(), savebarrier.Key(key), field).Result()
	if err != nil {
		return "", false
	}
	return v, true
}

func useDualFencing(t *testing.T) {
	t.Helper()
	require.NoError(t, SetFencingMode(db_config.FencingModeDual))
	t.Cleanup(func() { _ = SetFencingMode(db_config.FencingModeLegacy) })
}

func TestQuarantineMarksSaveOutboxTerminal(t *testing.T) {
	useDualFencing(t)
	w, _, _ := newTestWorker(t, 0)
	const key uint64 = 9601
	seedPendingIntent(t, w, key, 7, 9)
	wt := strictPlayerWriteTask(t, key, 7, 9)
	w.quarantineOrderingConflict(wt, wt.dbTask, metrics.DeadLetterFenceStaleOwner, "test stale owner")
	v, ok := barrierField(t, w, key, "outbox_terminal")
	require.True(t, ok)
	require.Equal(t, savebarrier.Version(7, 9), v)
	reason, _ := barrierField(t, w, key, "outbox_terminal_reason")
	require.Equal(t, metrics.DeadLetterFenceStaleOwner, reason)
	require.Equal(t, int64(1), redisListLen(t, w, w.retryDeadQueueKey), "死信照常保全载荷")
}

func TestDeferralMarksDeterministicRejectTerminalAndStampsTransientRetry(t *testing.T) {
	useDualFencing(t)
	t.Run("blob gate is terminal", func(t *testing.T) {
		w, _, _ := newTestWorker(t, 0)
		const key uint64 = 9602
		seedPendingIntent(t, w, key, 7, 9)
		wt := strictPlayerWriteTask(t, key, 7, 9)
		w.deferFailedTask(wt, wt.dbTask, fmt.Errorf("db write rejected by blob size gate: %w", &dbguard.ErrBlobTooLarge{}))
		reason, ok := barrierField(t, w, key, "outbox_terminal_reason")
		require.True(t, ok)
		require.Equal(t, "blob_too_large", reason)
		_, stamped := barrierField(t, w, key, "outbox_retry_ms")
		require.False(t, stamped, "终态不再记在途重试")
	})
	t.Run("asset evidence conflict is terminal", func(t *testing.T) {
		w, _, _ := newTestWorker(t, 0)
		const key uint64 = 9603
		seedPendingIntent(t, w, key, 7, 9)
		wt := strictPlayerWriteTask(t, key, 7, 9)
		w.deferFailedTask(wt, wt.dbTask, fmt.Errorf("publish: %w", errFenceEvidenceConflict))
		reason, _ := barrierField(t, w, key, "outbox_terminal_reason")
		require.Equal(t, "asset_evidence_conflict", reason)
	})
	t.Run("transient failure only stamps inflight retry", func(t *testing.T) {
		w, _, _ := newTestWorker(t, 0)
		const key uint64 = 9604
		seedPendingIntent(t, w, key, 7, 9)
		wt := strictPlayerWriteTask(t, key, 7, 9)
		w.deferFailedTask(wt, wt.dbTask, errors.New("transient store failure"))
		_, terminal := barrierField(t, w, key, "outbox_terminal")
		require.False(t, terminal, "可自愈的失败不得打终态")
		_, stamped := barrierField(t, w, key, "outbox_retry_ms")
		require.True(t, stamped)
	})
}

func TestSaveOutboxNotesIgnoreNonBarrierWrites(t *testing.T) {
	cases := map[string]func(t *testing.T, w *worker) *workerTask{
		"older version than current intent": func(t *testing.T, w *worker) *workerTask {
			useDualFencing(t)
			return strictPlayerWriteTask(t, 9610, 7, 8)
		},
		"legacy mode": func(t *testing.T, w *worker) *workerTask {
			require.NoError(t, SetFencingMode(db_config.FencingModeLegacy))
			return strictPlayerWriteTask(t, 9610, 7, 9)
		},
		"non barrier table": func(t *testing.T, w *worker) *workerTask {
			useDualFencing(t)
			wt := strictPlayerWriteTask(t, 9610, 7, 9)
			wt.dbTask.MsgType = placementTestMsgType
			return wt
		},
		"epoch zero compat write": func(t *testing.T, w *worker) *workerTask {
			useDualFencing(t)
			return strictPlayerWriteTask(t, 9610, 0, 0)
		},
	}
	for name, build := range cases {
		t.Run(name, func(t *testing.T) {
			w, _, _ := newTestWorker(t, 0)
			seedPendingIntent(t, w, 9610, 7, 9)
			wt := build(t, w)
			w.quarantineOrderingConflict(wt, wt.dbTask, metrics.DeadLetterFenceStaleOwner, "test")
			w.deferFailedTask(wt, wt.dbTask, errors.New("transient"))
			_, terminal := barrierField(t, w, 9610, "outbox_terminal")
			require.False(t, terminal)
			if name != "older version than current intent" {
				_, stamped := barrierField(t, w, 9610, "outbox_retry_ms")
				require.False(t, stamped)
			}
		})
	}
}

func TestDeterministicWriteRejectReasonIsClosedSet(t *testing.T) {
	for _, cause := range []error{errOrderingLockBusy, errOrderingLeaseLost, errPlacementFrozen, errors.New("x")} {
		_, ok := deterministicWriteRejectReason(fmt.Errorf("wrapped: %w", cause))
		require.False(t, ok, "%v 可能自愈,不是确定性拒写", cause)
	}
}
