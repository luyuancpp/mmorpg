package commerce

// tick_isolation_db_test.go —— 第一轮审计 §2.5 go-asset-services#0 / #1 的真库回归(2026-10-20)。
//
// 跑法(二选一;每例新建随机库、结束删库,只能指向一次性实例,账号要能 CREATE / DROP DATABASE):
//   - 一次性 MySQL:COMMERCE_TEST_MYSQL_DSN='root:<密码>@tcp(127.0.0.1:<端口>)/'  COMMERCE_REQUIRE_MYSQL=1
//   - 一次性 TiDB :COMMERCE_TEST_DSN=...(与 engine_tidb_test.go 同一门控)     COMMERCE_REQUIRE_TIDB=1
// 两个 DSN 都设时用 MySQL。都没设时整组 Skip —— Skip 不代表通过,验收必须带 REQUIRE 开关并在 -v 里逐条看到 PASS。
// 资产终局由夹具或生产写路径(Claim → Abandon、ResolveManually)直接落在 outbox 上,验证的是交易 SQL 状态机,
// 不冒充 scene 端到端。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	assetpb "proto/common/asset"
	componentpb "proto/common/component"
	tradepb "proto/trade"
	"shared/assetop"
	"trade/internal/data"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// realDBEngine 给本文件的用例选一个真库:优先一次性 MySQL,否则沿用 TiDB 门控(tiDBEngine)。
func realDBEngine(t *testing.T) *Engine {
	t.Helper()
	if dsn := os.Getenv("COMMERCE_TEST_MYSQL_DSN"); dsn != "" {
		return openEngineDB(t, dsn, false)
	}
	if os.Getenv("COMMERCE_REQUIRE_MYSQL") == "1" {
		t.Fatal("缺少 COMMERCE_TEST_MYSQL_DSN")
	}
	return tiDBEngine(t)
}

// testClock 是可拨动的引擎时钟(Engine.Now):推迟到期只靠拨钟,不睡墙钟。
// 库侧的 CURRENT_TIMESTAMP(截止判定)仍是真时钟,用例里的截止时间都留足一小时,拨钟两分钟不跨越它。
type testClock struct{ ms atomic.Int64 }

func newTestClock() *testClock {
	c := &testClock{}
	c.ms.Store(time.Now().UnixMilli())
	return c
}
func (c *testClock) Now() time.Time          { return time.UnixMilli(c.ms.Load()) }
func (c *testClock) Advance(d time.Duration) { c.ms.Add(d.Milliseconds()) }

func consignment(t *testing.T, e *Engine, seller uint64, requestID string) *tradepb.TradeCommerceRecord {
	t.Helper()
	r, err := e.CreateConsignment(context.Background(), seller, &tradepb.CreateConsignmentRequest{
		ClientRequestId: requestID, Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 2}},
		CurrencyType: 1, Price: 50, DurationMs: 3_600_000,
	})
	require.NoError(t, err)
	return r
}

// fillPendingDebits 走生产路径(EnsureSeqRows → AllocateSeqsTx → InsertOp)给 player 的 TRADE_DEBIT 流塞 n 条未决行。
// ref_kind=LISTING:不挂在任何合同上,用例可以单独终结它们。
func fillPendingDebits(t *testing.T, e *Engine, player uint64, n int) {
	t.Helper()
	ctx := context.Background()
	key := data.SeqKey{PlayerID: player, Stream: assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT}
	require.NoError(t, e.Repo.Ops.EnsureSeqRows(ctx, e.now(), key))
	payload, err := proto.Marshal(&assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: 10, Count: 1}}})
	require.NoError(t, err)
	for i := 0; i < n; i++ {
		opID, err := e.OpIDs.Next(ctx)
		require.NoError(t, err)
		require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error {
			alloc, err := e.Repo.Ops.AllocateSeqsTx(ctx, tx, e.now(), key)
			if err != nil {
				return err
			}
			a := alloc[key]
			return e.Repo.Ops.InsertOp(ctx, tx, &tradepb.TradeAssetOpRecord{
				OpId: opID, PlayerId: player, Stream: uint32(key.Stream), StreamEpoch: a.Epoch, Seq: a.Seq,
				Kind: tradepb.TradeAssetOpKind_TRADE_ASSET_OP_KIND_ESCROW_DEBIT, Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING,
				NextAttemptMs: e.now(), RefKind: tradepb.TradeAssetOpRefKind_TRADE_ASSET_OP_REF_KIND_LISTING, RefId: 7000 + uint64(i),
				Payload: payload, CreatedMs: e.now(), UpdatedMs: e.now(),
			})
		}))
	}
}

