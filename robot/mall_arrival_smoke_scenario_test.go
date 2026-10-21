package main

import (
	"regexp"
	"strings"
	"testing"

	tradepb "proto/trade"
	"robot/config"
)

func TestMallArrivalRequestIDFitsTradeAndMallLimits(t *testing.T) {
	id := mallArrivalRequestID(1760000000123, "0a1b2c3d")
	// trade 校验 8..80 字节;商城校验 [A-Za-z0-9_-]{8,128}。
	if len(id) < 8 || len(id) > 80 || !regexp.MustCompile(`^[A-Za-z0-9_-]{8,128}$`).MatchString(id) {
		t.Fatalf("请求号 %q 不满足 trade / 商城的约束", id)
	}
	if mallArrivalRequestID(1760000000123, "0a1b2c3d") == mallArrivalRequestID(1760000000123, "0a1b2c3e") {
		t.Fatal("随机后缀不同必须得到不同的请求号")
	}
}

func TestMallArrivalTransactionIDIsStablePerOrderAndMatchesMallPattern(t *testing.T) {
	order := "00112233-4455-6677-8899-aabbccddeeff"
	got := mallArrivalTransactionID(order)
	if got != mallArrivalTransactionID(order) {
		t.Fatal("同一订单必须得到同一个流水号(重发靠五元组相同判幂等)")
	}
	if got != "mockdev-00112233445566778899aabbccddeeff" || !commerceTransactionIDPattern.MatchString(got) {
		t.Fatalf("流水号 %q 不符合商城 [A-Za-z0-9_-]{1,128}", got)
	}
	if got == mallArrivalTransactionID("ffffffff-4455-6677-8899-aabbccddeeff") {
		t.Fatal("不同订单必须得到不同流水号(商城 (channel, transaction_id) 全局唯一)")
	}
}

func mallArrivalTestSku() *tradepb.OfficialSkuView {
	return &tradepb.OfficialSkuView{SkuId: 990001, Title: "回血药 ×2", PriceMinor: 600, Currency: "CNY", Available: 100}
}

func mallArrivalTestOrder() *tradepb.CommerceOrderView {
	return &tradepb.CommerceOrderView{
		OrderId: "00112233-4455-6677-8899-aabbccddeeff", Kind: tradepb.CommerceOrderKind_COMMERCE_ORDER_KIND_OFFICIAL,
		State: tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_AWAITING_PAYMENT, BuyerPlayerId: 22,
		Amount: 1200, Currency: "CNY", RelatedId: "990001",
	}
}

func TestMallArrivalCheckCreatedRejectsEachWrongField(t *testing.T) {
	if err := mallArrivalCheckCreated(mallArrivalTestOrder(), 22, mallArrivalTestSku(), 2); err != nil {
		t.Fatalf("合法下单应答被拒: %v", err)
	}
	cases := []struct {
		name string
		edit func(*tradepb.CommerceOrderView)
	}{
		{"订单号不是 UUID", func(o *tradepb.CommerceOrderView) { o.OrderId = "12345" }},
		{"不是官方单", func(o *tradepb.CommerceOrderView) { o.Kind = tradepb.CommerceOrderKind_COMMERCE_ORDER_KIND_CONSIGNMENT }},
		{"不是待付款", func(o *tradepb.CommerceOrderView) {
			o.State = tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERING
		}},
		{"买家不是自己", func(o *tradepb.CommerceOrderView) { o.BuyerPlayerId = 23 }},
		{"金额不是单价×份数", func(o *tradepb.CommerceOrderView) { o.Amount = 600 }},
		{"币种不一致", func(o *tradepb.CommerceOrderView) { o.Currency = "USD" }},
		{"关联的不是本 SKU", func(o *tradepb.CommerceOrderView) { o.RelatedId = "990002" }},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			order := mallArrivalTestOrder()
			c.edit(order)
			if err := mallArrivalCheckCreated(order, 22, mallArrivalTestSku(), 2); err == nil {
				t.Fatal("应当拒绝")
			}
		})
	}
	if err := mallArrivalCheckCreated(nil, 22, mallArrivalTestSku(), 2); err == nil {
		t.Fatal("没有 order 必须拒绝")
	}
}

func TestMallArrivalDeliveryVerdict(t *testing.T) {
	cases := []struct {
		state   tradepb.CommerceOrderState
		done    bool
		failing bool
	}{
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERED, true, false},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERING, false, false},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_REFUND_PENDING, false, true},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_MANUAL, false, true},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_CANCELLED, false, true},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_REFUNDED, false, true},
		// 商城已记账付款后仍显示待付款是缺陷,不能当「还在路上」一直等。
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_AWAITING_PAYMENT, false, true},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_SETTLED, false, true},
		{tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_UNSPECIFIED, false, true},
	}
	for _, c := range cases {
		done, err := mallArrivalDeliveryVerdict(c.state)
		if done != c.done || (err != nil) != c.failing {
			t.Fatalf("state=%v: done=%v err=%v,期望 done=%v failing=%v", c.state, done, err, c.done, c.failing)
		}
	}
}

