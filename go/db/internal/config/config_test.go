package config

import (
	"os"
	"reflect"
	"strings"
	"testing"

	"github.com/zeromicro/go-zero/core/conf"
)

func TestBlobGuardColumnDefaultsAndIsolation(t *testing.T) {
	input := BlobGuardConfig{PerTable: map[string]BlobTableLimit{
		"player_database": {ColumnBytes: map[string]int64{"currency": 1234}},
		"player_snapshot": {MaxColumnBytes: 8 << 20, ColumnBytes: map[string]int64{"data": 7 << 20}},
	}}
	got := input.WithDefaults()
	player := got.PerTable["player_database"]
	if got.MaxColumnBytes != 256<<10 || got.MaxRowBytes != 1<<20 || got.ReportOnly ||
		player.MaxRowBytes != 0 || player.MaxColumnBytes != 0 || player.ColumnBytes["asset_op_ledger"] != 1<<20 {
		t.Fatalf("资产账本默认预算未精准装配: %+v / %+v", got, player)
	}
	if player.ColumnBytes["currency"] != 1234 || got.PerTable["player_snapshot"].ColumnBytes["data"] != 7<<20 {
		t.Fatal("已有列预算必须保留")
	}
	if input.MaxColumnBytes != 0 || input.PerTable["player_database"].MaxRowBytes != 0 ||
		len(input.PerTable["player_database"].ColumnBytes) != 1 {
		t.Fatal("回填默认不得修改输入配置")
	}
	got.PerTable["player_database"].ColumnBytes["currency"] = 9
	got.PerTable["player_snapshot"].ColumnBytes["data"] = 10
	delete(got.PerTable, "player_snapshot")
	if input.PerTable["player_database"].ColumnBytes["currency"] != 1234 ||
		input.PerTable["player_snapshot"].ColumnBytes["data"] != 7<<20 {
		t.Fatal("表和列两层 map 必须独立克隆")
	}
	var normalized Config
	normalized.Normalize()
	before := normalized.ServerConfig.BlobGuard.WithDefaults()
	normalized.Normalize()
	if !reflect.DeepEqual(before, normalized.ServerConfig.BlobGuard) {
		t.Fatal("多次 Normalize 不得改变资产账本默认预算")
	}
}

func TestBlobGuardColumnYamlAndExplicitSmallerLimits(t *testing.T) {
	var holder struct {
		BlobGuard BlobGuardConfig `json:"BlobGuard,optional"`
	}
	text := "BlobGuard:\n  MaxColumnBytes: 1024\n  MaxRowBytes: 2048\n  WarnRatio: 0.5\n  PerTable:\n    player_database:\n      MaxColumnBytes: 512\n      MaxRowBytes: 1536\n      ColumnBytes:\n        asset_op_ledger: 768\n        currency: 256\n"
	if err := conf.LoadFromYamlBytes([]byte(text), &holder); err != nil {
		t.Fatalf("列覆盖 YAML 必须可解析: %v", err)
	}
	got := holder.BlobGuard.WithDefaults()
	player := got.PerTable["player_database"]
	if got.MaxColumnBytes != 1024 || got.MaxRowBytes != 2048 || got.WarnRatio != 0.5 ||
		player.MaxColumnBytes != 512 || player.MaxRowBytes != 1536 ||
		player.ColumnBytes["asset_op_ledger"] != 768 || player.ColumnBytes["currency"] != 256 {
		t.Fatalf("显式更小预算不能被默认放宽: %+v / %+v", got, player)
	}
	data, err := os.ReadFile("../../etc/db.yaml")
	if err != nil {
		t.Fatal(err)
	}
	var example Config
	if err := conf.LoadFromYamlBytes(data, &example); err != nil {
		t.Fatalf("示例 YAML 解析失败: %v", err)
	}
	player = example.ServerConfig.BlobGuard.PerTable["player_database"]
	if player.MaxRowBytes != 0 || player.ColumnBytes["asset_op_ledger"] != 1<<20 ||
		example.ServerConfig.BlobGuard.MaxColumnBytes != 256<<10 || example.ServerConfig.BlobGuard.MaxRowBytes != 1<<20 {
		t.Fatalf("示例必须显式声明精准预算，不能靠 Normalize 掩盖漏键: %+v", example.ServerConfig.BlobGuard)
	}
}

// placementHolder 只含 Placement 一个字段,标签与 Config 上的完全相同,用来单独钉住 go-zero 对这一段的解析语义,
// 不必为了单测凑齐 Config 的其余必填段。
type placementHolder struct {
	Placement PlacementConfig `json:"Placement,optional"`
}

// 字段零值保留旧解码形状，全球Config启动另由ValidateGlobalAuthority拒绝缺失门禁；
// 不用default/Normalize把显式false静默变true，历史形状仅在显式LegacyNonGlobal中可启动。
func TestPlacementBlockMissingMeansDefaults(t *testing.T) {
	var h placementHolder
	if err := conf.LoadFromYamlBytes([]byte("Name: db.rpc\n"), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	if !h.Placement.StoreFamiliesAllowed() {
		t.Fatal("a missing Placement block must allow store families")
	}
	if h.Placement.Required {
		t.Fatal("a missing Placement block must not require placement records")
	}
	if h.Placement.Redis != nil {
		t.Fatal("a missing Placement block must reuse RedisClient")
	}

	c := Config{Placement: h.Placement}
	c.Normalize()
	if c.Placement.ExtraStoreMaxOpenConn != DefaultPlacementExtraStoreMaxOpenConn ||
		c.Placement.ExtraStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("pool defaults = %d/%d, want %d/%d", c.Placement.ExtraStoreMaxOpenConn, c.Placement.ExtraStoreMaxIdleConn,
			DefaultPlacementExtraStoreMaxOpenConn, DefaultPlacementExtraStoreMaxIdleConn)
	}
}

