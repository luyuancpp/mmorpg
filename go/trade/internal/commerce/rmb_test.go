package commerce

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	base "proto/common/base"
	dspb "proto/data_service"
	tradepb "proto/trade"
	"shared/accountdirectory"
	"shared/roleidentity"
	"trade/internal/data"

	"github.com/stretchr/testify/require"
)

// 目录替身只隔离工作流I/O，SQL合同/outbox走真实专用TiDB；原生冻结证明由另外原生/目录用例验证。
type rmbDirectoryFixture struct {
	ConfirmReady            bool
	TransferUnknown         bool
	TransferCalls           []uint64
	BeginCalls, CancelCalls int
	TransferDone            bool
	Roles                   fixtureRoles
}

func (f *rmbDirectoryFixture) Lookup(ctx context.Context, p uint64) (*dspb.AccountRoleOwnership, error) {
	return f.Roles.Ownership(ctx, nil, p)
}
func (f *rmbDirectoryFixture) Begin(_ context.Context, in *dspb.BeginAccountRoleTradeRequest) (*dspb.AccountRoleTradeReceipt, error) {
	f.BeginCalls++
	return &dspb.AccountRoleTradeReceipt{ListingId: in.ListingId, PlayerId: in.PlayerId, SellerAccount: in.SellerAccount, SellerOwnerRevision: in.ExpectedOwnerRevision, State: dspb.AccountRoleTradeState_ACCOUNT_ROLE_TRADE_STATE_FREEZING}, nil
}
func (f *rmbDirectoryFixture) Confirm(_ context.Context, in *dspb.ConfirmAccountRoleTradeFreezeRequest) (*dspb.AccountRoleTradeReceipt, error) {
	if !f.ConfirmReady {
		return nil, accountdirectory.ErrUnavailable
	}
	return &dspb.AccountRoleTradeReceipt{ListingId: in.ListingId, PlayerId: in.PlayerId, SellerAccount: in.SellerAccount, SellerOwnerRevision: in.ExpectedOwnerRevision, State: dspb.AccountRoleTradeState_ACCOUNT_ROLE_TRADE_STATE_FROZEN, FreezeEvidence: &dspb.AccountRoleFreezeEvidence{ListingId: in.ListingId, PlayerId: in.PlayerId, OwnerRevision: in.ExpectedOwnerRevision, OwnerEpoch: 17, SaveSeq: 3, PersistedDigest: make([]byte, 32), Kind: dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_SCENE}}, nil
}
func (f *rmbDirectoryFixture) Cancel(_ context.Context, in *dspb.CancelAccountRoleTradeRequest) (*dspb.AccountRoleTradeReceipt, error) {
	f.CancelCalls++
	return &dspb.AccountRoleTradeReceipt{ListingId: in.ListingId, PlayerId: in.PlayerId, SellerAccount: in.SellerAccount, SellerOwnerRevision: in.ExpectedOwnerRevision, State: dspb.AccountRoleTradeState_ACCOUNT_ROLE_TRADE_STATE_CANCELLED}, nil
}
func (f *rmbDirectoryFixture) Transfer(_ context.Context, in *dspb.TransferAccountRoleRequest) (*dspb.AccountRoleTransferReceipt, error) {
	f.TransferCalls = append(f.TransferCalls, in.TxId)
	if f.TransferUnknown && !f.TransferDone {
		f.TransferDone = true
		return nil, context.DeadlineExceeded
	}
	return &dspb.AccountRoleTransferReceipt{TxId: in.TxId, ListingId: in.ListingId, PlayerId: in.PlayerId, SellerAccount: in.SellerAccount, BuyerAccount: in.BuyerAccount, SellerOwnerRevision: in.ExpectedOwnerRevision, BuyerOwnerRevision: in.ExpectedOwnerRevision + 1, Role: &base.AccountSimplePlayer{PlayerId: in.PlayerId}, CreatedMs: 1}, nil
}

type rmbSealerFixture struct{}

func (rmbSealerFixture) Seal(context.Context, *base.RoleTradeSeal) error { return nil }

type rmbMallProbe struct {
	Registers         []map[string]any
	Receipts          []map[string]string
	FailFirstRegister bool
}