func TestMallArrivalLeftoversPicksOnlyThisSkusMallOrders(t *testing.T) {
	unpaid := mallArrivalTestOrder()
	delivering := mallArrivalTestOrder()
	delivering.OrderId = "11112233-4455-6677-8899-aabbccddeeff"
	delivering.State = tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERING
	delivered := mallArrivalTestOrder()
	delivered.OrderId = "22222233-4455-6677-8899-aabbccddeeff"
	delivered.State = tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERED
	otherSku := mallArrivalTestOrder()
	otherSku.OrderId = "33332233-4455-6677-8899-aabbccddeeff"
	otherSku.RelatedId = "990002"
	// trade 自己的合同记录(数字号)与寄售单都不归本场景处理。
	tradeRecord := mallArrivalTestOrder()
	tradeRecord.OrderId = "9001"
	tradeRecord.State = tradepb.CommerceOrderState_COMMERCE_ORDER_STATE_DELIVERING
	consignment := mallArrivalTestOrder()
	consignment.OrderId = "44442233-4455-6677-8899-aabbccddeeff"
	consignment.Kind = tradepb.CommerceOrderKind_COMMERCE_ORDER_KIND_CONSIGNMENT

	gotUnpaid, gotDelivering := mallArrivalLeftovers(
		[]*tradepb.CommerceOrderView{unpaid, delivering, delivered, otherSku, tradeRecord, consignment}, 990001)
	if len(gotUnpaid) != 1 || gotUnpaid[0] != unpaid {
		t.Fatalf("未付单挑错了: %v", gotUnpaid)
	}
	if len(gotDelivering) != 1 || gotDelivering[0] != delivering {
		t.Fatalf("发货中的单挑错了: %v", gotDelivering)
	}
}

func TestMallArrivalTipHintCoversTheTradeMappings(t *testing.T) {
	for _, tip := range []uint32{tipMallFeatureDisabled, tipMallUnavailable, tipMallOrderNotFound, tipMallStateConflict,
		tipMallRequestConflict, tipMallInvalid, tipMallRateLimited} {
		if mallArrivalTipHint(tip) == "" {
			t.Fatalf("tip=%d 没有排障提示", tip)
		}
	}
}

func TestMallArrivalSmokeShippedYamlLoads(t *testing.T) {
	cfg, err := config.Load("etc/mall_arrival_smoke.yaml")
	if err != nil {
		t.Fatalf("load etc/mall_arrival_smoke.yaml: %v", err)
	}
	sc := cfg.MallArrivalSmoke
	if cfg.Mode != "mall-arrival-smoke" || sc.RunMode != "dev" || cfg.RobotCount != 1 {
		t.Fatalf("mode=%q run_mode=%q robot_count=%d", cfg.Mode, sc.RunMode, cfg.RobotCount)
	}
	// 与 etc/mall_arrival_smoke_seed.sql 种的那条商品一致:990001 → config 10 × 2。
	if sc.SkuID != 990001 || sc.ItemConfigID != 10 || sc.CountPerUnit != 2 || !strings.HasPrefix(sc.Channel, commerceMockChannelPrefix) {
		t.Fatalf("随仓配置与种子脚本不一致: %+v", sc)
	}
	if err := validateCommerceMockMallURL(sc.MallURL); err != nil {
		t.Fatalf("随仓 mall_url 过不了付款器的回环校验: %v", err)
	}
}

func TestMallArrivalSmokeConfigRejectsUnsafeValues(t *testing.T) {
	valid := func() config.MallArrivalSmokeConfig {
		return config.MallArrivalSmokeConfig{RunMode: "dev", MallURL: "http://127.0.0.1:18120", PaymentSubject: "payment-adapter",
			Channel: "mock-dev", SkuID: 990001, Quantity: 1, ItemConfigID: 10, CountPerUnit: 2, DeliverTimeoutSeconds: 90, PollIntervalMs: 1500}
	}
	base := valid()
	if err := base.Validate(); err != nil {
		t.Fatalf("合法配置被拒: %v", err)
	}
	cases := []struct {
		name string
		edit func(*config.MallArrivalSmokeConfig)
	}{
		{"没写 dev", func(c *config.MallArrivalSmokeConfig) { c.RunMode = "" }},
		{"prod", func(c *config.MallArrivalSmokeConfig) { c.RunMode = "prod" }},
		{"非回环商城", func(c *config.MallArrivalSmokeConfig) { c.MallURL = "http://192.168.1.5:18120" }},
		{"主机名商城", func(c *config.MallArrivalSmokeConfig) { c.MallURL = "http://localhost:18120" }},
		{"商城带路径", func(c *config.MallArrivalSmokeConfig) { c.MallURL = "http://127.0.0.1:18120/mall" }},
		{"商城缺端口", func(c *config.MallArrivalSmokeConfig) { c.MallURL = "http://127.0.0.1" }},
		{"真实渠道名", func(c *config.MallArrivalSmokeConfig) { c.Channel = "wechat" }},
		{"subject 非法", func(c *config.MallArrivalSmokeConfig) { c.PaymentSubject = "X" }},
		{"没写 sku", func(c *config.MallArrivalSmokeConfig) { c.SkuID = 0 }},
		{"份数 0", func(c *config.MallArrivalSmokeConfig) { c.Quantity = 0 }},
		{"份数过大", func(c *config.MallArrivalSmokeConfig) { c.Quantity = 17 }},
		{"发放超 4096", func(c *config.MallArrivalSmokeConfig) { c.Quantity = 16; c.CountPerUnit = 257 }},
		{"没写物品", func(c *config.MallArrivalSmokeConfig) { c.ItemConfigID = 0 }},
		{"等发货太短", func(c *config.MallArrivalSmokeConfig) { c.DeliverTimeoutSeconds = 5 }},
		{"轮询快过默认限流", func(c *config.MallArrivalSmokeConfig) { c.PollIntervalMs = 500 }},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			cfg := valid()
			c.edit(&cfg)
			if err := cfg.Validate(); err == nil {
				t.Fatal("应当拒绝")
			}
		})
	}
}