func dueIDs(t *testing.T, e *Engine) map[uint64]bool {
	t.Helper()
	rows, err := e.Repo.Due(context.Background(), e.now(), tickDueLimit)
	require.NoError(t, err)
	out := make(map[uint64]bool, len(rows))
	for _, r := range rows {
		out[r.CommerceId] = true
	}
	return out
}

func opCount(t *testing.T, e *Engine, player uint64, stream uint32) int {
	t.Helper()
	var n int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE player_id=? AND stream=?", player, stream).Scan(&n))
	return n
}

// buyerDebit 取寄售购买的扣款腿(角色名 buyer_debit_<序号>_<请求键>)。
func buyerDebit(w *tradepb.TradeCommerceWorkflow) *tradepb.TradeCommerceLeg {
	for _, l := range w.Legs {
		if strings.HasPrefix(l.Role, "buyer_debit_") {
			return l
		}
	}
	return nil
}

// abandonOp 走生产的自动放弃写路径:Claim 领租约 → Abandon 带令牌 CAS(刻意不置 durable)。
// 放弃之前的六条证据(玩家无位置、账本确认未见……)由 assetop.Loop.AbandonOnce 复核,不在本文件范围。
func abandonOp(t *testing.T, e *Engine, opID uint64) {
	t.Helper()
	ctx := context.Background()
	now := e.now()
	op, claimed, err := e.Repo.Ops.Claim(ctx, opID, now, now+60_000, now+120_000, 77)
	require.NoError(t, err)
	require.True(t, claimed)
	require.NoError(t, e.Repo.Ops.Abandon(ctx, op, now))
	var status int32
	var durable bool
	require.NoError(t, e.Repo.DB.QueryRow("SELECT status,durable FROM trade_asset_op WHERE op_id=?", opID).Scan(&status, &durable))
	require.EqualValues(t, tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED, status)
	require.False(t, durable, "放弃不置 durable:修复前合同正是因此读不到它")
}

// 审计 #0 的原始触发:队头合同撞上 ErrTooManyPending(对方离线攒满 16 条未决)。
// 修复前 Tick 在这一行整轮返回、updated_ms 不变,后面的合同每一轮都轮不到。
func TestRealDBTickFailingHeadRowDoesNotBlockOthers(t *testing.T) {
	e := realDBEngine(t)
	clock := newTestClock()
	e.Now = clock.Now
	fillPendingDebits(t, e, 11, int(assetop.DefaultLimits.MaxPending))
	head := consignment(t, e, 11, "blocked-head-01")
	var others []uint64
	for p := uint64(12); p <= 14; p++ {
		clock.Advance(time.Millisecond) // 让队头的 updated_ms 严格最早
		others = append(others, consignment(t, e, p, fmt.Sprintf("healthy-offer-%d", p)).CommerceId)
	}

	tick(t, e)
	for _, id := range others {
		_, w := state(t, e, id)
		debit := leg(w, "seller_debit")
		require.NotNil(t, debit, "队头失败不能挡住后面的合同")
		require.NotZero(t, debit.OpId)
	}
	hr, hw := state(t, e, head.CommerceId)
	require.Nil(t, leg(hw, "seller_debit"), "失败那一轮的合同事务整体回滚,不留半截托管")
	require.Equal(t, head.Version, hr.Version, "推迟不是业务变化,不改版本")
	nowMs := e.now()
	pendingMs := uint64(tickPendingBackoff.Milliseconds())
	require.GreaterOrEqual(t, hr.UpdatedMs, nowMs+pendingMs*8/10, "失败行按积压背压的退避推迟")
	require.LessOrEqual(t, hr.UpdatedMs, nowMs+pendingMs*12/10, "推迟有界")
	require.False(t, dueIDs(t, e)[head.CommerceId], "推迟期内 Due 不再把它排在队头")

	tick(t, e)
	again, _ := state(t, e, head.CommerceId)
	require.Equal(t, hr.UpdatedMs, again.UpdatedMs, "推迟期内不重试")
	require.Equal(t, hr.Version, again.Version)

	// 积压消化(那 16 条终结)且推迟到期之后照常推进:推迟不是丢弃。
	_, err := e.Repo.DB.Exec("UPDATE trade_asset_op SET status=3,durable=1 WHERE ref_kind=1 AND status=1")
	require.NoError(t, err)
	clock.Advance(2 * time.Minute)
	tick(t, e)
	_, hw = state(t, e, head.CommerceId)
	require.NotNil(t, leg(hw, "seller_debit"))
	require.NotZero(t, leg(hw, "seller_debit").OpId)
}

