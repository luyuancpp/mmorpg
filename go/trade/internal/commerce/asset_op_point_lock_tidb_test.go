package commerce

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"testing"
	"time"

	assetpb "proto/common/asset"
	tradepb "proto/trade"
	"shared/assetop"

	"github.com/stretchr/testify/require"
)

// 复用 tiDBEngine 的随机库与强 TiDB 连接策略。等待图必须可读；缺 PROCESS 权限直接失败，
// 不把无法观察竞争退化为概率并发。KEY_INFO 的 PRIMARY + FOR UPDATE 同时确认等的是悲观点锁，
// 旧自动提交写或按 ref 的批改即使也等待同一行，也不能满足这条断言。
// 字段依据：https://docs.pingcap.com/tidb/dev/information-schema-data-lock-waits/
const pendingPrimaryLocks = `SELECT COUNT(*) FROM information_schema.data_lock_waits
 WHERE JSON_UNQUOTE(JSON_EXTRACT(key_info, '$.db_name')) = DATABASE()
 AND JSON_UNQUOTE(JSON_EXTRACT(key_info, '$.table_name')) = 'trade_asset_op'
 AND JSON_UNQUOTE(JSON_EXTRACT(key_info, '$.index_name')) = 'PRIMARY'
 AND LOWER(COALESCE(sql_digest_text, '')) LIKE '%for update%'`

type pointLockResult struct {
	name string
	err  error
}

// 在真正持有目标 op PRIMARY 期间启动生产入口，先在 TiKV 等待图里观察全部写者排队，再放锁。
// 所有 goroutine 的结果均在返回前收回；失败也先释放持锁事务和取消调用，不向下一例遗留竞争者。
func runBehindAssetOpPointLock(t *testing.T, e *Engine, opID uint64, calls map[string]func(context.Context) error) []pointLockResult {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	holder, err := e.Repo.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	require.NoError(t, err)
	defer holder.Rollback()
	var locked uint64
	require.NoError(t, holder.QueryRowContext(ctx, "SELECT op_id FROM trade_asset_op WHERE op_id=? FOR UPDATE", opID).Scan(&locked))
	require.Equal(t, opID, locked)
	done := make(chan pointLockResult, len(calls))
	for name, call := range calls {
		go func() { done <- pointLockResult{name: name, err: call(ctx)} }()
	}
	observeCtx, observeCancel := context.WithTimeout(ctx, 1500*time.Millisecond)
	var observed int
	var observeErr error
	for {
		if err := e.Repo.DB.QueryRowContext(observeCtx, pendingPrimaryLocks).Scan(&observed); err != nil {
			observeErr = fmt.Errorf("读取 TiDB 等待图: %w", err)
			break
		}
		if observed >= len(calls) {
			break
		}
		timer := time.NewTimer(10 * time.Millisecond)
		select {
		case <-observeCtx.Done():
			observeErr = fmt.Errorf("仅观察到 %d/%d 个 PRIMARY FOR UPDATE 等待者: %w", observed, len(calls), observeCtx.Err())
		case <-timer.C:
		}
		timer.Stop()
		if observeErr != nil {
			break
		}
	}
	observeCancel()
	rollbackErr := holder.Rollback()
	if observeErr != nil || rollbackErr != nil {
		cancel()
	}
	results := make([]pointLockResult, 0, len(calls))
	drain := time.NewTimer(5 * time.Second)
	defer drain.Stop()
	for len(results) < len(calls) {
		select {
		case result := <-done:
			results = append(results, result)
		case <-drain.C:
			t.Fatalf("生产入口取消后未退出，已收回 %d/%d 项；等待图错误=%v", len(results), len(calls), observeErr)
		}
	}
	require.NoError(t, observeErr)
	require.NoError(t, rollbackErr)
	return results
}

