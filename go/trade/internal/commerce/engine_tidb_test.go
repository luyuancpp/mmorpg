package commerce

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"github.com/go-sql-driver/mysql"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	"net/http"
	"net/http/httptest"
	"os"
	assetpb "proto/common/asset"
	tradepb "proto/trade"
	"schemamigrate"
	"shared/assetop"
	"shared/authoritysql"
	"sync"
	"sync/atomic"
	"testing"
	"time"
	"trade/internal/data"
)

type testIDs struct{ value atomic.Uint64 }

func (i *testIDs) Next(context.Context) (uint64, error) { return i.value.Add(1), nil }

// 每例仅清理自身随机库；不复用、更不删除本地游戏库。
func tiDBEngine(t *testing.T) *Engine {
	t.Helper()
	dsn := os.Getenv("COMMERCE_TEST_DSN")
	if dsn == "" {
		if os.Getenv("COMMERCE_REQUIRE_TIDB") == "1" {
			t.Fatal("缺少 COMMERCE_TEST_DSN")
		}
		t.Skip("未配置隔离TiDB")
	}
	return openEngineDB(t, dsn, true)
}

// openEngineDB 在 dsn 指向的实例上新建随机库、按 proto 建表并装配 Engine;用例结束删库。
// requireTiDB=false 给一次性 MySQL 用(tick_isolation_db_test.go 的 realDBEngine):会话仍固定 RC,只是不设 TiDB 专用变量。
func openEngineDB(t *testing.T, dsn string, requireTiDB bool) *Engine {
	t.Helper()
	cfg, err := mysql.ParseDSN(dsn)
	require.NoError(t, err)
	cfg.DBName = ""
	admin, err := sql.Open("mysql", cfg.FormatDSN())
	require.NoError(t, err)
	raw := make([]byte, 10)
	_, err = rand.Read(raw)
	require.NoError(t, err)
	name := "commerce_it_" + hex.EncodeToString(raw)
	_, err = admin.Exec("CREATE DATABASE `" + name + "`")
	require.NoError(t, err)
	t.Cleanup(func() {
		_, e := admin.Exec("DROP DATABASE `" + name + "`")
		require.NoError(t, e)
		require.NoError(t, admin.Close())
	})
	cfg.DBName = name
	// TiDB 建表/预切 Region 可能超过运行池的 5s socket 预算。迁移使用正式独立迁移池，
	// 完成后关闭；状态机仍使用原运行池及其业务超时，不因测试建表放宽资产请求预算。
	migrationDB, err := authoritysql.OpenForMigration(context.Background(), cfg.FormatDSN(), name, requireTiDB)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, migrationDB.Close()) })
	report, err := schemamigrate.Up(context.Background(), migrationDB, schemamigrate.Options{Database: name, Tables: data.Tables()})
	require.NoError(t, err)
	require.Empty(t, report.Manual)
	require.NoError(t, migrationDB.Close())
	db, err := authoritysql.Open(context.Background(), cfg.FormatDSN(), name, requireTiDB, 12)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, db.Close()) })
	ops, err := data.NewAssetOpRepo(db, 3*time.Second)
	require.NoError(t, err)
	require.NoError(t, ops.EnsureSeqGuardRow(context.Background(), uint64(time.Now().UnixMilli())))
	return &Engine{Repo: &data.CommerceRepo{DB: db, Ops: ops}, IDs: &testIDs{}, OpIDs: &testIDs{}, Policy: Policy{Items: []uint32{10}, ItemTitles: map[uint32]string{10: "玄铁"}, Currencies: []uint32{1}}, Now: time.Now}
}
func tick(t *testing.T, e *Engine) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	require.NoError(t, e.Tick(ctx))
}

// 此夹具只为交易库状态机提供已提交的资产终局；不冒充scene真实资产端到端验收。
func settleLegs(t *testing.T, e *Engine, outcome uint32) {
	t.Helper()
	_, err := e.Repo.DB.Exec("UPDATE trade_asset_op SET status=?,durable=1 WHERE status=1", outcome)
	require.NoError(t, err)
}
func state(t *testing.T, e *Engine, id uint64) (*tradepb.TradeCommerceRecord, *tradepb.TradeCommerceWorkflow) {
	t.Helper()
	r, err := e.Repo.Get(context.Background(), id)
	require.NoError(t, err)
	w, err := data.Workflow(r)
	require.NoError(t, err)
	return r, w
}