// 合同自身数据坏了(解不开 / plan 判错)是确定性失败:隔离为 Manual 并移出 Due,不按暂时失败无限重试,也不挡其余合同。
func TestRealDBTickQuarantinesCorruptContracts(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	now := e.now()
	garbage := []byte{0xff, 0xff, 0xff} // 不完整的 varint,proto 必解不开
	refund, err := proto.MarshalOptions{Deterministic: true}.Marshal(&tradepb.TradeCommerceWorkflow{Stage: "bid_refund", SellerIdentity: &tradepb.TradeRoleIdentity{Account: "account-32", OwnerRevision: 1}})
	require.NoError(t, err)
	undecodable := &tradepb.TradeCommerceRecord{CommerceId: 900001, RequestKey: "corrupt-undecodable", RequestDigest: []byte{1},
		Kind: 3, Status: Escrowing, SellerPlayerId: 31, ExpiresMs: now + 3_600_000, CreatedMs: now - 2000, UpdatedMs: now - 2000,
		Version: 1, WorkflowProto: garbage}
	// 退款阶段却没有已提交的冻结扣款:plan 判"竞价退款缺已提交扣款"。
	contradictory := &tradepb.TradeCommerceRecord{CommerceId: 900002, RequestKey: "corrupt-plan", RequestDigest: []byte{2},
		Kind: BidKind, Status: CancelPending, SellerPlayerId: 32, CreatedMs: now - 1000, UpdatedMs: now - 1000,
		Version: 1, WorkflowProto: refund}
	for _, r := range []*tradepb.TradeCommerceRecord{undecodable, contradictory} {
		require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error { return e.Repo.Insert(ctx, tx, r) }))
	}
	healthy := consignment(t, e, 33, "healthy-behind-corrupt")

	tick(t, e)
	_, hw := state(t, e, healthy.CommerceId)
	require.NotNil(t, leg(hw, "seller_debit"), "坏数据行不能挡住后面的合同")

	bad, err := e.Repo.Get(ctx, undecodable.CommerceId)
	require.NoError(t, err)
	require.Equal(t, Manual, bad.Status, "解不开的合同隔离为人工")
	require.Equal(t, garbage, bad.WorkflowProto, "坏字节原样留作取证")
	require.EqualValues(t, 2, bad.Version)
	pr, pw := state(t, e, contradictory.CommerceId)
	require.Equal(t, Manual, pr.Status)
	require.Equal(t, "manual", pw.Stage, "stage 改成 manual:玩家操作与撮合都不会把它拉回推进")
	require.EqualValues(t, 2, pr.Version)
	require.Zero(t, opCount(t, e, 32, 4), "隔离不发出任何资产指令")

	tick(t, e)
	for _, id := range []uint64{undecodable.CommerceId, contradictory.CommerceId} {
		r, err := e.Repo.Get(ctx, id)
		require.NoError(t, err)
		require.EqualValues(t, 2, r.Version, "隔离之后不再被 Due 取到")
	}
}