// 写了段但没写 AllowStoreFamilies:同样按 true(*bool 的 nil),不能因为「写了半段」换一套默认值。
func TestPlacementPartialBlockKeepsFamilyDefault(t *testing.T) {
	var h placementHolder
	if err := conf.LoadFromYamlBytes([]byte("Placement:\n  Required: true\n"), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	if !h.Placement.StoreFamiliesAllowed() {
		t.Fatal("an omitted AllowStoreFamilies must stay true inside a present block")
	}
	if !h.Placement.Required {
		t.Fatal("Required=true must be honored")
	}
}

func TestPlacementExplicitValues(t *testing.T) {
	var h placementHolder
	yaml := "Placement:\n" +
		"  AllowStoreFamilies: false\n" +
		"  ExtraStoreMaxOpenConn: 16\n" +
		"  Redis:\n" +
		"    Hosts: \"10.0.0.1:6379\"\n" +
		"    DB: 3\n"
	if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	if h.Placement.StoreFamiliesAllowed() {
		t.Fatal("an explicit AllowStoreFamilies=false must disable store families")
	}
	if h.Placement.Redis == nil || h.Placement.Redis.Hosts != "10.0.0.1:6379" || h.Placement.Redis.DB != 3 {
		t.Fatalf("Placement.Redis = %+v", h.Placement.Redis)
	}

	c := Config{Placement: h.Placement}
	c.Normalize()
	if c.Placement.ExtraStoreMaxOpenConn != 16 {
		t.Fatalf("an explicit pool size must survive Normalize, got %d", c.Placement.ExtraStoreMaxOpenConn)
	}
	if c.Placement.ExtraStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("an omitted idle size falls back to the default, got %d", c.Placement.ExtraStoreMaxIdleConn)
	}
}

// Placement.Redis 段出现就必须带 Hosts:空的 Redis 段静默落到默认地址,读到的就不是 mapping Redis。
func TestPlacementRedisBlockRequiresHosts(t *testing.T) {
	var h placementHolder
	if err := conf.LoadFromYamlBytes([]byte("Placement:\n  Redis:\n    DB: 0\n"), &h); err == nil {
		t.Fatal("a Placement.Redis block without Hosts must be rejected")
	}
}

// etc/db.yaml必须能解析且显式写出全球默认，不能靠Normalize掩盖漏键。
func TestEtcYamlPlacementBlockMatchesDefaults(t *testing.T) {
	var c Config
	if err := conf.Load("../../etc/db.yaml", &c); err != nil {
		t.Fatalf("load etc/db.yaml: %v", err)
	}
	// Normalize 之前:示例 yaml 必须把两类池的四个新键与空闲秒数都写出来(键名写错会静默保持零值,
	// Normalize 之后又被回落值盖住,只看 Normalize 之后的值发现不了)。K8s ConfigMap 从这几个键镜像,总账也读它们。
	raw := c.Placement
	if raw.ZoneStoreMaxOpenConn <= 0 || raw.ZoneStoreMaxIdleConn <= 0 ||
		raw.NonZoneStoreMaxOpenConn <= 0 || raw.NonZoneStoreMaxIdleConn <= 0 || raw.ExtraStoreConnMaxIdleSeconds <= 0 {
		t.Fatalf("etc/db.yaml must spell out the split pool keys, got zone=%d/%d nonZone=%d/%d idleSeconds=%d",
			raw.ZoneStoreMaxOpenConn, raw.ZoneStoreMaxIdleConn, raw.NonZoneStoreMaxOpenConn, raw.NonZoneStoreMaxIdleConn,
			raw.ExtraStoreConnMaxIdleSeconds)
	}
	c.Normalize()
	if c.Placement.AllowStoreFamilies == nil {
		t.Fatal("etc/db.yaml must spell out AllowStoreFamilies so the example documents the key")
	}
	if !c.Placement.StoreFamiliesAllowed() || !c.Placement.Required {
		t.Fatalf("etc/db.yaml must ship the default semantics, got families=%v required=%v",
			c.Placement.StoreFamiliesAllowed(), c.Placement.Required)
	}
	if c.Placement.ZoneStoreMaxOpenConn != DefaultPlacementExtraStoreMaxOpenConn ||
		c.Placement.ZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn ||
		c.Placement.NonZoneStoreMaxOpenConn != DefaultPlacementExtraStoreMaxOpenConn ||
		c.Placement.NonZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("etc/db.yaml pools = zone %d/%d nonZone %d/%d, want both %d/%d",
			c.Placement.ZoneStoreMaxOpenConn, c.Placement.ZoneStoreMaxIdleConn,
			c.Placement.NonZoneStoreMaxOpenConn, c.Placement.NonZoneStoreMaxIdleConn,
			DefaultPlacementExtraStoreMaxOpenConn, DefaultPlacementExtraStoreMaxIdleConn)
	}
	if c.Placement.ExtraStoreConnMaxIdleSeconds != DefaultPlacementExtraStoreConnMaxIdleSeconds {
		t.Fatalf("etc/db.yaml ExtraStoreConnMaxIdleSeconds = %d, want %d",
			c.Placement.ExtraStoreConnMaxIdleSeconds, DefaultPlacementExtraStoreConnMaxIdleSeconds)
	}
	if c.Placement.Redis != nil {
		t.Fatal("etc/db.yaml must keep Placement.Redis commented out (reuse RedisClient) for local dev")
	}
}

// 两类额外落点库池(G2b 批 C)整段缺省时:都取默认 8 / 2,空闲回收 60s。
func TestPlacementSplitPoolsDefault(t *testing.T) {
	var h placementHolder
	if err := conf.LoadFromYamlBytes([]byte("Name: db.rpc\n"), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	c := Config{Placement: h.Placement}
	c.Normalize()
	p := c.Placement
	if p.ZoneStoreMaxOpenConn != DefaultPlacementExtraStoreMaxOpenConn || p.ZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("zone store pool = %d/%d, want %d/%d", p.ZoneStoreMaxOpenConn, p.ZoneStoreMaxIdleConn,
			DefaultPlacementExtraStoreMaxOpenConn, DefaultPlacementExtraStoreMaxIdleConn)
	}
	if p.NonZoneStoreMaxOpenConn != DefaultPlacementExtraStoreMaxOpenConn || p.NonZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("non-zone store pool = %d/%d, want %d/%d", p.NonZoneStoreMaxOpenConn, p.NonZoneStoreMaxIdleConn,
			DefaultPlacementExtraStoreMaxOpenConn, DefaultPlacementExtraStoreMaxIdleConn)
	}
	if p.ExtraStoreConnMaxIdleSeconds != DefaultPlacementExtraStoreConnMaxIdleSeconds {
		t.Fatalf("idle seconds = %d, want %d", p.ExtraStoreConnMaxIdleSeconds, DefaultPlacementExtraStoreConnMaxIdleSeconds)
	}

	// 可重复调用(Normalize 的契约):第二次不得改变任何值。
	before := c.Placement
	c.Normalize()
	if c.Placement != before {
		t.Fatalf("Normalize must be idempotent, Placement changed from %+v to %+v", before, c.Placement)
	}
}