func TestTiDBConsignmentConcurrentBuyAndWorkerRestart(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	offer, err := e.CreateConsignment(ctx, 11, &tradepb.CreateConsignmentRequest{ClientRequestId: "offer_0001", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 3}}, CurrencyType: 1, Price: 50, DurationMs: 60000})
	require.NoError(t, err)
	tick(t, e)
	r, _ := state(t, e, offer.CommerceId)
	require.Equal(t, Escrowing, r.Status, "未提交扣出不得上架")
	settleLegs(t, e, 2)
	tick(t, e)
	var wins atomic.Int32
	var wg sync.WaitGroup
	for p := uint64(21); p < 29; p++ {
		wg.Add(1)
		go func(p uint64) {
			defer wg.Done()
			_, err := e.Mutate(ctx, p, offer.CommerceId, "buy", fmt.Sprintf("buy-request-%d", p))
			if err == nil {
				wins.Add(1)
			} else {
				require.ErrorIs(t, err, ErrConflict)
			}
		}(p)
	}
	wg.Wait()
	require.EqualValues(t, 1, wins.Load())
	// 新进程只保留同一SQL库，读取原outbox/seq继续，不创建第二笔扣款。
	restarted := *e
	tick(t, &restarted)
	settleLegs(t, &restarted, 2)
	tick(t, &restarted)
	r, _ = state(t, e, offer.CommerceId)
	require.Equal(t, Delivering, r.Status)
	var credits int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&credits))
	require.Equal(t, 2, credits)
	settleLegs(t, &restarted, 2)
	tick(t, &restarted)
	r, _ = state(t, e, offer.CommerceId)
	require.Equal(t, Settled, r.Status)
	tick(t, &restarted)
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&credits))
	require.Equal(t, 2, credits)
}

func TestTiDBPlayerTradeRequiresBothEscrowSummariesAndReturnsOnCancel(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	offer, err := e.CreatePlayerTrade(ctx, 11, &tradepb.CreatePlayerTradeRequest{ClientRequestId: "trade_0001", BuyerPlayerId: 22, SellerAssets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 2}}, BuyerAssets: &tradepb.TradeAssetSelection{CurrencyType: 1, CurrencyAmount: 70}})
	require.NoError(t, err)
	tick(t, e)
	_, err = e.Mutate(ctx, 11, offer.CommerceId, "confirm")
	require.ErrorIs(t, err, ErrConflict)
	settleLegs(t, e, 2)
	tick(t, e)
	_, err = e.Mutate(ctx, 11, offer.CommerceId, "confirm")
	require.ErrorIs(t, err, ErrConflict)
	_, err = e.Mutate(ctx, 22, offer.CommerceId, "confirm")
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 2)
	tick(t, e)
	_, err = e.Mutate(ctx, 11, offer.CommerceId, "cancel")
	require.NoError(t, err)
	tick(t, e)
	r, w := state(t, e, offer.CommerceId)
	require.Equal(t, CancelPending, r.Status)
	require.NotNil(t, leg(w, "seller_return"))
	require.NotNil(t, leg(w, "buyer_return"))
	require.Nil(t, leg(w, "seller_receive"))
	settleLegs(t, e, 2)
	tick(t, e)
	r, _ = state(t, e, offer.CommerceId)
	require.Equal(t, Cancelled, r.Status)
}

func TestTiDBRejectedBuyRetryKeepsOriginalOutcomeAndSeq(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	offer, err := e.CreateConsignment(ctx, 11, &tradepb.CreateConsignmentRequest{ClientRequestId: "retry-offer-001", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}, CurrencyType: 1, Price: 50, DurationMs: 60000})
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 2)
	tick(t, e)
	_, err = e.Mutate(ctx, 22, offer.CommerceId, "buy", "buy-retry-001")
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 3)
	tick(t, e)
	_, before := state(t, e, offer.CommerceId)
	require.Equal(t, "open", before.Stage)
	_, err = e.Mutate(ctx, 22, offer.CommerceId, "buy", "buy-retry-001")
	require.ErrorIs(t, err, ErrConflict)
	tick(t, e)
	_, after := state(t, e, offer.CommerceId)
	require.True(t, proto.Equal(before, after), "被拒绝的同请求不能重开扣款")
	var count int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE player_id=22 AND stream=3").Scan(&count))
	require.Equal(t, 1, count)
	_, err = e.Mutate(ctx, 22, offer.CommerceId, "buy", "buy-retry-002")
	require.NoError(t, err, "显式新请求才允许重新购买")
	tick(t, e)
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE player_id=22 AND stream=3").Scan(&count))
	require.Equal(t, 2, count)
}