// 审计 #1 的原始场景:买家点击购买后下线不回来,扣款腿被自动放弃(durable 保持 0)。
// 修复前合同永远停在 pay,卖家托管的资产被扣作人质;现在放弃 = scene 从未扣过买家,寄售重新开放,
// 之后到期下架只退回卖家一次,买家不收任何"退款"。
func TestRealDBAbandonedBuyerDebitReleasesSellerEscrowOnce(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	offer := consignment(t, e, 11, "abandon-offer-01")
	tick(t, e)
	settleLegs(t, e, 2)
	tick(t, e)
	_, w := state(t, e, offer.CommerceId)
	require.Equal(t, "open", w.Stage)
	_, err := e.Mutate(ctx, 22, offer.CommerceId, "buy", "abandon-buy-0001")
	require.NoError(t, err)
	tick(t, e)
	_, w = state(t, e, offer.CommerceId)
	require.Equal(t, "pay", w.Stage)
	debit := buyerDebit(w)
	require.NotNil(t, debit)
	require.NotZero(t, debit.OpId)

	abandonOp(t, e, debit.OpId)

	tick(t, e)
	r, w := state(t, e, offer.CommerceId)
	require.Equal(t, "open", w.Stage, "放弃的扣款按没扣到处理:寄售重新开放,卖家托管不再被扣作人质")
	require.Zero(t, r.BuyerPlayerId)
	require.Equal(t, Escrowing, r.Status)
	require.EqualValues(t, tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED, buyerDebit(w).Outcome)
	require.Zero(t, opCount(t, e, 22, 4), "买家从未被扣,不给买家发任何退款")

	// 到期下架:退回卖家的 credit 只建一次,反复推进不再加。
	_, err = e.Repo.DB.Exec("UPDATE trade_commerce SET expires_ms=1 WHERE commerce_id=?", offer.CommerceId)
	require.NoError(t, err)
	for i := 0; i < 3; i++ {
		tick(t, e)
	}
	r, w = state(t, e, offer.CommerceId)
	require.Equal(t, "return", w.Stage)
	require.Equal(t, CancelPending, r.Status)
	require.Equal(t, 1, opCount(t, e, 11, 4))
	settleLegs(t, e, 2)
	tick(t, e)
	tick(t, e)
	r, _ = state(t, e, offer.CommerceId)
	require.Equal(t, Cancelled, r.Status)
	require.Equal(t, 1, opCount(t, e, 11, 4), "结案后不再补偿")
	require.Equal(t, 1, opCount(t, e, 22, 3), "被放弃的那条 seq 不重发")
}

// 人工终结同样不置 durable。修复前人工终结过的腿能不能被合同读到,取决于它此前某次重排有没有恰好写过 durable=1。
func TestRealDBManuallyResolvedLegsAdvanceContracts(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	resolve := func(opID uint64, final assetop.Status) {
		t.Helper()
		ok, err := assetop.ResolveManually(ctx, e.Repo.Ops, assetop.ManualResolution{
			OpID: opID, Final: final, Operator: "ops-regression", Reason: "按 scene 流水 correlation_id 判定",
		}, nil, time.Now)
		require.NoError(t, err)
		require.True(t, ok)
	}

	// 寄售:卖家托管人工判定成功(堆叠材料,不需要托管原件)→ 上架。
	offer := consignment(t, e, 41, "manual-offer-01")
	tick(t, e)
	_, w := state(t, e, offer.CommerceId)
	resolve(leg(w, "seller_debit").OpId, assetop.StatusApplied)
	tick(t, e)
	_, w = state(t, e, offer.CommerceId)
	require.Equal(t, "open", w.Stage)

	// 双方交易:买家托管人工判定中止 → 只退卖家一次,不给买家发货也不给买家退款。
	trade, err := e.CreatePlayerTrade(ctx, 51, &tradepb.CreatePlayerTradeRequest{ClientRequestId: "manual-trade-01", BuyerPlayerId: 52,
		SellerAssets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 2}},
		BuyerAssets:  &tradepb.TradeAssetSelection{CurrencyType: 1, CurrencyAmount: 70}})
	require.NoError(t, err)
	tick(t, e)
	_, w = state(t, e, trade.CommerceId)
	resolve(leg(w, "seller_debit").OpId, assetop.StatusApplied)
	tick(t, e)
	_, err = e.Mutate(ctx, 52, trade.CommerceId, "confirm")
	require.NoError(t, err)
	tick(t, e)
	_, w = state(t, e, trade.CommerceId)
	require.Equal(t, "buyer_escrow", w.Stage)
	resolve(leg(w, "buyer_debit").OpId, assetop.StatusAborted)
	for i := 0; i < 3; i++ {
		tick(t, e)
	}
	r, w := state(t, e, trade.CommerceId)
	require.Equal(t, "return", w.Stage)
	require.Equal(t, CancelPending, r.Status)
	require.NotNil(t, leg(w, "seller_return"))
	require.Nil(t, leg(w, "buyer_return"))
	require.Nil(t, leg(w, "buyer_receive"))
	require.Equal(t, 1, opCount(t, e, 51, 4))
	require.Zero(t, opCount(t, e, 52, 4))
}