// 只写旧键的旧部署:两类库都回落到旧键的值,维持原来的池大小。
func TestPlacementSplitPoolsFallBackToLegacyKeys(t *testing.T) {
	var h placementHolder
	yaml := "Placement:\n" +
		"  ExtraStoreMaxOpenConn: 16\n" +
		"  ExtraStoreMaxIdleConn: 4\n"
	if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	c := Config{Placement: h.Placement}
	c.Normalize()
	p := c.Placement
	if p.ZoneStoreMaxOpenConn != 16 || p.ZoneStoreMaxIdleConn != 4 {
		t.Fatalf("zone store pool = %d/%d, want legacy 16/4", p.ZoneStoreMaxOpenConn, p.ZoneStoreMaxIdleConn)
	}
	if p.NonZoneStoreMaxOpenConn != 16 || p.NonZoneStoreMaxIdleConn != 4 {
		t.Fatalf("non-zone store pool = %d/%d, want legacy 16/4", p.NonZoneStoreMaxOpenConn, p.NonZoneStoreMaxIdleConn)
	}
}

// 新键优先于旧键:只调大非 zone 池时,zone 家族池仍按旧键。
func TestPlacementSplitPoolsNewKeysWin(t *testing.T) {
	var h placementHolder
	yaml := "Placement:\n" +
		"  ExtraStoreMaxOpenConn: 16\n" +
		"  NonZoneStoreMaxOpenConn: 32\n" +
		"  ExtraStoreConnMaxIdleSeconds: 30\n"
	if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	c := Config{Placement: h.Placement}
	c.Normalize()
	p := c.Placement
	if p.ZoneStoreMaxOpenConn != 16 {
		t.Fatalf("zone store max open = %d, want the legacy fallback 16", p.ZoneStoreMaxOpenConn)
	}
	if p.NonZoneStoreMaxOpenConn != 32 {
		t.Fatalf("non-zone store max open = %d, want the explicit 32", p.NonZoneStoreMaxOpenConn)
	}
	if p.ZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn || p.NonZoneStoreMaxIdleConn != DefaultPlacementExtraStoreMaxIdleConn {
		t.Fatalf("idle pools = %d/%d, want the default %d", p.ZoneStoreMaxIdleConn, p.NonZoneStoreMaxIdleConn,
			DefaultPlacementExtraStoreMaxIdleConn)
	}
	if p.ExtraStoreConnMaxIdleSeconds != 30 {
		t.Fatalf("idle seconds = %d, want the explicit 30", p.ExtraStoreConnMaxIdleSeconds)
	}

	// 新旧键混写的起点上同样可重复调用。
	before := c.Placement
	c.Normalize()
	if c.Placement != before {
		t.Fatal("Normalize must be idempotent when legacy and new keys are mixed")
	}
}

// ── 读方就绪闸(D1 第 3 批,spec_D1_batch1b2_2_3.md §5.3 / §13.2)──────────────────────────

// 整段缺省 / 只写了别的键:RequireRouteReady=false(行为与引入之前相同),间隔经 Normalize 取 1000;显式值原样保留;
// Normalize 可重复调用。
func TestPlacementRouteReadyDefaults(t *testing.T) {
	for _, yaml := range []string{"Name: db.rpc\n", "Placement:\n  Required: true\n"} {
		var h placementHolder
		if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
			t.Fatalf("load %q: %v", yaml, err)
		}
		if h.Placement.RequireRouteReady {
			t.Fatalf("%q: an omitted RequireRouteReady must stay false", yaml)
		}
		c := Config{Placement: h.Placement}
		c.Normalize()
		if c.Placement.RouteReadyGateIntervalMs != DefaultPlacementRouteReadyGateIntervalMs {
			t.Fatalf("%q: interval = %d, want the default %d", yaml, c.Placement.RouteReadyGateIntervalMs,
				DefaultPlacementRouteReadyGateIntervalMs)
		}
		before := c.Placement
		c.Normalize()
		if c.Placement != before {
			t.Fatalf("%q: Normalize must be idempotent, Placement changed from %+v to %+v", yaml, before, c.Placement)
		}
	}

	var h placementHolder
	yaml := "Placement:\n" +
		"  RequireRouteReady: true\n" +
		"  RouteReadyGateIntervalMs: 500\n"
	if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	c := Config{Placement: h.Placement}
	c.Normalize()
	if !c.Placement.RequireRouteReady || c.Placement.RouteReadyGateIntervalMs != 500 {
		t.Fatalf("explicit values must survive Normalize, got require=%v interval=%d",
			c.Placement.RequireRouteReady, c.Placement.RouteReadyGateIntervalMs)
	}
	if DefaultPlacementRouteReadyGateIntervalMs != 1000 || MinPlacementRouteReadyGateIntervalMs != 200 ||
		MaxPlacementRouteReadyGateIntervalMs != 10000 {
		t.Fatal("route ready gate interval bounds changed: the spec states default 1000 within [200, 10000]")
	}
}

// 间隔越界一律拒启(不论开关取值),错误文本点名键;边界值与「没写」(Normalize 之前的 0)放行。
func TestPlacementRouteReadyIntervalValidate(t *testing.T) {
	for _, tc := range []struct {
		interval  int
		normalize bool
		wantErr   bool
	}{
		{interval: 0, normalize: false},
		{interval: 0, normalize: true},
		{interval: 200, normalize: true},
		{interval: 10000, normalize: true},
		{interval: 199, normalize: true, wantErr: true},
		{interval: 10001, normalize: true, wantErr: true},
		{interval: -1, normalize: true, wantErr: true},
		{interval: -1, normalize: false, wantErr: true},
	} {
		for _, require := range []bool{false, true} {
			c := legacyConfigForTest()
			c.Placement.RequireRouteReady = require
			c.Placement.RouteReadyGateIntervalMs = tc.interval
			if tc.normalize {
				c.Normalize()
			}
			err := c.Validate()
			if !tc.wantErr {
				if err != nil {
					t.Fatalf("interval=%d normalize=%v require=%v must pass, got %v", tc.interval, tc.normalize, require, err)
				}
				continue
			}
			if err == nil || !strings.Contains(err.Error(), "Placement.RouteReadyGateIntervalMs") {
				t.Fatalf("interval=%d normalize=%v require=%v: err = %v, want it to name Placement.RouteReadyGateIntervalMs",
					tc.interval, tc.normalize, require, err)
			}
		}
	}
}