func TestTiDBOrderBookPriceTimePartialCancelWaitsForChildren(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	open := func(player uint64, side tradepb.MarketOrderSide, price uint64, qty uint32) uint64 {
		r, err := e.PlaceMarket(ctx, player, &tradepb.PlaceMarketOrderRequest{ClientRequestId: fmt.Sprintf("market-open-%d", player), MarketId: 1, ItemConfigId: 10, Side: side, UnitPrice: price, Quantity: qty, CurrencyType: 1})
		require.NoError(t, err)
		require.NoError(t, e.advance(ctx, r))
		settleLegs(t, e, 2)
		r, _ = state(t, e, r.CommerceId)
		require.NoError(t, e.advance(ctx, r))
		return r.CommerceId
	}
	expensive := open(11, 2, 12, 2)
	first := open(12, 2, 10, 2)
	second := open(13, 2, 10, 2)
	buy := open(22, 1, 15, 3)
	require.NoError(t, e.matchMarket(ctx, buy))
	require.NoError(t, e.matchMarket(ctx, buy))
	rows, err := e.Repo.DB.Query("SELECT commerce_id,counterparty_id,unit_price,quantity FROM trade_commerce WHERE parent_id=? ORDER BY commerce_id", buy)
	require.NoError(t, err)
	var children []uint64
	for rows.Next() {
		var id, seller, price uint64
		var qty uint32
		require.NoError(t, rows.Scan(&id, &seller, &price, &qty))
		children = append(children, id)
		require.EqualValues(t, 10, price, "按最优卖价和maker价成交")
		if len(children) == 1 {
			require.Equal(t, first, seller)
			require.EqualValues(t, 2, qty)
		} else {
			require.Equal(t, second, seller)
			require.EqualValues(t, 1, qty)
		}
	}
	require.NoError(t, rows.Err())
	require.NoError(t, rows.Close())
	require.Len(t, children, 2)
	r, _ := state(t, e, expensive)
	require.Zero(t, r.FilledQuantity, "价格更差的旧单不抢占最优价")
	for _, id := range children {
		r, _ := state(t, e, id)
		require.NoError(t, e.advance(ctx, r))
		_, w := state(t, e, id)
		require.Equal(t, uint64(r.Quantity)*5, leg(w, "buyer_surplus").Bundle.Currencies[0].Amount)
	}
	_, err = e.Mutate(ctx, 13, second, "cancel")
	require.NoError(t, err)
	r, _ = state(t, e, second)
	require.NoError(t, e.advance(ctx, r))
	_, w := state(t, e, second)
	require.EqualValues(t, 1, leg(w, "seller_return").Bundle.Items[0].Count, "仅退未成交数量")
	_, err = e.Repo.DB.Exec("UPDATE trade_asset_op SET status=2,durable=1 WHERE ref_id=? AND stream=4", second)
	require.NoError(t, err)
	r, _ = state(t, e, second)
	require.NoError(t, e.advance(ctx, r))
	r, w = state(t, e, second)
	require.Equal(t, CancelPending, r.Status, "子成交交付未结束不能声称撤单完全完成")
	require.Equal(t, "market_wait", w.Stage)
	settleLegs(t, e, 2)
	for _, id := range children {
		r, _ = state(t, e, id)
		require.NoError(t, e.advance(ctx, r))
	}
	r, _ = state(t, e, second)
	require.NoError(t, e.advance(ctx, r))
	r, _ = state(t, e, second)
	require.Equal(t, Cancelled, r.Status)
}

func TestTiDBPartialStopsBeforeCounterpartyOrRefund(t *testing.T) {
	e := tiDBEngine(t)
	offer, err := e.CreateConsignment(context.Background(), 11, &tradepb.CreateConsignmentRequest{ClientRequestId: "partial_001", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 3}}, CurrencyType: 1, Price: 50, DurationMs: 60000})
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 5)
	tick(t, e)
	r, _ := state(t, e, offer.CommerceId)
	require.Equal(t, Manual, r.Status)
	var credits int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&credits))
	require.Zero(t, credits)
}