// 人工判定"实例扣款已成功"时 outbox 行没有托管原件(ResolveManually 不写 escrow_snapshot;自动路径缺原件根本不终结)。
// 合同必须在记下结局时就隔离为人工,不能先上架、等买家付了款才在发货时撞错,把第二个玩家的钱也压进人工单。
func TestRealDBManualAppliedInstanceDebitWithoutOriginalIsQuarantined(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	e.Policy.AllowInstances = true
	offer, err := e.CreateConsignment(ctx, 81, &tradepb.CreateConsignmentRequest{ClientRequestId: "manual-instance-01",
		Assets: &tradepb.TradeAssetSelection{ItemUuids: []uint64{77}}, CurrencyType: 1, Price: 50, DurationMs: 3_600_000})
	require.NoError(t, err)
	tick(t, e)
	_, w := state(t, e, offer.CommerceId)
	debit := leg(w, "seller_debit")
	require.NotNil(t, debit)
	ok, err := assetop.ResolveManually(ctx, e.Repo.Ops, assetop.ManualResolution{
		OpID: debit.OpId, Final: assetop.StatusApplied, Operator: "ops-regression", Reason: "scene 流水显示已扣出",
	}, nil, time.Now)
	require.NoError(t, err)
	require.True(t, ok)

	tick(t, e)
	r, w := state(t, e, offer.CommerceId)
	require.Equal(t, Manual, r.Status, "没有托管原件就不能上架")
	require.Equal(t, "manual", w.Stage)
	require.Zero(t, opCount(t, e, 81, 4), "隔离不发出任何退回")
	require.False(t, dueIDs(t, e)[offer.CommerceId])
	_, err = e.Mutate(ctx, 82, offer.CommerceId, "buy", "manual-instance-buy-1")
	require.ErrorIs(t, err, ErrConflict, "隔离后的寄售不接受购买")
}

// bid_ready 的出价还没被拍品接受,拍品不引用它。拍品行缺失(或解不开)时这条出价按不合格原币退回,
// 不能像以前那样每轮返回错误、冻结的钱永远退不回来;退款腿只建一次。
func TestRealDBReadyBidWithMissingLotIsRefundedOnce(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	now := e.now()
	sel := &tradepb.TradeAssetSelection{CurrencyType: 1, CurrencyAmount: 60}
	raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(&tradepb.TradeCommerceWorkflow{Stage: "bid_ready", SellerAssets: sel, SellerIdentity: &tradepb.TradeRoleIdentity{Account: "account-71", OwnerRevision: 1},
		Legs: []*tradepb.TradeCommerceLeg{{Role: "bid_debit", PlayerId: 71, Stream: 3, Bundle: Bundle(sel), OpId: 424242, Outcome: 2}}})
	require.NoError(t, err)
	bid := &tradepb.TradeCommerceRecord{CommerceId: 900101, RequestKey: "orphan-ready-bid", RequestDigest: []byte{3}, Kind: BidKind,
		Status: Escrowing, SellerPlayerId: 71, ParentId: 999999, UnitPrice: 60, CurrencyType: 1,
		CreatedMs: now - 1000, UpdatedMs: now - 1000, Version: 1, WorkflowProto: raw}
	require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error { return e.Repo.Insert(ctx, tx, bid) }))

	tick(t, e)
	r, w := state(t, e, bid.CommerceId)
	require.Equal(t, "bid_refund", w.Stage)
	require.Equal(t, CancelPending, r.Status)
	for i := 0; i < 3; i++ {
		tick(t, e)
	}
	_, w = state(t, e, bid.CommerceId)
	require.Equal(t, "return", w.Stage)
	refund := leg(w, "bid_return")
	require.NotNil(t, refund)
	require.EqualValues(t, 60, refund.Bundle.Currencies[0].Amount)
	require.Equal(t, 1, opCount(t, e, 71, 4), "原币退款只建一次")
}