func rmbRuntime(t *testing.T) (*RmbConsignments, *rmbDirectoryFixture, *rmbMallProbe) {
	t.Helper()
	e := tiDBEngine(t)
	directory := &rmbDirectoryFixture{Roles: fixtureRoles{}}
	probe := &rmbMallProbe{}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		require.Equal(t, "trade", r.Header.Get("X-Commerce-Subject"))
		require.Len(t, r.Header.Get("X-Commerce-Signature"), 64)
		switch r.URL.Path {
		case "/internal/consignments/register":
			var in map[string]any
			require.NoError(t, json.NewDecoder(r.Body).Decode(&in))
			probe.Registers = append(probe.Registers, in)
			if probe.FailFirstRegister && len(probe.Registers) == 1 {
				w.WriteHeader(503)
				return
			}
			require.NoError(t, json.NewEncoder(w).Encode(rmbListing{ID: in["listingId"].(string), Seller: in["sellerPlayerId"].(string), Zone: 1, Kind: in["assetKind"].(string), Title: in["title"].(string), Price: uint64(in["priceFen"].(float64)), Fee: 500, Cooling: 259200000, NoticeEnd: 1, SaleEnd: 100000, State: "LISTED", Phase: "SALE"}))
		case "/internal/consignments/cancel":
			var in map[string]string
			require.NoError(t, json.NewDecoder(r.Body).Decode(&in))
			require.NoError(t, json.NewEncoder(w).Encode(rmbListing{ID: in["listingId"], Seller: "11", Zone: 1, Kind: "CHARACTER", Title: "原托管", Price: 101, Fee: 500, NoticeEnd: 1, SaleEnd: 100000, State: "RETURNING", Phase: "ENDED"}))
		case "/internal/deliveries/receipt":
			var in map[string]string
			require.NoError(t, json.NewDecoder(r.Body).Decode(&in))
			probe.Receipts = append(probe.Receipts, in)
		default:
			w.WriteHeader(404)
		}
	}))
	t.Cleanup(server.Close)
	mall, err := NewMall(server.URL, strings.Repeat("g", 32), strings.Repeat("d", 32), e)
	require.NoError(t, err)
	s := &RmbConsignments{Engine: e, Mall: mall, Directory: directory, Sealer: rmbSealerFixture{}, CurrencyTypes: []uint32{0}}
	e.Rmb = s
	return s, directory, probe
}
func rmbStep(t *testing.T, s *RmbConsignments, id uint64) error {
	t.Helper()
	r, err := s.Engine.Repo.Get(context.Background(), id)
	require.NoError(t, err)
	return s.step(context.Background(), r)
}
func rmbCreate(t *testing.T, s *RmbConsignments, character bool) *tradepb.TradeCommerceRecord {
	t.Helper()
	in := &tradepb.CreateRmbConsignmentRequest{ClientRequestId: "rmb-request-original", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 2}}, PriceFen: 101, Title: "原托管"}
	if character {
		in.Assets = nil
		in.CharacterPlayerId = 11
	}
	r, err := s.Create(context.Background(), "account-11", 11, 1, 1, in)
	require.NoError(t, err)
	return r
}