func pendingTradeDebit(t *testing.T, e *Engine) (uint64, assetop.Op) {
	t.Helper()
	ctx := context.Background()
	offer, err := e.CreatePlayerTrade(ctx, 11, &tradepb.CreatePlayerTradeRequest{
		ClientRequestId: "point_lock_trade", BuyerPlayerId: 22,
		SellerAssets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 2}},
		BuyerAssets:  &tradepb.TradeAssetSelection{CurrencyType: 1, CurrencyAmount: 70},
	})
	require.NoError(t, err)
	tick(t, e)
	_, workflow := state(t, e, offer.CommerceId)
	debit := leg(workflow, "seller_debit")
	require.NotNil(t, debit)
	now := e.now()
	op, claimed, err := e.Repo.Ops.Claim(ctx, debit.OpId, now, now+60_000, now+120_000, 99)
	require.NoError(t, err)
	require.True(t, claimed)
	return offer.CommerceId, op
}

func TestTiDBRescheduleAndFinalizeQueueOnPrimaryLock(t *testing.T) {
	e := tiDBEngine(t)
	_, op := pendingTradeDebit(t, e)
	now := e.now()
	results := runBehindAssetOpPointLock(t, e, op.OpID, map[string]func(context.Context) error{
		"重排": func(ctx context.Context) error {
			return e.Repo.Ops.Reschedule(ctx, op, now+60_000, assetop.Result{Local: true}, now)
		},
		"终结": func(ctx context.Context) error {
			ok, err := e.Repo.Ops.Finalize(ctx, op, assetop.StatusApplied, assetop.Result{
				Outcome: assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_APPLIED, Durable: true, SQLCommitted: true,
			}, now+1)
			if err == nil && !ok {
				return errors.New("未终结原待办")
			}
			return err
		},
	})
	for _, result := range results {
		if result.name == "重排" && errors.Is(result.err, assetop.ErrLeaseLost) {
			continue // 终结先拿到锁：过时重排必须落空。
		}
		require.NoError(t, result.err, result.name)
	}
	var status uint32
	var durable bool
	var next uint64
	require.NoError(t, e.Repo.DB.QueryRow("SELECT status,durable,next_attempt_ms FROM trade_asset_op WHERE op_id=?", op.OpID).Scan(&status, &durable, &next))
	require.Equal(t, uint32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED), status)
	require.True(t, durable)
	require.Equal(t, now+1, next, "过时重排不能把终态的清理时刻推迟")
}

func TestTiDBCommerceCancelAndConsumeQueueOnPrimaryLock(t *testing.T) {
	for _, action := range []string{"取消", "业务消费"} {
		t.Run(action, func(t *testing.T) {
			e := tiDBEngine(t)
			commerceID, op := pendingTradeDebit(t, e)
			results := runBehindAssetOpPointLock(t, e, op.OpID, map[string]func(context.Context) error{
				action: func(ctx context.Context) error {
					if action == "取消" {
						_, err := e.Mutate(ctx, 11, commerceID, "cancel")
						return err
					}
					return e.Repo.Tx(ctx, func(tx *sql.Tx) error {
						if _, err := e.Repo.Lock(ctx, tx, commerceID); err != nil {
							return err
						}
						return e.Repo.Ops.MarkCommerceOpsConsumedTx(ctx, tx, commerceID)
					})
				},
			})
			require.NoError(t, results[0].err)
			var status uint32
			var deadline uint64
			var consumed bool
			require.NoError(t, e.Repo.DB.QueryRow("SELECT status,deadline_ms,business_consumed FROM trade_asset_op WHERE op_id=?", op.OpID).Scan(&status, &deadline, &consumed))
			require.Equal(t, uint32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING), status)
			if action == "取消" {
				require.LessOrEqual(t, deadline, e.now())
				r, workflow := state(t, e, commerceID)
				require.Equal(t, CancelPending, r.Status)
				require.True(t, workflow.CancelRequested)
			} else {
				require.True(t, consumed)
			}
		})
	}
}