// 持有中的竞价在拍品结算前无事可做。修复前它的 updated_ms 不变、每轮都排在 Due 队头,
// 有出价的在拍拍品数一到 Due 上限,Due 就只回这些行,拍品自己也推进不了。
func TestRealDBHeldBidWaitsOutsideDueHead(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	lot, err := e.CreateLot(ctx, 11, &tradepb.CreateAuctionLotRequest{ClientRequestId: "held-lot-0001",
		Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}, CurrencyType: 1, OpeningPrice: 50, MinimumIncrement: 5, DurationMs: 3_600_000})
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 2)
	tick(t, e)
	placeHeldBid := func(player, amount uint64, requestID string) *tradepb.TradeCommerceRecord {
		t.Helper()
		bid, err := e.PlaceBid(ctx, player, lot.CommerceId, &tradepb.PlaceAuctionBidRequest{LotId: fmt.Sprint(lot.CommerceId), ClientRequestId: requestID, Amount: amount})
		require.NoError(t, err)
		tick(t, e)
		settleLegs(t, e, 2)
		tick(t, e)
		tick(t, e)
		_, bw := state(t, e, bid.CommerceId)
		require.Equal(t, "bid_held", bw.Stage)
		return bid
	}
	first := placeHeldBid(21, 60, "held-bid-0001")
	before, _ := state(t, e, first.CommerceId)

	tick(t, e)
	held, _ := state(t, e, first.CommerceId)
	lr, _ := state(t, e, lot.CommerceId)
	require.Equal(t, lr.ExpiresMs, held.UpdatedMs, "推迟到拍品截止:之前它不可能结算")
	require.Equal(t, before.Version, held.Version, "推迟不改版本")
	require.False(t, dueIDs(t, e)[first.CommerceId])

	// 被超价时照常立刻回到推进:新竞价接受时把它改为 bid_refund,Save 把 updated_ms 写回现在。
	placeHeldBid(22, 70, "held-bid-0002")
	_, fw := state(t, e, first.CommerceId)
	require.Equal(t, "bid_refund", fw.Stage)
	require.True(t, dueIDs(t, e)[first.CommerceId])
	tick(t, e)
	require.Equal(t, 1, opCount(t, e, 21, 4), "被超价者原币退款待办")
}

// recordingApplier 记录收据 ACK 尝试过哪些 op(correlation_id = op_id),一律回本地 NOT_HERE(卖家离线)。
type recordingApplier struct {
	mu   sync.Mutex
	seen []uint64
}

func (a *recordingApplier) Do(_ context.Context, _ assetop.RPC, req *assetpb.AssetOpRequest) (assetop.Result, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.seen = append(a.seen, req.GetCorrelationId())
	return assetop.Result{Outcome: assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_NOT_HERE, Local: true, Absent: true}, nil
}

func (a *recordingApplier) attempts() []uint64 {
	a.mu.Lock()
	defer a.mu.Unlock()
	return append([]uint64(nil), a.seen...)
}

