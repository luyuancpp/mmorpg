package commerce

import (
	"bytes"
	"context"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"io"
	"net/http"
	"net/url"
	assetpb "proto/common/asset"
	tradepb "proto/trade"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// Mall只管官方库存/真实支付；本适配器保持HTTP契约隔离，不拥有第二份钱包。
type Mall struct {
	Base                          string
	GatewaySecret, DeliverySecret string
	HTTP                          *http.Client
	Engine                        *Engine
}
type mallIntent struct {
	OrderID  string `json:"orderId"`
	BuyerID  string `json:"buyerId"`
	SKU      uint64 `json:"skuId"`
	Quantity uint32 `json:"quantity"`
	Config   uint32 `json:"itemConfigId"`
	Count    uint32 `json:"itemCount"`
	Title    string `json:"title"`
	Amount   uint64 `json:"amountMinor"`
	Currency string `json:"currency"`
}
type MallOrder struct {
	OrderID  string `json:"orderId"`
	BuyerID  string `json:"buyerId"`
	SKU      uint64 `json:"skuId"`
	Quantity uint32 `json:"quantity"`
	Amount   uint64 `json:"amountMinor"`
	Currency string `json:"currency"`
	State    string `json:"state"`
	Deadline uint64 `json:"paymentDeadlineMs"`
}

var mallOrderID = regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`)

type mallSKU struct {
	ID        uint64 `json:"skuId"`
	Title     string `json:"title"`
	Price     uint64 `json:"priceMinor"`
	Currency  string `json:"currency"`
	Available uint64 `json:"available"`
}

func NewMall(base, gateway, delivery string, e *Engine) (*Mall, error) {
	u, err := url.Parse(base)
	if err != nil || u.Host == "" || (u.Scheme != "http" && u.Scheme != "https") || u.User != nil || u.RawQuery != "" || u.Fragment != "" || u.Path != "" && u.Path != "/" || len(gateway) < 32 || len(delivery) < 32 || gateway == delivery {
		return nil, errors.New("商城地址或独立调用密钥配置非法")
	}
	return &Mall{Base: strings.TrimRight(base, "/"), GatewaySecret: gateway, DeliverySecret: delivery, HTTP: &http.Client{Timeout: 2500 * time.Millisecond, CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }}, Engine: e}, nil
}

// commerceSignature 是商城 X-Commerce-Signature 的 v1 算法：正文 SHA-256 小写 hex，与其余七项按 \n 拼成规范串，
// HMAC-SHA256 后取小写 hex。必须与 java/mall_game HmacAuthentication.sign、robot commerceSign 逐字一致；
// 三边用同一组输入核对同一个期望值（mall_signature_test.go），改任一边都要同批改三边，否则线上表现是全部 401。
// path 是商城看到的原始 URI（含部署前缀），body 是实际发出的字节。
func commerceSignature(secret, role, subject, timestamp, nonce, method, path string, body []byte) string {
	hash := sha256.Sum256(body)
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write([]byte(strings.Join([]string{"v1", role, subject, timestamp, nonce, method, path, hex.EncodeToString(hash[:])}, "\n")))
	return hex.EncodeToString(mac.Sum(nil))
}

func (m *Mall) call(ctx context.Context, role, subject, method, path string, input, output any) error {
	var body []byte
	var err error
	if input != nil {
		body, err = json.Marshal(input)
		if err != nil {
			return err
		}
	}
	request, err := http.NewRequestWithContext(ctx, method, m.Base+path, bytes.NewReader(body))
	if err != nil {
		return err
	}
	stamp := strconv.FormatInt(time.Now().UnixMilli(), 10)
	nonceRaw := make([]byte, 16)
	if _, err = rand.Read(nonceRaw); err != nil {
		return err
	}
	nonce := hex.EncodeToString(nonceRaw)
	secret := m.GatewaySecret
	if role == "delivery" {
		secret = m.DeliverySecret
	}
	request.Header.Set("Content-Type", "application/json")
	request.Header.Set("X-Commerce-Subject", subject)
	request.Header.Set("X-Commerce-Timestamp", stamp)
	request.Header.Set("X-Commerce-Nonce", nonce)
	request.Header.Set("X-Commerce-Signature", commerceSignature(secret, role, subject, stamp, nonce, method, path, body))
	response, err := m.HTTP.Do(request)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	raw, err := io.ReadAll(io.LimitReader(response.Body, 256*1024+1))
	if err != nil {
		return err
	}
	if len(raw) > 256*1024 {
		return errors.New("商城应答越界")
	}
	if response.StatusCode != 200 {
		if response.StatusCode == 400 {
			return ErrInvalid
		}
		if response.StatusCode == 404 {
			return ErrMissing
		}
		if response.StatusCode == 409 {
			return ErrConflict
		}
		return fmt.Errorf("商城请求失败 HTTP %d", response.StatusCode)
	}
	if output != nil {
		return json.Unmarshal(raw, output)
	}
	return nil
}
func (m *Mall) Skus(ctx context.Context, player, after uint64, limit uint32) (*tradepb.ListOfficialSkusResponse, error) {
	var raw struct {
		Skus []mallSKU `json:"skus"`
	}
	if err := m.call(ctx, "gateway", strconv.FormatUint(player, 10), "POST", "/api/skus/list", map[string]any{"afterSkuId": after, "limit": min(max(limit, 1), 100)}, &raw); err != nil {
		return nil, err
	}
	out := &tradepb.ListOfficialSkusResponse{}
	for _, s := range raw.Skus {
		if s.ID <= after {
			continue
		}
		out.Skus = append(out.Skus, &tradepb.OfficialSkuView{SkuId: s.ID, Title: s.Title, PriceMinor: s.Price, Currency: s.Currency, Available: s.Available})
		out.NextSkuId = s.ID
		if len(out.Skus) >= int(min(max(limit, 1), 100)) {
			break
		}
	}
	return out, nil
}
func (m *Mall) Create(ctx context.Context, player uint64, in *tradepb.CreateOfficialOrderRequest) (*tradepb.CommerceOrderResponse, error) {
	if player == 0 || in.GetSkuId() == 0 || in.GetQuantity() == 0 || in.GetQuantity() > 4096 || len(in.GetClientRequestId()) < 8 || len(in.GetClientRequestId()) > 80 {
		return nil, ErrInvalid
	}
	var out MallOrder
	err := m.call(ctx, "gateway", strconv.FormatUint(player, 10), "POST", "/api/orders", map[string]any{"requestId": in.GetClientRequestId(), "skuId": in.GetSkuId(), "quantity": in.GetQuantity()}, &out)
	if err == nil && out.BuyerID != strconv.FormatUint(player, 10) {
		return nil, errors.New("商城订单主体不匹配")
	}
	if err != nil {
		return nil, err
	}
	return mallAnswer(out)
}
func (m *Mall) List(ctx context.Context, player uint64, after string, limit uint32) ([]*tradepb.CommerceOrderView, error) {
	var orders []MallOrder
	err := m.call(ctx, "gateway", strconv.FormatUint(player, 10), "POST", "/api/orders/list", map[string]any{"afterOrderId": after, "limit": min(max(limit, 1), 100)}, &orders)
	if err != nil {
		return nil, err
	}
	out := make([]*tradepb.CommerceOrderView, 0, len(orders))
	for _, r := range orders {
		if r.BuyerID != strconv.FormatUint(player, 10) {
			return nil, errors.New("商城订单主体不匹配")
		}
		answer, err := mallAnswer(r)
		if err != nil {
			return nil, err
		}
		out = append(out, answer.Order)
	}
	return out, nil
}
func (m *Mall) Order(ctx context.Context, player uint64, id string, cancel bool) (*tradepb.CommerceOrderResponse, error) {
	if !mallOrderID.MatchString(id) {
		return nil, ErrInvalid
	}
	var out MallOrder
	method, path := "GET", "/api/orders/"+id
	if cancel {
		method = "POST"
		path += "/cancel"
	}
	err := m.call(ctx, "gateway", strconv.FormatUint(player, 10), method, path, nil, &out)
	if err == nil && out.BuyerID != strconv.FormatUint(player, 10) {
		return nil, errors.New("商城订单主体不匹配")
	}
	if err != nil {
		return nil, err
	}
	return mallAnswer(out)
}
func mallAnswer(o MallOrder) (*tradepb.CommerceOrderResponse, error) {
	states := map[string]tradepb.CommerceOrderState{"AWAITING_PAYMENT": 2, "DELIVERY_PENDING": 4, "DELIVERED": 5, "CANCELLED": 7, "REFUND_PENDING": 8, "REFUNDED": 9, "MANUAL": 10}
	state, known := states[o.State]
	buyer, err := strconv.ParseUint(o.BuyerID, 10, 64)
	if !known || err != nil || buyer == 0 || !mallOrderID.MatchString(o.OrderID) {
		return nil, errors.New("商城订单应答无效")
	}
	return &tradepb.CommerceOrderResponse{Order: &tradepb.CommerceOrderView{OrderId: o.OrderID, Kind: 1, State: state, Version: uint64(state), BuyerPlayerId: buyer, Amount: o.Amount, Currency: o.Currency, RelatedId: strconv.FormatUint(o.SKU, 10)}, ServerNowMs: uint64(time.Now().UnixMilli())}, nil
}
func (m *Mall) accept(ctx context.Context, in mallIntent) (*tradepb.TradeCommerceRecord, error) {
	player, err := strconv.ParseUint(in.BuyerID, 10, 64)
	if err != nil || player == 0 || !mallOrderID.MatchString(in.OrderID) || in.Config == 0 || in.Count == 0 || in.Count > 4096 || in.Quantity == 0 {
		return nil, ErrInvalid
	}
	key := RequestKey("official", player, in.OrderID)
	raw, err := json.Marshal(in)
	if err != nil {
		return nil, err
	}
	hash := sha256.Sum256(raw)
	if old, err := m.Engine.Repo.ByRequest(ctx, key); err == nil {
		if !bytes.Equal(hash[:], old.RequestDigest) {
			return nil, ErrReplay
		}
		return old, nil
	}
	id, err := m.Engine.IDs.Next(ctx)
	if err != nil {
		return nil, err
	}
	now := m.Engine.now()
	w := &tradepb.TradeCommerceWorkflow{Stage: "official_deliver", ExternalOrderSn: in.OrderID, SkuId: in.SKU, SkuQuantity: in.Quantity, ExternalAmount: in.Amount, ExternalCurrency: in.Currency, OfficialBundle: &assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: in.Config, Count: in.Count}}}}
	payload, err := proto.MarshalOptions{Deterministic: true}.Marshal(w)
	if err != nil {
		return nil, err
	}
	r := &tradepb.TradeCommerceRecord{CommerceId: id, RequestKey: key, RequestDigest: hash[:], Kind: 1, Status: Delivering, BuyerPlayerId: player, CreatedMs: now, UpdatedMs: now, Version: 1, WorkflowProto: payload}
	err = m.Engine.Repo.Tx(ctx, func(tx *sql.Tx) error { return m.Engine.Repo.Insert(ctx, tx, r) })
	if err != nil {
		old, read := m.Engine.Repo.ByRequest(ctx, key)
		if read == nil && bytes.Equal(hash[:], old.RequestDigest) {
			return old, nil
		}
		return nil, err
	}
	return r, nil
}

// 领取无需破坏性出队：多个副本会得到同一意图，同库唯一request_key保证只有一条资产seq。
// 单条意图失败不挡同批其余意图(2026-10-20,与 Engine.Tick 的单行隔离同一条修复):各条错误汇总后一并返回,
// 本轮预算用完才提前结束。
func (m *Mall) Tick(ctx context.Context) error {
	var intents []mallIntent
	if err := m.call(ctx, "delivery", "trade", "POST", "/internal/deliveries/poll", map[string]int{"limit": 20}, &intents); err != nil {
		return err
	}
	var errs []error
	for _, in := range intents {
		if err := ctx.Err(); err != nil {
			errs = append(errs, err)
			break
		}
		if err := m.deliverIntent(ctx, in); err != nil {
			errs = append(errs, fmt.Errorf("商城交付意图 %q: %w", in.OrderID, err))
		}
	}
	return errors.Join(errs...)
}

// deliverIntent 为一条已付款意图建(或找回)交付合同;合同已有终局时回报商城。
func (m *Mall) deliverIntent(ctx context.Context, in mallIntent) error {
	r, err := m.accept(ctx, in)
	if err != nil {
		return err
	}
	state := ""
	switch r.Status {
	case Settled:
		state = "DELIVERED"
	case 8:
		state = "REFUND_PENDING"
	case Manual:
		state = "MANUAL"
	}
	if state == "" {
		return nil
	}
	sum := sha256.Sum256(r.WorkflowProto)
	receipt := map[string]string{"orderId": in.OrderID, "tradeOrderId": strconv.FormatUint(r.CommerceId, 10), "state": state, "proof": hex.EncodeToString(sum[:])}
	return m.call(ctx, "delivery", "trade", "POST", "/internal/deliveries/receipt", receipt, nil)
}