// etc/db.yaml必须显式写ready门禁开、间隔1000；键名写错不能静默退回旧MGET。
func TestEtcYamlPlacementRouteReadyKeys(t *testing.T) {
	raw, err := os.ReadFile("../../etc/db.yaml")
	if err != nil {
		t.Fatalf("read etc/db.yaml: %v", err)
	}
	text := strings.ReplaceAll(string(raw), "\r\n", "\n")
	if !strings.Contains(text, "\n  RequireRouteReady: true\n") {
		t.Fatal("全球etc/db.yaml必须显式Placement.RequireRouteReady: true")
	}
	var c Config
	if err := conf.LoadFromYamlBytes([]byte(text), &c); err != nil {
		t.Fatalf("load etc/db.yaml: %v", err)
	}
	if !c.Placement.RequireRouteReady {
		t.Fatal("全球etc/db.yaml必须保持Placement.RequireRouteReady=true")
	}
	if c.Placement.RouteReadyGateIntervalMs != DefaultPlacementRouteReadyGateIntervalMs {
		t.Fatalf("etc/db.yaml must spell out Placement.RouteReadyGateIntervalMs=%d before Normalize, got %d",
			DefaultPlacementRouteReadyGateIntervalMs, c.Placement.RouteReadyGateIntervalMs)
	}
}

// ── 消费组按 zone 拆(docs/design/db-consumer-group-per-zone.md §2)──────────────────────────

// kafkaServerConfigHolder / kafkaHolder 只含 ServerConfig.Kafka 一段,标签与 Config 上的相同,
// 用来单独钉住 go-zero 对这一段的解析语义(同 placementHolder 的做法)。
type kafkaServerConfigHolder struct {
	Kafka KafkaConfig `json:"Kafka"`
}

type kafkaHolder struct {
	ServerConfig kafkaServerConfigHolder `json:"ServerConfig"`
}

// preHandoffKafkaConfig 与交接闸之前(HEAD 旧版)的 KafkaConfig 同形:GroupID 是必填的 `json:"GroupID"`。
// 只保留会影响「加载成不成功」的必填键(Brokers / GroupID / PartitionCnt / IsOfflineExpand),
// 用来在单测里代表旧二进制。
type preHandoffKafkaConfig struct {
	Brokers         []string `json:"Brokers"`
	GroupID         string   `json:"GroupID"`
	PartitionCnt    int32    `json:"PartitionCnt"`
	IsOfflineExpand bool     `json:"IsOfflineExpand"`
}

type preHandoffServerConfigHolder struct {
	Kafka preHandoffKafkaConfig `json:"Kafka"`
}

type preHandoffHolder struct {
	ServerConfig preHandoffServerConfigHolder `json:"ServerConfig"`
}

// 交接形状 handoff(Z←L):只写 ConsumerGroup + GroupHandoffFrom,不写 GroupID。
const handoffShapedKafkaYaml = "ServerConfig:\n" +
	"  Kafka:\n" +
	"    Brokers:\n" +
	"      - \"127.0.0.1:9092\"\n" +
	"    ConsumerGroup: \"db_rpc_consumer_group_z101\"\n" +
	"    GroupHandoffFrom: \"db_rpc_consumer_group\"\n" +
	"    PartitionCnt: 10\n" +
	"    IsOfflineExpand: false\n"

// 最终形状 final(Z):只写 GroupID。
const finalZoneShapedKafkaYaml = "ServerConfig:\n" +
	"  Kafka:\n" +
	"    Brokers:\n" +
	"      - \"127.0.0.1:9092\"\n" +
	"    GroupID: \"db_rpc_consumer_group_z101\"\n" +
	"    PartitionCnt: 10\n" +
	"    IsOfflineExpand: false\n"

// normalizedKafka 把一段 KafkaConfig 装进 Config 走一遍 Normalize,返回回填后的值。
func normalizedKafka(k KafkaConfig) KafkaConfig {
	c := Config{}
	c.ServerConfig.Kafka = k
	c.Normalize()
	return c.ServerConfig.Kafka
}

// 三个超时键不写时取 sarama 默认值(行为与引入之前相同);显式值原样保留;Normalize 可重复调用。
func TestKafkaTimeoutDefaults(t *testing.T) {
	k := normalizedKafka(KafkaConfig{GroupID: "g"})
	if k.SessionTimeoutMs != DefaultKafkaSessionTimeoutMs || k.HeartbeatIntervalMs != DefaultKafkaHeartbeatIntervalMs ||
		k.RebalanceTimeoutMs != DefaultKafkaRebalanceTimeoutMs {
		t.Fatalf("timeouts = %d/%d/%d, want %d/%d/%d", k.SessionTimeoutMs, k.HeartbeatIntervalMs, k.RebalanceTimeoutMs,
			DefaultKafkaSessionTimeoutMs, DefaultKafkaHeartbeatIntervalMs, DefaultKafkaRebalanceTimeoutMs)
	}
	if DefaultKafkaSessionTimeoutMs != 10000 || DefaultKafkaHeartbeatIntervalMs != 3000 || DefaultKafkaRebalanceTimeoutMs != 60000 {
		t.Fatal("defaults must equal sarama v1.43.1's NewConfig defaults (10s / 3s / 60s)")
	}

	explicit := normalizedKafka(KafkaConfig{GroupID: "g", SessionTimeoutMs: 30000, HeartbeatIntervalMs: 5000, RebalanceTimeoutMs: 90000})
	if explicit.SessionTimeoutMs != 30000 || explicit.HeartbeatIntervalMs != 5000 || explicit.RebalanceTimeoutMs != 90000 {
		t.Fatalf("explicit timeouts must survive Normalize, got %d/%d/%d",
			explicit.SessionTimeoutMs, explicit.HeartbeatIntervalMs, explicit.RebalanceTimeoutMs)
	}
	if again := normalizedKafka(explicit); again.SessionTimeoutMs != 30000 || again.HeartbeatIntervalMs != 5000 ||
		again.RebalanceTimeoutMs != 90000 {
		t.Fatal("Normalize must be idempotent for Kafka timeouts")
	}
}