func (a *recordingApplier) distinct() map[uint64]bool {
	out := map[uint64]bool{}
	for _, id := range a.attempts() {
		out[id] = true
	}
	return out
}

// emptyLedger 实现 SQLCommitLedgerReader:AcknowledgeReceipt 要求账本读取器能读 SQL 提交证据。
// 本用例的 ACK 在 Applier 那一步就回 NOT_HERE,走不到读账本。
type emptyLedger struct{}

func (emptyLedger) ReadPersistedLedger(context.Context, uint64) (*componentpb.PlayerAssetOpLedgerComp, error) {
	return nil, nil
}
func (emptyLedger) ReadSQLCommittedLedger(context.Context, uint64) (*componentpb.PlayerAssetOpLedgerComp, error) {
	return nil, nil
}

// 离线卖家的收据 ACK 一直 done=false。修复前每轮都从表头取同样的 100 条,后面的 ACK 永远轮不到。
func TestRealDBReceiptAckPagesPastStuckHead(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	applier := &recordingApplier{}
	e.Applier = applier
	e.Ledger = emptyLedger{}
	payload, err := proto.Marshal(&assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: 10, Count: 1}}})
	require.NoError(t, err)
	total := tickAckLimit + tickAckLimit/2
	now := e.now()
	for i := 1; i <= total; i++ {
		rec := &tradepb.TradeAssetOpRecord{OpId: uint64(i), PlayerId: 61, Stream: uint32(assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT),
			StreamEpoch: 1, Seq: uint64(i), Kind: tradepb.TradeAssetOpKind_TRADE_ASSET_OP_KIND_ESCROW_DEBIT,
			Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED, Durable: true, ReceiptAckPending: true,
			RefKind: tradepb.TradeAssetOpRefKind_TRADE_ASSET_OP_REF_KIND_ORDER, RefId: 1, Payload: payload,
			NextAttemptMs: now, CreatedMs: now, UpdatedMs: now}
		require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error { return e.Repo.Ops.InsertOp(ctx, tx, rec) }))
	}

	tick(t, e)
	require.Len(t, applier.distinct(), tickAckLimit)
	tick(t, e)
	require.Len(t, applier.distinct(), total, "第二轮接着上一页往后取,表头卡住的 ACK 不再挡住后面的")
	before := len(applier.attempts())
	tick(t, e)
	after := applier.attempts()
	require.Greater(t, len(after), before)
	require.Equal(t, uint64(1), after[before], "到表尾后从头轮转")
}

// 同一页前两条分别模拟 RPC 失败和 SQL 账本失败，其余均正常离线。
// 依赖错误必须由 Tick 上报，同时不能阻止同页余项或下一页；待办保留到再次轮转。
type dependencyFailureAckApplier struct {
	recordingApplier
	rpcError error
}

func (a *dependencyFailureAckApplier) Do(ctx context.Context, rpc assetop.RPC, req *assetpb.AssetOpRequest) (assetop.Result, error) {
	result, err := a.recordingApplier.Do(ctx, rpc, req)
	switch req.GetCorrelationId() {
	case 1:
		return assetop.Result{}, a.rpcError
	case 2:
		return assetop.Result{Outcome: assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_APPLIED}, nil
	default:
		return result, err
	}
}

type dependencyFailureAckLedger struct {
	emptyLedger
	sqlError error
}

func (r dependencyFailureAckLedger) ReadSQLCommittedLedger(context.Context, uint64) (*componentpb.PlayerAssetOpLedgerComp, error) {
	return nil, r.sqlError
}

