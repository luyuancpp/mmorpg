package main

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

// 商城签名的跨语言黄金样例:同一组输入、同一个期望值逐字出现在本文件、
// go/trade/internal/commerce/mall_signature_test.go 与 java/mall_game/.../HmacSignatureGoldenTest.java。
// 期望值由与三边都无关的 Python hmac / hashlib 离线算出。改算法必须三边同批改、三处期望值同批换。
const (
	goldenCommercePaymentBody = `{"orderId":"00112233-4455-6677-8899-aabbccddeeff","channel":"mock-dev","transactionId":"mockdev-00112233445566778899aabbccddeeff","amountMinor":600,"currency":"CNY"}`
	testCommercePaymentSecret = "robot-mock-payer-test-secret-000000000001"
)

// 旧三语言黄金向量只锁签名算法；驱动不再发送这套可指定渠道金额的请求体。
type commercePayment struct {
	OrderID       string `json:"orderId"`
	Channel       string `json:"channel"`
	TransactionID string `json:"transactionId"`
	AmountMinor   uint64 `json:"amountMinor"`
	Currency      string `json:"currency"`
}

func TestCommerceSignMatchesCrossLanguageGoldenForPaymentPost(t *testing.T) {
	got := commerceSign("commerce-golden-payment-secret-0123456789", "payment", "payment-adapter", "1700000000123",
		"golden-nonce-0000000001", "POST", "/internal/payments", []byte(goldenCommercePaymentBody))
	if want := "886584609f3fb1fa50a5ddb30f02837fb7e75d11fa0075f344447872c7eaf051"; got != want {
		t.Fatalf("payment 签名与三语言黄金值不一致\n实际: %s\n期望: %s", got, want)
	}
}

func TestCommerceSignMatchesCrossLanguageGoldenForEmptyBodyGet(t *testing.T) {
	got := commerceSign("commerce-golden-gateway-secret-0123456789", "gateway", "22", "1700000000456",
		"golden-nonce-0000000002", "GET", "/api/orders/00112233-4455-6677-8899-aabbccddeeff", nil)
	if want := "3d85985f791cbcf33107071195269b4a64890a3c3637ada6379df8c1c547fae1"; got != want {
		t.Fatalf("空正文签名与三语言黄金值不一致\n实际: %s\n期望: %s", got, want)
	}
}

// 本文件生成的请求体必须与黄金样例的正文逐字节相同:字段顺序 / 命名一变,商城验签用的就不再是这份字节。
func TestCommercePaymentJSONMatchesGoldenBodyBytes(t *testing.T) {
	body, err := json.Marshal(commercePayment{
		OrderID: "00112233-4455-6677-8899-aabbccddeeff", Channel: "mock-dev",
		TransactionID: "mockdev-00112233445566778899aabbccddeeff", AmountMinor: 600, Currency: "CNY",
	})
	if err != nil {
		t.Fatal(err)
	}
	if string(body) != goldenCommercePaymentBody {
		t.Fatalf("付款请求体与商城契约字段不符\n实际: %s\n期望: %s", body, goldenCommercePaymentBody)
	}
}

func TestNewCommerceMockPayerRejectsUnsafeConfiguration(t *testing.T) {
	cases := []struct {
		name, url, subject, secret string
	}{
		{"非回环地址", "http://10.0.0.8:18120", "payment-adapter", testCommercePaymentSecret},
		{"主机名而非字面量 IP", "http://localhost:18120", "payment-adapter", testCommercePaymentSecret},
		{"https", "https://127.0.0.1:18120", "payment-adapter", testCommercePaymentSecret},
		{"带凭据", "http://u:p@127.0.0.1:18120", "payment-adapter", testCommercePaymentSecret},
		{"带路径", "http://127.0.0.1:18120/mall", "payment-adapter", testCommercePaymentSecret},
		{"带查询", "http://127.0.0.1:18120?x=1", "payment-adapter", testCommercePaymentSecret},
		{"缺端口", "http://127.0.0.1", "payment-adapter", testCommercePaymentSecret},
		{"零端口", "http://127.0.0.1:0", "payment-adapter", testCommercePaymentSecret},
		{"端口越界", "http://127.0.0.1:65536", "payment-adapter", testCommercePaymentSecret},
		{"subject 大写", "http://127.0.0.1:18120", "Payment", testCommercePaymentSecret},
		{"密钥过短", "http://127.0.0.1:18120", "payment-adapter", "short-secret"},
		{"密钥带空白", "http://127.0.0.1:18120", "payment-adapter", testCommercePaymentSecret + " "},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if _, err := newCommerceMockPayer(c.url, c.subject, c.secret); err == nil {
				t.Fatalf("应当拒绝:%s", c.url)
			}
		})
	}
	if _, err := newCommerceMockPayer("http://127.0.0.1:18120", "payment-adapter", testCommercePaymentSecret); err != nil {
		t.Fatalf("合法配置被拒: %v", err)
	}
	if _, err := newCommerceMockPayer("http://[::1]:18120/", "payment-adapter", testCommercePaymentSecret); err != nil {
		t.Fatalf("IPv6 回环被拒: %v", err)
	}
}