func TestValidateConsumerGroup(t *testing.T) {
	for _, tc := range []struct {
		name    string
		k       KafkaConfig
		wantErr string // 空 = 必须通过;否则错误文本须包含它
	}{
		{name: "只有 GroupID(最终形状)", k: KafkaConfig{GroupID: "db_rpc_consumer_group_z101"}},
		{name: "ConsumerGroup + GroupHandoffFrom(交接形状)",
			k: KafkaConfig{ConsumerGroup: "db_rpc_consumer_group_z101", GroupHandoffFrom: "db_rpc_consumer_group"}},
		{name: "GroupID 与 ConsumerGroup 同时出现",
			k:       KafkaConfig{GroupID: "a", ConsumerGroup: "b"},
			wantErr: "set only one of"},
		{name: "两者都空", k: KafkaConfig{}, wantErr: "must be set"},
		{name: "GroupID + GroupHandoffFrom(旧二进制会错误接受的形状)",
			k:       KafkaConfig{GroupID: "db_rpc_consumer_group_z101", GroupHandoffFrom: "db_rpc_consumer_group"},
			wantErr: "requires Kafka.ConsumerGroup"},
		{name: "GroupHandoffFrom 等于组名",
			k:       KafkaConfig{ConsumerGroup: "db_rpc_consumer_group", GroupHandoffFrom: "db_rpc_consumer_group"},
			wantErr: "must differ"},
		{name: "heartbeat 4000 × 3 > session 10000",
			k:       KafkaConfig{GroupID: "g", HeartbeatIntervalMs: 4000, SessionTimeoutMs: 10000},
			wantErr: "HeartbeatIntervalMs"},
		{name: "rebalance 14999 < 15000",
			k:       KafkaConfig{GroupID: "g", RebalanceTimeoutMs: 14999},
			wantErr: "RebalanceTimeoutMs"},
		{name: "rebalance 恰好 15000", k: KafkaConfig{GroupID: "g", RebalanceTimeoutMs: 15000}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			err := normalizedKafka(tc.k).ValidateConsumerGroup()
			if tc.wantErr == "" {
				if err != nil {
					t.Fatalf("must pass, got %v", err)
				}
				return
			}
			if err == nil || !strings.Contains(err.Error(), tc.wantErr) {
				t.Fatalf("err = %v, want it to contain %q", err, tc.wantErr)
			}
		})
	}
}

func TestEffectiveGroupID(t *testing.T) {
	if got := (KafkaConfig{GroupID: "db_rpc_consumer_group"}).EffectiveGroupID(); got != "db_rpc_consumer_group" {
		t.Fatalf("final shape: got %q", got)
	}
	if got := (KafkaConfig{ConsumerGroup: "db_rpc_consumer_group_z7", GroupHandoffFrom: "db_rpc_consumer_group"}).EffectiveGroupID(); got != "db_rpc_consumer_group_z7" {
		t.Fatalf("handoff shape: got %q", got)
	}
	if got := (KafkaConfig{}).EffectiveGroupID(); got != "" {
		t.Fatalf("empty config: got %q", got)
	}
}

// 能力证明(F5 / F9,修订稿 R1):交接形状不写 GroupID,交接闸之前的二进制(GroupID 必填)在加载时就拒启,
// 而当前结构能加载同一份 YAML。go-zero「缺必填键报错」这一行为由本用例钉住,升级 go-zero 时它先红。
func TestPreHandoffBinaryRefusesZoneShapedConfig(t *testing.T) {
	var old preHandoffHolder
	err := conf.LoadFromYamlBytes([]byte(handoffShapedKafkaYaml), &old)
	if err == nil {
		t.Fatal("a pre-handoff binary must refuse the handoff-shaped config (missing required GroupID)")
	}
	if !strings.Contains(err.Error(), "GroupID") {
		t.Fatalf("the refusal must name GroupID, got %v", err)
	}

	var cur kafkaHolder
	if err := conf.LoadFromYamlBytes([]byte(handoffShapedKafkaYaml), &cur); err != nil {
		t.Fatalf("the current binary must load the handoff-shaped config: %v", err)
	}
	k := cur.ServerConfig.Kafka
	if k.ConsumerGroup != "db_rpc_consumer_group_z101" || k.GroupHandoffFrom != "db_rpc_consumer_group" || k.GroupID != "" {
		t.Fatalf("loaded ConsumerGroup=%q GroupHandoffFrom=%q GroupID=%q", k.ConsumerGroup, k.GroupHandoffFrom, k.GroupID)
	}
	if err := normalizedKafka(k).ValidateConsumerGroup(); err != nil {
		t.Fatalf("the handoff shape must validate: %v", err)
	}
}

// 修订稿 R1:最终形状 final(Z) 只写 GroupID,回档 Step 6 与全新 namespace 只渲染它 —— 任何版本的二进制都必须能加载。
func TestPreHandoffBinaryLoadsFinalZoneShape(t *testing.T) {
	var old preHandoffHolder
	if err := conf.LoadFromYamlBytes([]byte(finalZoneShapedKafkaYaml), &old); err != nil {
		t.Fatalf("a pre-handoff binary must load final(Z): %v", err)
	}
	if old.ServerConfig.Kafka.GroupID != "db_rpc_consumer_group_z101" {
		t.Fatalf("old binary GroupID = %q", old.ServerConfig.Kafka.GroupID)
	}

	var cur kafkaHolder
	if err := conf.LoadFromYamlBytes([]byte(finalZoneShapedKafkaYaml), &cur); err != nil {
		t.Fatalf("the current binary must load final(Z): %v", err)
	}
	k := normalizedKafka(cur.ServerConfig.Kafka)
	if err := k.ValidateConsumerGroup(); err != nil {
		t.Fatalf("final(Z) must validate: %v", err)
	}
	if k.EffectiveGroupID() != "db_rpc_consumer_group_z101" || k.GroupHandoffFrom != "" {
		t.Fatalf("final(Z) must join db_rpc_consumer_group_z101 without a handoff, got %q handoff=%q",
			k.EffectiveGroupID(), k.GroupHandoffFrom)
	}
}