func TestRealDBReceiptAckReportsDependencyErrorsWithoutBlockingPages(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	rpcFailure := errors.New("测试收据RPC不可用")
	sqlFailure := errors.New("测试SQL账本不可用")
	applier := &dependencyFailureAckApplier{rpcError: rpcFailure}
	e.Applier = applier
	e.Ledger = dependencyFailureAckLedger{sqlError: sqlFailure}
	payload, err := proto.Marshal(&assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: 10, Count: 1}}})
	require.NoError(t, err)
	now := e.now()
	for i := 1; i <= tickAckLimit+1; i++ {
		rec := &tradepb.TradeAssetOpRecord{OpId: uint64(i), PlayerId: 63, Stream: uint32(assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT),
			StreamEpoch: 1, Seq: uint64(i), Kind: tradepb.TradeAssetOpKind_TRADE_ASSET_OP_KIND_ESCROW_DEBIT,
			Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED, Durable: true, ReceiptAckPending: true,
			RefKind: tradepb.TradeAssetOpRefKind_TRADE_ASSET_OP_REF_KIND_ORDER, RefId: 1, Payload: payload,
			NextAttemptMs: now, CreatedMs: now, UpdatedMs: now}
		require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error { return e.Repo.Ops.InsertOp(ctx, tx, rec) }))
	}

	err = e.Tick(ctx)
	require.ErrorIs(t, err, rpcFailure, "RPC故障不能伪装成正常离线或成功Tick")
	require.ErrorIs(t, err, sqlFailure, "页末汇总仍须保留SQL账本故障原因")
	require.Len(t, applier.attempts(), tickAckLimit, "前两条失败不能阻止本页余项")
	require.EqualValues(t, tickAckLimit, e.ackAfterOpID)
	var pending int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE receipt_ack_pending=1").Scan(&pending))
	require.Equal(t, tickAckLimit+1, pending, "依赖错误与正常离线都不能清除未获SQL确认的收据待办")

	require.NoError(t, e.Tick(ctx), "正常离线done=false不属于故障，下一页仍应成功")
	require.Len(t, applier.distinct(), tickAckLimit+1)
	require.Zero(t, e.ackAfterOpID, "读到尾页后回到表头")
	err = e.Tick(ctx)
	require.ErrorIs(t, err, rpcFailure, "原失败项仍须再次尝试")
	require.ErrorIs(t, err, sqlFailure)
	require.EqualValues(t, 1, applier.attempts()[tickAckLimit+1], "页尾后须重新尝试未清理的失败项")
}

// 一行收据 ACK 的 payload 解不开时,修复前 AckDue 整页报错、游标停在它前面,全服收据 ACK 停摆。
// 现在跳过这一行,同页其余 ACK 照常尝试,Tick 不报错;坏行原样留在表里等人工(不清 receipt_ack_pending)。
func TestRealDBReceiptAckSkipsUndecodablePayload(t *testing.T) {
	e := realDBEngine(t)
	ctx := context.Background()
	applier := &recordingApplier{}
	e.Applier = applier
	e.Ledger = emptyLedger{}
	good, err := proto.Marshal(&assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: 10, Count: 1}}})
	require.NoError(t, err)
	now := e.now()
	for i := 1; i <= 3; i++ {
		payload := good
		if i == 1 {
			payload = []byte{0xff, 0xff, 0xff}
		}
		rec := &tradepb.TradeAssetOpRecord{OpId: uint64(i), PlayerId: 62, Stream: uint32(assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT),
			StreamEpoch: 1, Seq: uint64(i), Kind: tradepb.TradeAssetOpKind_TRADE_ASSET_OP_KIND_ESCROW_DEBIT,
			Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED, Durable: true, ReceiptAckPending: true,
			RefKind: tradepb.TradeAssetOpRefKind_TRADE_ASSET_OP_REF_KIND_ORDER, RefId: 1, Payload: payload,
			NextAttemptMs: now, CreatedMs: now, UpdatedMs: now}
		require.NoError(t, e.Repo.Tx(ctx, func(tx *sql.Tx) error { return e.Repo.Ops.InsertOp(ctx, tx, rec) }))
	}

	tick(t, e)
	seen := applier.distinct()
	require.False(t, seen[1], "解不开的 payload 不发 ACK")
	require.True(t, seen[2], "坏行不能挡住同页后面的 ACK")
	require.True(t, seen[3])
	var pending bool
	require.NoError(t, e.Repo.DB.QueryRow("SELECT receipt_ack_pending FROM trade_asset_op WHERE op_id=1").Scan(&pending))
	require.True(t, pending, "坏行保留 ACK 待办,等人工")
}