func TestRmbSelectionRejectsMixedKindsBoundCurrencyAndUnconfiguredDiamond(t *testing.T) {
	s := &RmbConsignments{Engine: &Engine{Policy: Policy{Items: []uint32{10}, AllowInstances: true}}}
	for _, v := range []*tradepb.CreateRmbConsignmentRequest{{Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}, PetId: 7}}, {Assets: &tradepb.TradeAssetSelection{CurrencyAmount: 1, CurrencyType: 2}}, {Assets: &tradepb.TradeAssetSelection{CurrencyAmount: 1, CurrencyType: 1}}, {CharacterPlayerId: 11, Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}}} {
		_, err := s.selection(v)
		require.Error(t, err)
	}
	kind, err := s.selection(&tradepb.CreateRmbConsignmentRequest{Assets: &tradepb.TradeAssetSelection{CurrencyAmount: 1}})
	require.NoError(t, err)
	require.Equal(t, "CURRENCY", kind)
	s.CurrencyTypes = []uint32{0, 1}
	_, err = s.selection(&tradepb.CreateRmbConsignmentRequest{Assets: &tradepb.TradeAssetSelection{CurrencyAmount: 1, CurrencyType: 1}})
	require.NoError(t, err)
}
func TestTiDBRmbRegistersOnlyAfterEscrowAndDeliversOneOriginalCredit(t *testing.T) {
	s, _, probe := rmbRuntime(t)
	r := rmbCreate(t, s, false)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Empty(t, probe.Registers)
	settleLegs(t, s.Engine, 2)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Empty(t, probe.Registers)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Len(t, probe.Registers, 1)
	current, w := state(t, s.Engine, r.CommerceId)
	require.Equal(t, "rmb_listed", w.Stage)
	in := rmbDelivery{Order: "00112233-4455-6677-8899-aabbccddeeff", Buyer: "22", Account: "account-22", Revision: "1", Listing: probe.Registers[0]["listingId"].(string), Reference: w.Rmb.EscrowReference, Proof: w.Rmb.EscrowProof, Amount: 101}
	require.NoError(t, s.deliver(context.Background(), in))
	require.NoError(t, rmbStep(t, s, current.CommerceId))
	settleLegs(t, s.Engine, 2)
	require.NoError(t, rmbStep(t, s, current.CommerceId))
	require.NoError(t, s.deliver(context.Background(), in))
	require.NoError(t, s.deliver(context.Background(), in))
	require.Len(t, probe.Receipts, 2)
	require.Equal(t, probe.Receipts[0], probe.Receipts[1])
	require.Equal(t, "DELIVERED", probe.Receipts[0]["state"])
	var credits int
	require.NoError(t, s.Engine.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE player_id=22 AND stream=4").Scan(&credits))
	require.Equal(t, 1, credits)
	changed := in
	changed.Account = "new-owner"
	require.ErrorIs(t, s.deliver(context.Background(), changed), ErrReplay)
}
func TestTiDBRmbCharacterDoesNotPublishWithoutSQLProofAndReusesTransferTxAfterUnknownResult(t *testing.T) {
	s, dir, probe := rmbRuntime(t)
	r := rmbCreate(t, s, true)
	require.Error(t, rmbStep(t, s, r.CommerceId))
	require.Empty(t, probe.Registers)
	dir.ConfirmReady = true
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	_, w := state(t, s.Engine, r.CommerceId)
	in := rmbDelivery{Order: "00112233-4455-6677-8899-aabbccddeeff", Buyer: "22", Account: "account-22", Revision: "1", Listing: probe.Registers[0]["listingId"].(string), Reference: w.Rmb.EscrowReference, Proof: w.Rmb.EscrowProof, Amount: 101}
	require.NoError(t, s.deliver(context.Background(), in))
	dir.TransferUnknown = true
	require.ErrorIs(t, rmbStep(t, s, r.CommerceId), context.DeadlineExceeded)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Len(t, dir.TransferCalls, 2)
	require.NotZero(t, dir.TransferCalls[0])
	require.Equal(t, dir.TransferCalls[0], dir.TransferCalls[1])
	require.NoError(t, s.deliver(context.Background(), in))
	require.Equal(t, "DELIVERED", probe.Receipts[0]["state"])
	var ops int
	require.NoError(t, s.Engine.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op").Scan(&ops))
	require.Zero(t, ops, "角色过户不能复制玩家资产或伪造credit")
}
func TestTiDBRmbUnknownRegisterMustResolveBeforeCancelUnfreezesRole(t *testing.T) {
	s, dir, probe := rmbRuntime(t)
	dir.ConfirmReady = true
	probe.FailFirstRegister = true
	r := rmbCreate(t, s, true)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Error(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, s.Cancel(context.Background(), "account-11", r.CommerceId))
	require.Zero(t, dir.CancelCalls)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Len(t, probe.Registers, 2)
	require.Equal(t, probe.Registers[0], probe.Registers[1])
	require.Zero(t, dir.CancelCalls)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Equal(t, 1, dir.CancelCalls)
	cur, _ := state(t, s.Engine, r.CommerceId)
	require.EqualValues(t, Cancelled, cur.Status)
}
func TestTiDBRmbCancelBeforeFreezeDoesNotNeedSuccessfulBegin(t *testing.T) {
	s, dir, probe := rmbRuntime(t)
	r := rmbCreate(t, s, true)
	require.NoError(t, s.Cancel(context.Background(), "account-11", r.CommerceId))
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	require.Zero(t, dir.BeginCalls)
	require.Equal(t, 1, dir.CancelCalls)
	require.Empty(t, probe.Registers)
	current, workflow := state(t, s.Engine, r.CommerceId)
	require.EqualValues(t, Cancelled, current.Status)
	require.Equal(t, "rmb_returned", workflow.Stage)
}
func TestTiDBRmbBuyerOwnershipChangePersistsRefundDecisionWithoutCredit(t *testing.T) {
	s, dir, probe := rmbRuntime(t)
	r := rmbCreate(t, s, false)
	require.NoError(t, rmbStep(t, s, r.CommerceId))
	settleLegs(t, s.Engine, 2)
	for n := 0; n < 3; n++ {
		require.NoError(t, rmbStep(t, s, r.CommerceId))
	}
	_, w := state(t, s.Engine, r.CommerceId)
	dir.Roles.Values = map[uint64]*dspb.AccountRoleOwnership{22: {Account: "new-owner", OwnerRevision: 2, Role: &base.AccountSimplePlayer{PlayerId: 22}}}
	in := rmbDelivery{Order: "00112233-4455-6677-8899-aabbccddeeff", Buyer: "22", Account: "account-22", Revision: "1", Listing: probe.Registers[0]["listingId"].(string), Reference: w.Rmb.EscrowReference, Proof: w.Rmb.EscrowProof, Amount: 101}
	require.NoError(t, s.deliver(context.Background(), in))
	dir.Roles.Values = nil
	require.NoError(t, s.deliver(context.Background(), in))
	require.Len(t, probe.Receipts, 2)
	require.Equal(t, probe.Receipts[0], probe.Receipts[1])
	require.Equal(t, "REFUND_PENDING", probe.Receipts[0]["state"])
	cur, cw := state(t, s.Engine, r.CommerceId)
	require.Equal(t, "rmb_refund", cw.Stage)
	require.Zero(t, cur.BuyerPlayerId)
	var credits int
	require.NoError(t, s.Engine.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op WHERE stream=4").Scan(&credits))
	require.Zero(t, credits)
}
func TestTiDBOrdinaryBackgroundUsesPersistentOriginalIdentityAndRequestDigest(t *testing.T) {
	e := tiDBEngine(t)
	original := &dspb.AccountRoleOwnership{Account: "account-11", OwnerRevision: 1, Role: &base.AccountSimplePlayer{PlayerId: 11}}
	ctx, err := roleidentity.Capture(context.Background(), original)
	require.NoError(t, err)
	in := &tradepb.CreateConsignmentRequest{ClientRequestId: "original-contract", Assets: &tradepb.TradeAssetSelection{Items: map[uint32]uint32{10: 1}}, CurrencyType: 1, Price: 10, DurationMs: 60000}
	r, err := e.CreateConsignment(ctx, 11, in)
	require.NoError(t, err)
	e.Roles = fixtureRoles{Values: map[uint64]*dspb.AccountRoleOwnership{11: {Account: "buyer", OwnerRevision: 2, Role: &base.AccountSimplePlayer{PlayerId: 11}}}}
	require.ErrorIs(t, e.advance(context.Background(), r), ErrConflict)
	var ops int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op").Scan(&ops))
	require.Zero(t, ops)
	other, err := roleidentity.Capture(context.Background(), &dspb.AccountRoleOwnership{Account: "buyer", OwnerRevision: 2, Role: &base.AccountSimplePlayer{PlayerId: 11}})
	require.NoError(t, err)
	_, err = e.CreateConsignment(other, 11, in)
	require.ErrorIs(t, err, ErrReplay)
}
func TestTiDBOfficialIntentForOldOwnerCreatesDurableRefundWithoutAssetOp(t *testing.T) {
	e := tiDBEngine(t)
	e.Roles = fixtureRoles{Values: map[uint64]*dspb.AccountRoleOwnership{22: {Account: "new-owner", OwnerRevision: 2, Role: &base.AccountSimplePlayer{PlayerId: 22}}}}
	m := &Mall{Engine: e}
	in := mallIntent{OrderID: "00112233-4455-6677-8899-aabbccddeeff", BuyerID: "22", BuyerAccount: "account-22", BuyerOwnerRevision: "1", SKU: 1, Quantity: 1, Config: 10, Count: 1, Amount: 101, Currency: "CNY"}
	r, err := m.accept(context.Background(), in)
	require.NoError(t, err)
	require.EqualValues(t, 8, r.Status)
	w, err := data.Workflow(r)
	require.NoError(t, err)
	require.Equal(t, "official_refund", w.Stage)
	require.Empty(t, w.Legs)
	var ops int
	require.NoError(t, e.Repo.DB.QueryRow("SELECT COUNT(*) FROM trade_asset_op").Scan(&ops))
	require.Zero(t, ops)
	again, err := m.accept(context.Background(), in)
	require.NoError(t, err)
	require.Equal(t, r.CommerceId, again.CommerceId)
}