// 旧形状 YAML 多带新增的超时键时,旧结构仍能加载:记录 go-zero 忽略未知键这一事实(F9)。
// 这条事实决定了「往最终形状里加超时键」不会让旧二进制拒启。
func TestPreHandoffBinaryIgnoresUnknownKeys(t *testing.T) {
	yaml := finalZoneShapedKafkaYaml +
		"    SessionTimeoutMs: 10000\n" +
		"    HeartbeatIntervalMs: 3000\n" +
		"    RebalanceTimeoutMs: 60000\n"
	var old preHandoffHolder
	if err := conf.LoadFromYamlBytes([]byte(yaml), &old); err != nil {
		t.Fatalf("a pre-handoff binary must ignore unknown keys, got %v", err)
	}
	if old.ServerConfig.Kafka.GroupID != "db_rpc_consumer_group_z101" {
		t.Fatalf("old binary GroupID = %q", old.ServerConfig.Kafka.GroupID)
	}
}

// etc/db.yaml 是本地默认配置,也是 k8s_deploy / 回档 / merge_zone 推导组名的基名来源:
// 必须是最终形状(只写 GroupID)、基名为 db_rpc_consumer_group、不带交接键,且能通过校验。
func TestEtcYamlConsumerGroupIsFinalShape(t *testing.T) {
	var c Config
	if err := conf.Load("../../etc/db.yaml", &c); err != nil {
		t.Fatalf("load etc/db.yaml: %v", err)
	}
	c.Normalize()
	k := c.ServerConfig.Kafka
	if k.GroupID != "db_rpc_consumer_group" || k.ConsumerGroup != "" || k.GroupHandoffFrom != "" {
		t.Fatalf("etc/db.yaml GroupID=%q ConsumerGroup=%q GroupHandoffFrom=%q, want the bare final shape",
			k.GroupID, k.ConsumerGroup, k.GroupHandoffFrom)
	}
	if err := k.ValidateConsumerGroup(); err != nil {
		t.Fatalf("etc/db.yaml must validate: %v", err)
	}
}

// ── 重试只交给分区属主 + 入队落盘确认(docs/design/db-retry-owner-routing.md)──────────────────────────

// 环境变量优先、拼错拒启(大小写敏感,不静默回落 shared),来源只用于启动日志。
func TestResolveRetryRouting(t *testing.T) {
	for _, tc := range []struct {
		name, configured, env string
		wantMode, wantSource  string
		wantErr               bool
	}{
		{name: "都没写", wantMode: RetryRoutingShared, wantSource: "default"},
		{name: "配置 owner", configured: "owner", wantMode: RetryRoutingOwner, wantSource: "config"},
		{name: "配置 shared", configured: "shared", wantMode: RetryRoutingShared, wantSource: "config"},
		{name: "环境变量优先", configured: "shared", env: "owner", wantMode: RetryRoutingOwner, wantSource: "env DB_RETRY_ROUTING"},
		{name: "环境变量去首尾空白", configured: "owner", env: " shared ", wantMode: RetryRoutingShared, wantSource: "env DB_RETRY_ROUTING"},
		{name: "环境变量只有空白视同没设", configured: "owner", env: "   ", wantMode: RetryRoutingOwner, wantSource: "config"},
		{name: "环境变量大小写不对", env: "Owner", wantErr: true},
		{name: "环境变量拼错", configured: "owner", env: "ownr", wantErr: true},
		{name: "配置拼错", configured: "bogus", wantErr: true},
		{name: "配置大小写不对", configured: "Owner", wantErr: true},
		{name: "配置带空格", configured: " owner", wantErr: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			mode, source, err := ResolveRetryRouting(tc.configured, tc.env)
			if tc.wantErr {
				if err == nil {
					t.Fatalf("ResolveRetryRouting(%q, %q) = %q/%q, want an error", tc.configured, tc.env, mode, source)
				}
				return
			}
			if err != nil {
				t.Fatalf("ResolveRetryRouting(%q, %q): %v", tc.configured, tc.env, err)
			}
			if mode != tc.wantMode || source != tc.wantSource {
				t.Fatalf("ResolveRetryRouting(%q, %q) = %q/%q, want %q/%q", tc.configured, tc.env, mode, source, tc.wantMode, tc.wantSource)
			}
		})
	}
	if RetryRoutingEnvVar != "DB_RETRY_ROUTING" {
		t.Fatalf("RetryRoutingEnvVar = %q: the rollout runbook (kubectl set env) is written against DB_RETRY_ROUTING", RetryRoutingEnvVar)
	}
}

// redisServerConfigHolder / redisHolder 只含 ServerConfig.RedisClient 一段,标签与 Config 上的相同(同 placementHolder 的做法)。
type redisServerConfigHolder struct {
	RedisClient RedisConfig `json:"RedisClient"`
}

type redisHolder struct {
	ServerConfig redisServerConfigHolder `json:"ServerConfig"`
}

const redisClientYaml = "ServerConfig:\n" +
	"  RedisClient:\n" +
	"    Hosts: \"127.0.0.1:6379\"\n" +
	"    DefaultTTLSeconds: 3600\n" +
	"    Password: \"\"\n" +
	"    DB: 0\n"

// loadRedisClient 加载一段只有 RedisClient 的 yaml,装进 Config 返回(未 Normalize)。
func loadRedisClient(t *testing.T, yaml string) Config {
	t.Helper()
	var h redisHolder
	if err := conf.LoadFromYamlBytes([]byte(yaml), &h); err != nil {
		t.Fatalf("load: %v", err)
	}
	c := globalConfigForTest()
	c.ServerConfig.RedisClient = h.ServerConfig.RedisClient
	return c
}

