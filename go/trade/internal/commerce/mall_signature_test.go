package commerce

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/require"
)

// 商城签名的跨语言黄金样例。同一组输入、同一个期望值逐字出现在三处：
//   - 本文件（go/trade 的 commerceSignature，trade 以 gateway / delivery 角色调商城）；
//   - robot/commerce_mock_payer_test.go（robot 的 commerceSign，dev 冒烟以 payment 角色记已付）；
//   - java/mall_game/src/test/java/com/game/mall/HmacSignatureGoldenTest.java（商城验签 HmacAuthentication.sign）。
// 期望值由与三边都无关的第四份实现（Python hmac / hashlib）离线算出。任何一边改了拼串规则，
// 这里的字面量就会把它抓出来 —— 否则线上表现是「全部 401」，而且没有一条日志能直接指出是哪一边改了。
// 改签名算法必须三边同批改、三处期望值同批换，不能只把某一边的字面量「修」成新输出。
const (
	goldenPaymentSecret = "commerce-golden-payment-secret-0123456789"
	goldenPaymentBody   = `{"orderId":"00112233-4455-6677-8899-aabbccddeeff","channel":"mock-dev","transactionId":"mockdev-00112233445566778899aabbccddeeff","amountMinor":600,"currency":"CNY"}`
	goldenPaymentSig    = "886584609f3fb1fa50a5ddb30f02837fb7e75d11fa0075f344447872c7eaf051"

	goldenGatewaySecret = "commerce-golden-gateway-secret-0123456789"
	goldenGatewaySig    = "3d85985f791cbcf33107071195269b4a64890a3c3637ada6379df8c1c547fae1"
)

func TestCommerceSignatureMatchesCrossLanguageGoldenForPaymentPost(t *testing.T) {
	got := commerceSignature(goldenPaymentSecret, "payment", "payment-adapter", "1700000000123", "golden-nonce-0000000001",
		"POST", "/internal/payments", []byte(goldenPaymentBody))
	require.Equal(t, goldenPaymentSig, got)
}

// 空正文（GET）取空字节的 SHA-256（e3b0c442…），不是省略这一行。
func TestCommerceSignatureMatchesCrossLanguageGoldenForEmptyBodyGet(t *testing.T) {
	got := commerceSignature(goldenGatewaySecret, "gateway", "22", "1700000000456", "golden-nonce-0000000002",
		"GET", "/api/orders/00112233-4455-6677-8899-aabbccddeeff", nil)
	require.Equal(t, goldenGatewaySig, got)
}

// 签名必须覆盖角色、主体、路径和正文：任一项变了签名就变。只靠黄金样例的话，
// 将来有人把某一项从拼串里删掉，只有字面量对不上，很容易被顺手改字面量「修好」。
func TestCommerceSignatureCoversEveryCanonicalField(t *testing.T) {
	base := func() []string {
		return []string{goldenPaymentSecret, "payment", "payment-adapter", "1700000000123", "golden-nonce-0000000001", "POST", "/internal/payments", goldenPaymentBody}
	}
	sign := func(v []string) string {
		return commerceSignature(v[0], v[1], v[2], v[3], v[4], v[5], v[6], []byte(v[7]))
	}
	reference := sign(base())
	for index, name := range []string{"secret", "role", "subject", "timestamp", "nonce", "method", "path", "body"} {
		tampered := base()
		tampered[index] += "x"
		require.NotEqual(t, reference, sign(tampered), "改了 %s 签名却没变", name)
	}
}

// Mall.call 签的必须是实际发出的路径和正文字节：桩服务把收到的请求原样交回测试 goroutine 重算一遍
// （断言不在处理器 goroutine 里做，FailNow 只能在测试 goroutine 上调用）。
func TestMallCallSignsTheExactPathAndBodyItSends(t *testing.T) {
	const gatewaySecret = "gateway-only-test-secret-00000000001"
	type seen struct {
		method, path string
		header       http.Header
		body         []byte
	}
	received := make(chan seen, 1)
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		select {
		case received <- seen{method: r.Method, path: r.URL.Path, header: r.Header.Clone(), body: body}:
		default:
		}
		_ = json.NewEncoder(w).Encode(map[string]any{"skus": []any{}})
	}))
	defer server.Close()
	m, err := NewMall(server.URL, gatewaySecret, "delivery-only-test-secret-0000000002", nil)
	require.NoError(t, err)
	_, err = m.Skus(context.Background(), 42, 0, 10)
	require.NoError(t, err)
	var got seen
	select {
	case got = <-received:
	default:
		t.Fatal("桩商城没有收到请求")
	}
	require.Equal(t, "POST", got.method)
	require.Equal(t, "/api/skus/list", got.path)
	require.Equal(t, "42", got.header.Get("X-Commerce-Subject"))
	require.Regexp(t, `^[A-Za-z0-9_-]{16,128}$`, got.header.Get("X-Commerce-Nonce"))
	require.Regexp(t, `^[0-9]{1,15}$`, got.header.Get("X-Commerce-Timestamp"))
	want := commerceSignature(gatewaySecret, "gateway", "42", got.header.Get("X-Commerce-Timestamp"),
		got.header.Get("X-Commerce-Nonce"), got.method, got.path, got.body)
	require.Equal(t, want, got.header.Get("X-Commerce-Signature"))
}