func TestTiDBBidOutbidRefundAndLateBid(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	lot, err := e.CreateLot(ctx, 11, &tradepb.CreateAuctionLotRequest{ClientRequestId: "auction_001", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}, CurrencyType: 1, OpeningPrice: 50, MinimumIncrement: 5, DurationMs: 60000})
	require.NoError(t, err)
	tick(t, e)
	settleLegs(t, e, 2)
	tick(t, e)
	for i, p := range []uint64{21, 22} {
		bid, err := e.PlaceBid(ctx, p, lot.CommerceId, &tradepb.PlaceAuctionBidRequest{LotId: fmt.Sprint(lot.CommerceId), ClientRequestId: fmt.Sprintf("bid_000%d", i), Amount: uint64(50 + i*10)})
		require.NoError(t, err)
		tick(t, e)
		settleLegs(t, e, 2)
		tick(t, e)
		tick(t, e)
		_, bw := state(t, e, bid.CommerceId)
		require.Equal(t, "bid_held", bw.Stage)
	}
	tick(t, e)
	var refunds int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&refunds))
	require.Equal(t, 1, refunds, "被超价者原币退款待办")
	_, err = e.Repo.DB.Exec("UPDATE trade_commerce SET expires_ms=1 WHERE commerce_id=?", lot.CommerceId)
	require.NoError(t, err)
	_, err = e.PlaceBid(ctx, 23, lot.CommerceId, &tradepb.PlaceAuctionBidRequest{ClientRequestId: "late_bid_01", Amount: 100})
	require.ErrorIs(t, err, ErrEnded)
}

func TestTransferPreservesInstanceUnknownBytes(t *testing.T) {
	raw := []byte{0x08, 0x01, 0xf8, 0x7f, 0x03}
	s := &assetpb.AssetSnapshot{Items: []*assetpb.AssetItemInstance{{ItemUuid: 77, ConfigId: 10, StackSize: 1, InstanceProto: raw}}}
	out, err := Transfer(&assetpb.AssetBundle{ItemUuids: []uint64{77}}, s)
	require.NoError(t, err)
	require.True(t, proto.Equal(s.Items[0], out.ItemInstances[0]))
}

func TestTiDBContractCannotFinalizeFromRedisDurableOnly(t *testing.T) {
	e := tiDBEngine(t)
	offer, err := e.CreateConsignment(context.Background(), 11, &tradepb.CreateConsignmentRequest{ClientRequestId: "proof-guard-001", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}, CurrencyType: 1, Price: 50, DurationMs: 60000})
	require.NoError(t, err)
	tick(t, e)
	var id uint64
	require.NoError(t, e.Repo.DB.QueryRow("SELECT op_id FROM trade_asset_op WHERE ref_id=?", offer.CommerceId).Scan(&id))
	ok, err := e.Repo.Ops.Finalize(context.Background(), assetop.Op{OpID: id}, assetop.StatusApplied, assetop.Result{Outcome: assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_APPLIED, Durable: true}, uint64(time.Now().UnixMilli()))
	require.Error(t, err)
	require.False(t, ok)
	var status uint32
	require.NoError(t, e.Repo.DB.QueryRow("SELECT status FROM trade_asset_op WHERE op_id=?", id).Scan(&status))
	require.EqualValues(t, 1, status)
}

func TestTiDBMallBridgeWaitsForCommittedLegAndRecoversLostReceipt(t *testing.T) {
	e := tiDBEngine(t)
	ctx := context.Background()
	in := mallIntent{OrderID: "00112233-4455-6677-8899-aabbccddeeff", BuyerID: "22", SKU: 1, Quantity: 1, Config: 10, Count: 2, Title: "玄铁"}
	var acknowledgements atomic.Int32
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		require.Equal(t, "trade", r.Header.Get("X-Commerce-Subject"))
		require.Len(t, r.Header.Get("X-Commerce-Signature"), 64)
		if r.URL.Path == "/internal/deliveries/poll" {
			require.NoError(t, json.NewEncoder(w).Encode([]mallIntent{in}))
			return
		}
		if r.URL.Path == "/internal/deliveries/receipt" {
			var receipt map[string]string
			require.NoError(t, json.NewDecoder(r.Body).Decode(&receipt))
			require.Equal(t, "DELIVERED", receipt["state"])
			require.Equal(t, in.OrderID, receipt["orderId"])
			if acknowledgements.Add(1) == 1 {
				w.WriteHeader(503)
			}
			return
		}
		w.WriteHeader(404)
	}))
	defer server.Close()
	m, err := NewMall(server.URL, "gateway-only-test-secret-00000000001", "delivery-only-test-secret-0000000002", e)
	require.NoError(t, err)
	require.NoError(t, m.Tick(ctx))
	tick(t, e)
	require.NoError(t, m.Tick(ctx))
	require.Zero(t, acknowledgements.Load(), "仅RPC成功或待办入库不能通知商城已发货")
	var ops int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&ops))
	require.Equal(t, 1, ops)
	settleLegs(t, e, 2)
	tick(t, e)
	require.Error(t, m.Tick(ctx), "丢失回执保持同一合同")
	require.NoError(t, m.Tick(ctx))
	require.EqualValues(t, 2, acknowledgements.Load())
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&ops))
	require.Equal(t, 1, ops, "恢复不得换seq重发资产")
}