// 不写三个 WaitAOF 键:Normalize 之后是 1 / 0 / 1500,WaitsForAOF=true(默认就要确认落盘);可重复调用。
func TestRedisWaitAOFDefaultsWhenMissing(t *testing.T) {
	c := loadRedisClient(t, redisClientYaml)
	r := c.ServerConfig.RedisClient
	if r.WaitAOFNumLocal != nil || r.WaitAOFNumReplicas != nil || r.WaitAOFTimeoutMs != 0 {
		t.Fatalf("unset keys must load as nil / nil / 0, got %v / %v / %d", r.WaitAOFNumLocal, r.WaitAOFNumReplicas, r.WaitAOFTimeoutMs)
	}
	if !r.WaitsForAOF() || r.NumLocal() != DefaultRedisWaitAOFNumLocal || r.NumReplicas() != DefaultRedisWaitAOFNumReplicas {
		t.Fatal("accessors must read unset keys as the defaults even before Normalize")
	}
	// conf.Load 在 Normalize 之前就会调 Validate:没写的键必须放行。
	if err := c.Validate(); err != nil {
		t.Fatalf("an unnormalized config without the WaitAOF keys must validate: %v", err)
	}

	c.Normalize()
	r = c.ServerConfig.RedisClient
	if r.WaitAOFNumLocal == nil || r.WaitAOFNumReplicas == nil {
		t.Fatal("Normalize must fill both WaitAOF pointers")
	}
	if *r.WaitAOFNumLocal != 1 || *r.WaitAOFNumReplicas != 0 || r.WaitAOFTimeoutMs != 1500 {
		t.Fatalf("defaults = %d / %d / %d, want 1 / 0 / 1500", *r.WaitAOFNumLocal, *r.WaitAOFNumReplicas, r.WaitAOFTimeoutMs)
	}
	if DefaultRedisWaitAOFNumLocal != 1 || DefaultRedisWaitAOFNumReplicas != 0 || DefaultRedisWaitAOFTimeoutMs != 1500 {
		t.Fatal("WaitAOF defaults changed: the design doc and the alerts assume 1 / 0 / 1500")
	}
	if !r.WaitsForAOF() {
		t.Fatal("the default must wait for AOF")
	}
	if err := c.Validate(); err != nil {
		t.Fatalf("normalized defaults must validate: %v", err)
	}

	before := *r.WaitAOFNumLocal
	c.Normalize()
	if *c.ServerConfig.RedisClient.WaitAOFNumLocal != before || c.ServerConfig.RedisClient.WaitAOFTimeoutMs != 1500 {
		t.Fatal("Normalize must be idempotent for the WaitAOF keys")
	}
}

// 两项显式写 0 = 关闭 WAITAOF(只给没开 AOF 的环境):Normalize 不得把 0 当成没写,Validate 放行。
func TestRedisWaitAOFExplicitZeroDisables(t *testing.T) {
	c := loadRedisClient(t, redisClientYaml+
		"    WaitAOFNumLocal: 0\n"+
		"    WaitAOFNumReplicas: 0\n")
	r := c.ServerConfig.RedisClient
	if r.WaitAOFNumLocal == nil || r.WaitAOFNumReplicas == nil {
		t.Fatal("an explicit 0 must load as a non-nil pointer (otherwise it cannot be told apart from unset)")
	}
	c.Normalize()
	r = c.ServerConfig.RedisClient
	if *r.WaitAOFNumLocal != 0 || *r.WaitAOFNumReplicas != 0 {
		t.Fatalf("explicit zeros must survive Normalize, got %d / %d", *r.WaitAOFNumLocal, *r.WaitAOFNumReplicas)
	}
	if r.WaitsForAOF() {
		t.Fatal("WaitAOFNumLocal=0 and WaitAOFNumReplicas=0 must disable WAITAOF")
	}
	if r.WaitAOFTimeoutMs != DefaultRedisWaitAOFTimeoutMs {
		t.Fatalf("an unset timeout still takes the default, got %d", r.WaitAOFTimeoutMs)
	}
	if err := c.Validate(); err != nil {
		t.Fatalf("disabling WAITAOF must validate: %v", err)
	}

	// 只关一项不算关闭:NumReplicas=1 仍要等副本。
	half := loadRedisClient(t, redisClientYaml+
		"    WaitAOFNumLocal: 0\n"+
		"    WaitAOFNumReplicas: 1\n")
	half.Normalize()
	if !half.ServerConfig.RedisClient.WaitsForAOF() {
		t.Fatal("NumLocal=0 with NumReplicas=1 must still wait for AOF")
	}
}

func intPtr(v int) *int { return &v }

// 每一项写错都拒启,错误文本点名键;边界值放行。
func TestConfigValidateRejects(t *testing.T) {
	base := func() Config {
		c := globalConfigForTest()
		c.Normalize()
		return c
	}
	for _, tc := range []struct {
		name    string
		mutate  func(c *Config)
		wantErr string // 空 = 必须通过
	}{
		{name: "默认值", mutate: func(c *Config) {}},
		{name: "RetryRouting owner", mutate: func(c *Config) { c.ServerConfig.Kafka.RetryRouting = RetryRoutingOwner }},
		{name: "RetryRouting 大小写不对", mutate: func(c *Config) { c.ServerConfig.Kafka.RetryRouting = "Owner" },
			wantErr: "ServerConfig.Kafka.RetryRouting"},
		{name: "RetryRouting 拼错", mutate: func(c *Config) { c.ServerConfig.Kafka.RetryRouting = "own" },
			wantErr: "ServerConfig.Kafka.RetryRouting"},
		{name: "NumLocal=2", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFNumLocal = intPtr(2) },
			wantErr: "WaitAOFNumLocal"},
		{name: "NumLocal=-1", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFNumLocal = intPtr(-1) },
			wantErr: "WaitAOFNumLocal"},
		{name: "NumReplicas=-1", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFNumReplicas = intPtr(-1) },
			wantErr: "WaitAOFNumReplicas"},
		{name: "NumReplicas=17", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFNumReplicas = intPtr(17) },
			wantErr: "WaitAOFNumReplicas"},
		{name: "NumReplicas=16 边界", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFNumReplicas = intPtr(16) }},
		{name: "TimeoutMs=-1", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFTimeoutMs = -1 },
			wantErr: "WaitAOFTimeoutMs"},
		{name: "TimeoutMs=2001", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFTimeoutMs = 2001 },
			wantErr: "WaitAOFTimeoutMs"},
		{name: "TimeoutMs=2000 边界", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFTimeoutMs = 2000 }},
		{name: "TimeoutMs=1 边界", mutate: func(c *Config) { c.ServerConfig.RedisClient.WaitAOFTimeoutMs = 1 }},
		{name: "Fencing 缺省", mutate: func(c *Config) { c.ServerConfig.Fencing.Mode = "" }},
		{name: "Fencing legacy", mutate: func(c *Config) { c.ServerConfig.Fencing.Mode = FencingModeLegacy }},
		{name: "Fencing dual", mutate: func(c *Config) { c.ServerConfig.Fencing.Mode = FencingModeDual }},
		{name: "Fencing 大小写不对", mutate: func(c *Config) { c.ServerConfig.Fencing.Mode = "Dual" },
			wantErr: "ServerConfig.Fencing.Mode"},
		{name: "Fencing 拼错", mutate: func(c *Config) { c.ServerConfig.Fencing.Mode = "sql" },
			wantErr: "ServerConfig.Fencing.Mode"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			c := base()
			tc.mutate(&c)
			err := c.Validate()
			if tc.wantErr == "" {
				if err != nil {
					t.Fatalf("must pass, got %v", err)
				}
				return
			}
			if err == nil || !strings.Contains(err.Error(), tc.wantErr) {
				t.Fatalf("err = %v, want it to name %q", err, tc.wantErr)
			}
		})
	}
	if MaxRedisWaitAOFNumReplicas != 16 || MaxRedisWaitAOFTimeoutMs != 2000 {
		t.Fatal("WaitAOF upper bounds changed: the design doc states [0,16] and [1,2000]")
	}
}