// 密钥形状错误时错误文本不得带出密钥本身。
func TestNewCommerceMockPayerNeverEchoesTheSecret(t *testing.T) {
	const leaked = "this-secret-is-not-long-enough"
	_, err := newCommerceMockPayer("http://127.0.0.1:18120", "payment-adapter", leaked)
	if err == nil || strings.Contains(err.Error(), leaked) {
		t.Fatalf("错误文本回显了密钥或没有报错: %v", err)
	}
}

type capturedCommerceRequest struct {
	method, path string
	header       http.Header
	body         []byte
}

// newCapturingMall 起一个桩商城:把收到的请求交回测试 goroutine,并按 respond 应答。
func newCapturingMall(t *testing.T, respond func(w http.ResponseWriter, body []byte)) (*httptest.Server, <-chan capturedCommerceRequest) {
	t.Helper()
	received := make(chan capturedCommerceRequest, 8)
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		received <- capturedCommerceRequest{method: r.Method, path: r.URL.Path, header: r.Header.Clone(), body: body}
		respond(w, body)
	}))
	t.Cleanup(server.Close)
	return server, received
}

func testPayment() commerceMockPay {
	return commerceMockPay{
		OrderID: "00112233-4455-6677-8899-aabbccddeeff", RequestID: "mock-order-request-0001",
	}
}

