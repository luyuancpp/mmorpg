package config

import (
	"strings"
	"testing"

	"github.com/zeromicro/go-zero/core/conf"
)

// placementHolder 只含 Placement 一个字段,标签与 Config 上的完全相同,用来单独钉住 go-zero 对这一段的解析语义,
// 不必为了单测凑齐 Config 的其余必填段。
type placementHolder struct {
	Placement PlacementConfig `json:"Placement,optional"`
}

// 整段缺省必须等价于 AllowStoreFamilies=true、Required=false(player-storage-placement.md §6.1 / §6.2):
// go-zero 对整段 optional 且未出现的嵌套结构不回填内层 default,零值本身必须就是这个语义。
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

// etc/db.yaml 的示例段必须能被解析,且写出的就是缺省语义(段名 / 键名写错会静默保持零值)。
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
	if !c.Placement.StoreFamiliesAllowed() || c.Placement.Required {
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
