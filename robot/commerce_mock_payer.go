package main

// 商城模拟付款器仅用于本机dev冒烟，以payment角色调用/internal/mock/pay。
//
// 为什么 robot 里会有一个付款方:真钱付款事实只能来自核实过渠道交易的可信适配器(设计上是 go/payment,
// 见 docs/design/payment-recharge.md;它尚未落码)。dev 冒烟要走通「下单 → 付款 → 发货」就得有人以 payment
// 角色签名记账,这里是与充值会话约定的冒烟例外(2026-10-19),边界如下,任何一条都不许放宽:
//   - 只连字面量回环 IP 的商城(newCommerceMockPayer 在任何网络操作之前校验),不跟随跳转、不走代理;
//   - 只提交orderId和稳定requestId，商城从冻结订单读取金额，先落mock提供方原件再投递回执；
//   - 不调用旧/internal/payments，不提供渠道、流水、金额或币种，不给mock-dev补允许名单；
//   - 密钥只从环境变量 COMMERCE_PAYMENT_SECRET 读,不进 yaml、不打日志、不进错误文本。
//
// 签名算法 commerceSign 与 java/mall_game HmacAuthentication.sign、go/trade commerceSignature 逐字一致,
// 三边各有一份同输入同期望值的签名黄金用例；旧向量仅保留为算法证据，不再用于发送旧付款命令。
// 本文件不依赖任何冒烟场景,人民币寄售冒烟等后续场景直接复用。