func TestRecordPaymentSignsTheExactBytesAsPaymentRole(t *testing.T) {
	server, received := newCapturingMall(t, func(w http.ResponseWriter, body []byte) {
		var in commerceMockPay
		_ = json.Unmarshal(body, &in)
		_ = json.NewEncoder(w).Encode(commerceMallOrder{OrderID: in.OrderID, BuyerID: "22", SkuID: 990001, Quantity: 1,
			AmountMinor: 600, Currency: "CNY", State: "DELIVERY_PENDING"})
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	payer.now = func() time.Time { return time.UnixMilli(1700000000999) }
	order, err := payer.RecordPayment(context.Background(), testPayment())
	if err != nil {
		t.Fatalf("记已付失败: %v", err)
	}
	if order.State != "DELIVERY_PENDING" || order.BuyerID != "22" {
		t.Fatalf("应答解析错误: %+v", order)
	}
	got := <-received
	var fields map[string]json.RawMessage
	if err := json.Unmarshal(got.body, &fields); err != nil || len(fields) != 2 || fields["orderId"] == nil || fields["requestId"] == nil {
		t.Fatal("mock只可发送原订单和稳定请求号，不能发送金额、币种、渠道或流水")
	}
	if got.method != http.MethodPost || got.path != commercePaymentPath {
		t.Fatalf("请求行错误: %s %s", got.method, got.path)
	}
	if got.header.Get("X-Commerce-Subject") != "payment-adapter" || got.header.Get("X-Commerce-Timestamp") != "1700000000999" {
		t.Fatalf("认证头错误: subject=%q timestamp=%q", got.header.Get("X-Commerce-Subject"), got.header.Get("X-Commerce-Timestamp"))
	}
	nonce := got.header.Get("X-Commerce-Nonce")
	if len(nonce) != 32 || strings.Trim(nonce, "0123456789abcdef") != "" {
		t.Fatalf("nonce 必须是 32 位小写 hex: %q", nonce)
	}
	want := commerceSign(testCommercePaymentSecret, "payment", "payment-adapter", "1700000000999", nonce, http.MethodPost, commercePaymentPath, got.body)
	if got.header.Get("X-Commerce-Signature") != want {
		t.Fatalf("签名不覆盖实际发出的正文字节")
	}
	for _, name := range []string{"X-Commerce-Subject", "X-Commerce-Timestamp", "X-Commerce-Nonce", "X-Commerce-Signature"} {
		if len(got.header.Values(name)) != 1 {
			t.Fatalf("%s 必须恰好出现一次(商城拒绝重复头)", name)
		}
	}
}

// 每次请求换新 nonce:商城按 (角色, nonce) 去重,同一笔付款的幂等重发也必须是新 nonce。
func TestRecordPaymentUsesAFreshNoncePerRequest(t *testing.T) {
	server, received := newCapturingMall(t, func(w http.ResponseWriter, body []byte) {
		var in commerceMockPay
		_ = json.Unmarshal(body, &in)
		_ = json.NewEncoder(w).Encode(commerceMallOrder{OrderID: in.OrderID, State: "DELIVERY_PENDING"})
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 2; i++ {
		if _, err := payer.RecordPayment(context.Background(), testPayment()); err != nil {
			t.Fatal(err)
		}
	}
	first, second := <-received, <-received
	if first.header.Get("X-Commerce-Nonce") == second.header.Get("X-Commerce-Nonce") {
		t.Fatal("两次请求复用了同一个 nonce")
	}
	if string(first.body) != string(second.body) {
		t.Fatal("同一笔mock付款必须保留原订单和原请求号字节")
	}
}

func TestRecordPaymentRejectsInvalidOriginalBeforeAnyRequest(t *testing.T) {
	server, received := newCapturingMall(t, func(w http.ResponseWriter, _ []byte) { w.WriteHeader(http.StatusOK) })
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	for _, edit := range []func(*commerceMockPay){
		func(p *commerceMockPay) { p.OrderID = "00112233-4455-6677-8899-AABBCCDDEEFF" },
		func(p *commerceMockPay) { p.OrderID = "" },
		func(p *commerceMockPay) { p.RequestID = "has space" },
		func(p *commerceMockPay) { p.RequestID = "short" },
		func(p *commerceMockPay) { p.RequestID = strings.Repeat("x", 129) },
	} {
		payment := testPayment()
		edit(&payment)
		if _, err := payer.RecordPayment(context.Background(), payment); err == nil {
			t.Fatalf("应当在发送前拒绝: %+v", payment)
		}
	}
	select {
	case got := <-received:
		t.Fatalf("非法付款不该发出任何请求,却发出了 %s %s", got.method, got.path)
	default:
	}
}

func TestProbeAmountOverrideUsesOnlyNewMockPathAndPreservesOriginal(t *testing.T) {
	server, received := newCapturingMall(t, func(w http.ResponseWriter, _ []byte) {
		w.WriteHeader(http.StatusBadRequest)
		_, _ = w.Write([]byte(`{"error":"mock命令字段非法"}`))
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	var denied *commerceHTTPError
	if err := payer.ProbeAmountOverride(context.Background(), testPayment(), 601); !errors.As(err, &denied) || denied.Status != 400 {
		t.Fatalf("金额覆盖必须以400反例返回: %v", err)
	}
	got := <-received
	if got.path != "/internal/mock/pay" {
		t.Fatal("反例不能退回旧付款事实接口")
	}
	var fields map[string]json.RawMessage
	if err := json.Unmarshal(got.body, &fields); err != nil || len(fields) != 3 || string(fields["amountMinor"]) != "601" {
		t.Fatal("反例必须在两个原字段上只追加金额覆盖")
	}
	var original commerceMockPay
	if err := json.Unmarshal(got.body, &original); err != nil || original != testPayment() {
		t.Fatal("金额反例改动了原订单或请求号")
	}
}

func TestRecordPaymentSurfacesMallRejectionStatus(t *testing.T) {
	server, _ := newCapturingMall(t, func(w http.ResponseWriter, _ []byte) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusConflict)
		_, _ = w.Write([]byte(`{"error":"支付金额或币种不匹配"}`))
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	_, err = payer.RecordPayment(context.Background(), testPayment())
	var rejected *commerceHTTPError
	if !errors.As(err, &rejected) || rejected.Status != http.StatusConflict || rejected.Message != "支付金额或币种不匹配" {
		t.Fatalf("应当带回商城的 409 与原因: %v", err)
	}
}

// 不跟随跳转:误配的跳转不得把签名请求带去别处。
func TestRecordPaymentDoesNotFollowRedirects(t *testing.T) {
	server, received := newCapturingMall(t, func(w http.ResponseWriter, _ []byte) {
		w.Header().Set("Location", "http://127.0.0.1:1/elsewhere")
		w.WriteHeader(http.StatusTemporaryRedirect)
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	_, err = payer.RecordPayment(context.Background(), testPayment())
	var rejected *commerceHTTPError
	if !errors.As(err, &rejected) || rejected.Status != http.StatusTemporaryRedirect {
		t.Fatalf("跳转应当原样作为失败返回: %v", err)
	}
	<-received
	select {
	case got := <-received:
		t.Fatalf("跟随了跳转: %s %s", got.method, got.path)
	default:
	}
}

func TestRecordPaymentRejectsAnswerForAnotherOrder(t *testing.T) {
	server, _ := newCapturingMall(t, func(w http.ResponseWriter, _ []byte) {
		_ = json.NewEncoder(w).Encode(commerceMallOrder{OrderID: "ffffffff-4455-6677-8899-aabbccddeeff", State: "DELIVERY_PENDING"})
	})
	payer, err := newCommerceMockPayer(server.URL, "payment-adapter", testCommercePaymentSecret)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := payer.RecordPayment(context.Background(), testPayment()); err == nil {
		t.Fatal("应答订单号不符必须判失败")
	}
}