// 仓库自带的 etc/db.yaml:两个新键都必须显式写出(K8s 的 ConfigMap 用 Get-AuthoritativeScalar 镜像它们,缺键即
// 生成失败),取值合法且不得悄悄关掉 WAITAOF。不断言取值等于 shared 或默认常量:rollout 第 4 步把它改成 owner、
// redis-ha 接入时把 NumReplicas 改成 1,都不应该让这条测试变红(ConfigMap 与 db.yaml 一致由契约测试守住)。
func TestEtcYamlRetryKeysValid(t *testing.T) {
	var c Config
	if err := conf.Load("../../etc/db.yaml", &c); err != nil {
		t.Fatalf("load etc/db.yaml: %v", err)
	}
	// Normalize 之前:键必须写出来(键名写错会静默保持零值 / nil)。
	routing := c.ServerConfig.Kafka.RetryRouting
	if routing == "" {
		t.Fatal("etc/db.yaml must spell out ServerConfig.Kafka.RetryRouting")
	}
	if _, source, err := ResolveRetryRouting(routing, ""); err != nil || source != "config" {
		t.Fatalf("etc/db.yaml RetryRouting=%q must resolve from config, got source=%q err=%v", routing, source, err)
	}
	r := c.ServerConfig.RedisClient
	if r.WaitAOFNumLocal == nil || r.WaitAOFNumReplicas == nil || r.WaitAOFTimeoutMs <= 0 {
		t.Fatalf("etc/db.yaml must spell out WaitAOFNumLocal / WaitAOFNumReplicas / WaitAOFTimeoutMs, got %v / %v / %d",
			r.WaitAOFNumLocal, r.WaitAOFNumReplicas, r.WaitAOFTimeoutMs)
	}

	c.Normalize()
	if err := c.Validate(); err != nil {
		t.Fatalf("etc/db.yaml must validate: %v", err)
	}
	if !c.ServerConfig.RedisClient.WaitsForAOF() {
		t.Fatal("etc/db.yaml must not disable WAITAOF (only environments without AOF write both keys as 0)")
	}
}

// 回滚兼容(修订稿 rulings 第 8 条):回滚到 S1 之前的镜像时,旧二进制要读带新键(RetryRouting、WaitAOF*)的 ConfigMap。
// go-zero v1.10.0 忽略未知键(core/mapping 只按结构体字段取值),所以能起;这条用例在顶层、Kafka 段、RedisClient 段
// 各注入一个结构体里没有的键,加载必须成功。以后升级 go-zero 若改成严格模式,这里先红,回滚规程就要改成
// 「先生成不含新键的 ConfigMap 再回滚镜像」。
func TestConfigLoaderIgnoresUnknownKeys(t *testing.T) {
	raw, err := os.ReadFile("../../etc/db.yaml")
	if err != nil {
		t.Fatalf("read etc/db.yaml: %v", err)
	}
	text := strings.ReplaceAll(string(raw), "\r\n", "\n")
	for _, inject := range []struct{ anchor, replacement string }{
		{"\nServerConfig:\n", "\nFutureTopLevelKeyForRollbackTest: 1\nServerConfig:\n"},
		{"\n  Kafka:\n", "\n  Kafka:\n    FutureKafkaKeyForRollbackTest: \"x\"\n"},
		{"\n  RedisClient:\n", "\n  RedisClient:\n    FutureRedisKeyForRollbackTest: 7\n"},
	} {
		if strings.Count(text, inject.anchor) != 1 {
			t.Fatalf("etc/db.yaml must contain the anchor %q exactly once (the injection would silently miss otherwise)", inject.anchor)
		}
		text = strings.Replace(text, inject.anchor, inject.replacement, 1)
	}
	for _, key := range []string{"FutureTopLevelKeyForRollbackTest", "FutureKafkaKeyForRollbackTest", "FutureRedisKeyForRollbackTest"} {
		if !strings.Contains(text, key) {
			t.Fatalf("injection of %s failed", key)
		}
	}

	var c Config
	if err := conf.LoadFromYamlBytes([]byte(text), &c); err != nil {
		t.Fatalf("unknown keys must be ignored by the config loader (rollback to a pre-S1 image depends on it): %v", err)
	}
	if c.ServerConfig.Kafka.PartitionCnt <= 0 || c.ServerConfig.RedisClient.Hosts == "" {
		t.Fatalf("the rest of etc/db.yaml must still load, got PartitionCnt=%d RedisClient.Hosts=%q",
			c.ServerConfig.Kafka.PartitionCnt, c.ServerConfig.RedisClient.Hosts)
	}

	// 旧二进制的 KafkaConfig 没有 RetryRouting:用交接闸之前的结构(preHandoffKafkaConfig)代表它,读带新键的 Kafka 段。
	var old preHandoffHolder
	if err := conf.LoadFromYamlBytes([]byte(finalZoneShapedKafkaYaml+
		"    RetryRouting: owner\n"+
		"    FutureKafkaKeyForRollbackTest: \"x\"\n"), &old); err != nil {
		t.Fatalf("a struct without RetryRouting must load a Kafka block that has it: %v", err)
	}
}