import (
	"bytes"
	"context"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const (
	// commercePaymentSecretEnv 与商城 application.yaml 的 commerce.payment-secret 读同一个环境变量。
	commercePaymentSecretEnv = "COMMERCE_PAYMENT_SECRET"
	// commerceMinSecretLen 与商城 CommerceProperties 的下限一致(短于它商城本身就拒启)。
	commerceMinSecretLen = 32
	// 只调用本地mock事实适配器；运行模式与独立开关由商城再次核验。
	commercePaymentPath = "/internal/mock/pay"
	// commerceMaxResponseBytes 限制读回的应答体:订单视图远小于它,超出即判失败而不是无界读。
	commerceMaxResponseBytes = 64 * 1024
)

// 与商城 CommerceApi.Payment / HmacAuthentication 的校验正则一致:发送前先挡住,失败原因留在本地而不是一个 400。
var (
	commerceSubjectPattern     = regexp.MustCompile(`^[a-z][a-z0-9-]{2,63}$`)
	commerceMockRequestPattern = regexp.MustCompile(`^[A-Za-z0-9_-]{8,128}$`)
	commerceOrderIDPattern     = regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`)
)

// commerceSign 是商城 X-Commerce-Signature 的 v1 算法:正文 SHA-256 小写 hex,与其余七项按 \n 拼成规范串,
// HMAC-SHA256 后取小写 hex。path 是商城看到的原始 URI,body 是实际发出的字节(空正文取空字节的摘要)。
func commerceSign(secret, role, subject, timestamp, nonce, method, path string, body []byte) string {
	digest := sha256.Sum256(body)
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write([]byte(strings.Join([]string{"v1", role, subject, timestamp, nonce, method, path, hex.EncodeToString(digest[:])}, "\n")))
	return hex.EncodeToString(mac.Sum(nil))
}

// 同一订单与请求号双向绑定，响应未知时仍重放这两个原字段。
type commerceMockPay struct {
	OrderID   string `json:"orderId"`
	RequestID string `json:"requestId"`
}

// commerceMallOrder 是商城订单视图(CommerceApi.Order)。State 是商城自己的状态字符串:
// AWAITING_PAYMENT / DELIVERY_PENDING / DELIVERED / CANCELLED / REFUND_PENDING / REFUNDED / MANUAL。
type commerceMallOrder struct {
	OrderID           string `json:"orderId"`
	BuyerID           string `json:"buyerId"`
	SkuID             uint64 `json:"skuId"`
	Quantity          uint32 `json:"quantity"`
	AmountMinor       uint64 `json:"amountMinor"`
	Currency          string `json:"currency"`
	State             string `json:"state"`
	PaymentDeadlineMs uint64 `json:"paymentDeadlineMs"`
}

// commerceHTTPError 是商城回了非 200。Message 取商城错误体里的 error 文本(商城不回显请求内容与密钥),截断到 100 个字符。
type commerceHTTPError struct {
	Status  int
	Message string
}

func (e *commerceHTTPError) Error() string {
	return fmt.Sprintf("商城回 HTTP %d: %s", e.Status, e.Message)
}

// commerceMockPayer 见文件头。一个实例可被串行或并发复用(无可变状态;now / nonce 只供测试替换)。
type commerceMockPayer struct {
	baseURL string
	subject string
	secret  string
	client  *http.Client
	now     func() time.Time
	nonce   func() (string, error)
}

// validateCommerceMockMallURL 只接受 http://<字面量回环 IP>:<端口>,不带凭据 / 路径 / 查询 / 片段。
// 不接受 localhost 等主机名:名字解析到哪里不归本进程控制。
func validateCommerceMockMallURL(raw string) error {
	endpoint, err := url.Parse(raw)
	if err != nil || endpoint.Scheme != "http" || endpoint.User != nil || endpoint.RawQuery != "" ||
		endpoint.Fragment != "" || endpoint.Opaque != "" || (endpoint.Path != "" && endpoint.Path != "/") {
		return fmt.Errorf("模拟付款器的商城地址必须是 http://<回环IP>:<端口>,不带凭据、路径或查询")
	}
	port, portErr := strconv.Atoi(endpoint.Port())
	if portErr != nil || port < 1 || port > 65535 {
		return fmt.Errorf("模拟付款器的商城地址必须显式带端口")
	}
	if ip := net.ParseIP(endpoint.Hostname()); ip == nil || !ip.IsLoopback() {
		return fmt.Errorf("模拟付款器只允许连字面量回环 IP 的商城(仅限本机 dev 冒烟)")
	}
	return nil
}

func newCommerceMockPayer(baseURL, subject, secret string) (*commerceMockPayer, error) {
	if err := validateCommerceMockMallURL(baseURL); err != nil {
		return nil, err
	}
	if !commerceSubjectPattern.MatchString(subject) {
		return nil, fmt.Errorf("payment subject 必须匹配 [a-z][a-z0-9-]{2,63}(与商城 COMMERCE_PAYMENT_SUBJECT 一致)")
	}
	// 只报长度与形状,不回显内容(AGENTS §11.3)。
	if len(secret) < commerceMinSecretLen || strings.TrimSpace(secret) != secret || strings.ContainsAny(secret, " \t\r\n") {
		return nil, fmt.Errorf("环境变量 %s 必须是至少 %d 字节、不含空白的商城 payment 密钥(当前长度 %d)",
			commercePaymentSecretEnv, commerceMinSecretLen, len(secret))
	}
	return &commerceMockPayer{
		baseURL: strings.TrimRight(baseURL, "/"),
		subject: subject,
		secret:  secret,
		client: &http.Client{
			Timeout: 5 * time.Second,
			// 不跟随跳转:误配的跳转不得把签名请求带去另一个地址。
			CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse },
			// 不走代理:回环地址本来就不该经代理,这里显式关掉,不依赖环境变量里的例外规则。
			Transport: &http.Transport{Proxy: nil},
		},
		now:   time.Now,
		nonce: commerceRandomNonce,
	}, nil
}

// commerceRandomNonce 生成 32 位小写 hex(满足商城 [A-Za-z0-9_-]{16,128})。每个请求一个,重发也换新的:
// 商城按 (角色, nonce) 去重,原样重放旧签名请求会被 409。
func commerceRandomNonce() (string, error) {
	raw := make([]byte, 16)
	if _, err := rand.Read(raw); err != nil {
		return "", err
	}
	return hex.EncodeToString(raw), nil
}

func validateCommercePayment(p commerceMockPay) error {
	switch {
	case !commerceOrderIDPattern.MatchString(p.OrderID):
		return fmt.Errorf("订单号 %q 不是商城的小写 UUID", p.OrderID)
	case !commerceMockRequestPattern.MatchString(p.RequestID):
		return fmt.Errorf("mock请求号必须匹配[A-Za-z0-9_-]{8,128}")
	}
	return nil
}

// 不自动重试未知提交；仅以同一原订单和requestId显式重放，传输nonce每次重新生成。
func (p *commerceMockPayer) RecordPayment(ctx context.Context, payment commerceMockPay) (commerceMallOrder, error) {
	if err := validateCommercePayment(payment); err != nil {
		return commerceMallOrder{}, err
	}
	body, err := json.Marshal(payment)
	if err != nil {
		return commerceMallOrder{}, err
	}
	return p.post(ctx, payment.OrderID, body)
}

// 冒烟反例：新mock接口必须拒绝调用方指定金额，不能退回旧渠道回执接口。
func (p *commerceMockPayer) ProbeAmountOverride(ctx context.Context, payment commerceMockPay, amount uint64) error {
	if err := validateCommercePayment(payment); err != nil {
		return err
	}
	body, err := json.Marshal(struct {
		OrderID     string `json:"orderId"`
		RequestID   string `json:"requestId"`
		AmountMinor uint64 `json:"amountMinor"`
	}{payment.OrderID, payment.RequestID, amount})
	if err != nil {
		return err
	}
	_, err = p.post(ctx, payment.OrderID, body)
	return err
}

func (p *commerceMockPayer) post(ctx context.Context, orderID string, body []byte) (commerceMallOrder, error) {
	nonce, err := p.nonce()
	if err != nil {
		return commerceMallOrder{}, fmt.Errorf("生成 nonce 失败: %w", err)
	}
	timestamp := strconv.FormatInt(p.now().UnixMilli(), 10)
	request, err := http.NewRequestWithContext(ctx, http.MethodPost, p.baseURL+commercePaymentPath, bytes.NewReader(body))
	if err != nil {
		return commerceMallOrder{}, err
	}
	request.Header.Set("Content-Type", "application/json")
	request.Header.Set("X-Commerce-Subject", p.subject)
	request.Header.Set("X-Commerce-Timestamp", timestamp)
	request.Header.Set("X-Commerce-Nonce", nonce)
	request.Header.Set("X-Commerce-Signature",
		commerceSign(p.secret, "payment", p.subject, timestamp, nonce, http.MethodPost, commercePaymentPath, body))
	response, err := p.client.Do(request)
	if err != nil {
		return commerceMallOrder{}, fmt.Errorf("连不上商城: %w", err)
	}
	defer response.Body.Close()
	raw, err := io.ReadAll(io.LimitReader(response.Body, commerceMaxResponseBytes+1))
	if err != nil {
		return commerceMallOrder{}, fmt.Errorf("读商城应答失败: %w", err)
	}
	if len(raw) > commerceMaxResponseBytes {
		return commerceMallOrder{}, errors.New("商城应答超过 64KiB")
	}
	if response.StatusCode != http.StatusOK {
		var reason struct {
			Error string `json:"error"`
		}
		_ = json.Unmarshal(raw, &reason)
		message := reason.Error
		if runes := []rune(message); len(runes) > 100 {
			message = string(runes[:100]) // 按字符截断:商城错误文本是中文,按字节切会切出半个字
		}
		return commerceMallOrder{}, &commerceHTTPError{Status: response.StatusCode, Message: message}
	}
	var order commerceMallOrder
	if err := json.Unmarshal(raw, &order); err != nil {
		return commerceMallOrder{}, fmt.Errorf("解析商城订单应答失败: %w", err)
	}
	if order.OrderID != orderID {
		return commerceMallOrder{}, fmt.Errorf("商城应答的订单号 %q 不是本次付款的 %q", order.OrderID, orderID)
	}
	return order, nil
}
