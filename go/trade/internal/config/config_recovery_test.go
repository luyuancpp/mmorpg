package config

// 落地恢复(docs/design/landing-recovery-sync-digest.md §6.4)在 trade 配置上的回归:停放档 / 自动放弃 / 日对账 /
// 落地唤醒 / 终态行清理这几组键与 guild 同名同默认,默认值的事实源是 shared/assetop.DefaultLoopConfig 与
// shared/assetopwake 的 Default*;越界在 Enabled(清理是 CleanupEnabled)时拒启,关着时不查。

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
	"time"

	"shared/assetop"
	"shared/assetopwake"

	"github.com/zeromicro/go-zero/core/conf"
)

// enabledAssetOp 是一份开着通道、落地恢复参数全取默认值的 AssetOp 段,负向用例在它上面逐项改坏。
func enabledAssetOp() AssetOpConf {
	return AssetOpConf{
		Enabled:                true,
		SecretEnv:              AssetOpSecretEnvPrefix + "TRADE",
		OfflineAfterAttempts:   5,
		OfflineBackoffMs:       1800000,
		AbandonAfterDays:       180,
		AbandonSafetyGapMs:     3600000,
		AbandonBatch:           200,
		AbandonIntervalMs:      3600000,
		AuditIntervalMs:        86400000,
		CleanupEnabled:         true,
		CleanupIntervalMinutes: 10,
		TerminalRetentionDays:  30,
	}
}

// assertRecoveryMatchesAssetopDefaults 比对 AssetOp 段的落地恢复参数与 assetop / assetopwake 的默认值。
// 两边分叉的表现是"同一份默认配置,guild 与 trade 的停放 / 放弃节律不一样",而 guild 的配置守的是同一组值。
func assertRecoveryMatchesAssetopDefaults(t *testing.T, a AssetOpConf) {
	t.Helper()
	d := assetop.DefaultLoopConfig()
	ms := func(v int) time.Duration { return time.Duration(v) * time.Millisecond }
	if uint32(a.OfflineAfterAttempts) != d.OfflineAfterAttempts {
		t.Errorf("OfflineAfterAttempts = %d, want %d", a.OfflineAfterAttempts, d.OfflineAfterAttempts)
	}
	if ms(a.OfflineBackoffMs) != d.OfflineBackoff {
		t.Errorf("OfflineBackoffMs = %d, want %v", a.OfflineBackoffMs, d.OfflineBackoff)
	}
	if time.Duration(a.AbandonAfterDays)*24*time.Hour != d.AbandonAfter {
		t.Errorf("AbandonAfterDays = %d, want %v", a.AbandonAfterDays, d.AbandonAfter)
	}
	if ms(a.AbandonSafetyGapMs) != d.AbandonSafetyGap {
		t.Errorf("AbandonSafetyGapMs = %d, want %v", a.AbandonSafetyGapMs, d.AbandonSafetyGap)
	}
	if a.AbandonBatch != d.AbandonBatch {
		t.Errorf("AbandonBatch = %d, want %d", a.AbandonBatch, d.AbandonBatch)
	}
	if ms(a.AbandonIntervalMs) != d.AbandonInterval {
		t.Errorf("AbandonIntervalMs = %d, want %v", a.AbandonIntervalMs, d.AbandonInterval)
	}
	if ms(a.AuditIntervalMs) != d.AuditInterval {
		t.Errorf("AuditIntervalMs = %d, want %v", a.AuditIntervalMs, d.AuditInterval)
	}
	if !a.Wake.On() {
		t.Error("Wake 默认必须订阅")
	}
	if a.Wake.EffectiveConcurrency() != assetopwake.DefaultConcurrency {
		t.Errorf("Wake 生效并发 = %d, want %d", a.Wake.EffectiveConcurrency(), assetopwake.DefaultConcurrency)
	}
	if a.Wake.EffectiveTimeout() != assetopwake.DefaultWakeTimeout {
		t.Errorf("Wake 生效超时 = %v, want %v", a.Wake.EffectiveTimeout(), assetopwake.DefaultWakeTimeout)
	}
}

// TestEtcYamlAssetOpRecoveryBlock:etc/trade.yaml 写出的落地恢复与清理键与默认值一致,且通道一旦打开就能过校验
// (yaml 里写着却过不了校验的值,会在有人第一次把 Enabled 改成 true 时才炸)。
func TestEtcYamlAssetOpRecoveryBlock(t *testing.T) {
	var c Config
	if err := conf.Load(etcYaml, &c); err != nil {
		t.Fatalf("load %s: %v", etcYaml, err)
	}
	assertRecoveryMatchesAssetopDefaults(t, c.AssetOp)
	if !c.AssetOp.CleanupEnabled || c.AssetOp.CleanupIntervalMinutes != 10 || c.AssetOp.TerminalRetentionDays != 30 {
		t.Errorf("清理键 = (%v, %d, %d), want (true, 10, 30)(与 guild 同值)",
			c.AssetOp.CleanupEnabled, c.AssetOp.CleanupIntervalMinutes, c.AssetOp.TerminalRetentionDays)
	}
	if err := c.AssetOp.validateRecovery(); err != nil {
		t.Errorf("yaml 里的落地恢复参数在通道打开时过不了校验: %v", err)
	}
	if err := c.AssetOp.validateCleanup(); err != nil {
		t.Errorf("yaml 里的清理参数过不了校验: %v", err)
	}
}

// TestAssetOpKeysAbsentFillTagDefaults:AssetOp 段在、落地恢复键都没写时,go-zero 按 struct tag 回填的默认值
// 必须等于 assetop / assetopwake 的默认值(tag 只能写字面量,这里守住两处相等);Wake 段整段缺失 = 订阅、取默认值;
// 清理默认关(optional),间隔与保留期的 tag 默认与 guild 同值。
func TestAssetOpKeysAbsentFillTagDefaults(t *testing.T) {
	raw, err := os.ReadFile(etcYaml)
	if err != nil {
		t.Fatalf("read %s: %v", etcYaml, err)
	}
	minimal := "AssetOp:\n  Enabled: false\n  SecretEnv: " + AssetOpSecretEnvPrefix + "TRADE\n"
	// 换掉 `AssetOp:` 与它下面的缩进行(含段内缩进的注释);段上方的顶格注释留着无害。
	replaced := regexp.MustCompile(`(?m)^AssetOp:\r?\n(?:[ \t]+.*\r?\n?)*`).ReplaceAllString(string(raw), minimal)
	if strings.Contains(replaced, "OfflineAfterAttempts") {
		t.Fatal("AssetOp 段没被换干净,本用例失去意义")
	}
	path := filepath.Join(t.TempDir(), "trade.yaml")
	if err := os.WriteFile(path, []byte(replaced), 0o600); err != nil {
		t.Fatalf("write temp yaml: %v", err)
	}
	var c Config
	if err := conf.Load(path, &c); err != nil {
		t.Fatalf("只写开关与密钥来源的 AssetOp 段必须能加载: %v", err)
	}
	assertRecoveryMatchesAssetopDefaults(t, c.AssetOp)
	if c.AssetOp.Wake.Enabled != nil || c.AssetOp.Wake.Concurrency != 0 || c.AssetOp.Wake.TimeoutMs != 0 {
		t.Errorf("Wake 段缺失时应全是零值(默认值走方法),得到 %+v", c.AssetOp.Wake)
	}
	if c.AssetOp.CleanupEnabled {
		t.Error("CleanupEnabled 没写时必须是 false(optional)")
	}
	if c.AssetOp.CleanupIntervalMinutes != 10 || c.AssetOp.TerminalRetentionDays != 30 {
		t.Errorf("清理 tag 默认 = (%d, %d), want (10, 30)", c.AssetOp.CleanupIntervalMinutes, c.AssetOp.TerminalRetentionDays)
	}
}

// TestAssetOpWakeConfAccessors:Enabled 用 *bool 区分"没写"(开)与"显式 false"(关);0 取 assetopwake 默认值。
func TestAssetOpWakeConfAccessors(t *testing.T) {
	yes, no := true, false
	if !(AssetOpWakeConf{}).On() || !(AssetOpWakeConf{Enabled: &yes}).On() || (AssetOpWakeConf{Enabled: &no}).On() {
		t.Error("On():没写与 true 为开,显式 false 为关")
	}
	w := AssetOpWakeConf{Concurrency: 7, TimeoutMs: 15000}
	if w.EffectiveConcurrency() != 7 || w.EffectiveTimeout() != 15*time.Second {
		t.Errorf("显式值 = (%d, %v), want (7, 15s)", w.EffectiveConcurrency(), w.EffectiveTimeout())
	}
}

// TestAssetOpRecoveryBounds:通道开着时,落地恢复参数越界即拒启;同样的值在通道关着时不查(没有消费者)。
// 放弃不可逆,门槛与安全间隔配错一个量级(毫秒 / 秒 / 天)的后果最重,所以每一项都有一条反例。
func TestAssetOpRecoveryBounds(t *testing.T) {
	cases := []struct {
		name   string
		mutate func(a *AssetOpConf)
	}{
		{"停放次数为负", func(a *AssetOpConf) { a.OfflineAfterAttempts = -1 }},
		{"停放次数过大", func(a *AssetOpConf) { a.OfflineAfterAttempts = maxOfflineAfterAttempts + 1 }},
		{"停放间隔小于退避封顶", func(a *AssetOpConf) { a.OfflineBackoffMs = minOfflineBackoffMs - 1 }},
		{"停放间隔超过 24 小时", func(a *AssetOpConf) { a.OfflineBackoffMs = maxOfflineBackoffMs + 1 }},
		{"放弃门槛不足 30 天", func(a *AssetOpConf) { a.AbandonAfterDays = minAbandonAfterDays - 1 }},
		{"放弃门槛为负", func(a *AssetOpConf) { a.AbandonAfterDays = -1 }},
		{"放弃门槛过大", func(a *AssetOpConf) { a.AbandonAfterDays = maxAbandonAfterDays + 1 }},
		{"安全间隔不足 10 分钟", func(a *AssetOpConf) { a.AbandonSafetyGapMs = minAbandonSafetyGapMs - 1 }},
		{"单轮放弃 0 行", func(a *AssetOpConf) { a.AbandonBatch = 0 }},
		{"单轮放弃过多", func(a *AssetOpConf) { a.AbandonBatch = maxAbandonBatch + 1 }},
		{"放弃间隔过短", func(a *AssetOpConf) { a.AbandonIntervalMs = minAbandonIntervalMs - 1 }},
		{"对账间隔过短", func(a *AssetOpConf) { a.AuditIntervalMs = minAuditIntervalMs - 1 }},
		{"对账间隔过长", func(a *AssetOpConf) { a.AuditIntervalMs = maxAuditIntervalMs + 1 }},
		{"唤醒并发为负", func(a *AssetOpConf) { a.Wake.Concurrency = -1 }},
		{"唤醒并发过大", func(a *AssetOpConf) { a.Wake.Concurrency = maxWakeConcurrency + 1 }},
		{"唤醒超时不大于单行预算", func(a *AssetOpConf) { a.Wake.TimeoutMs = 2500 }},
		{"唤醒超时过长", func(a *AssetOpConf) { a.Wake.TimeoutMs = maxWakeTimeoutMs + 1 }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			c := validConfig()
			c.AssetOp = enabledAssetOp()
			tc.mutate(&c.AssetOp)
			if err := c.Validate(); err == nil {
				t.Fatal("通道开着时应被 Validate 拒绝")
			}

			c.AssetOp.Enabled = false
			if err := c.Validate(); err != nil {
				t.Errorf("通道关着时落地恢复参数不该被校验: %v", err)
			}
		})
	}
}

// TestAssetOpRecoveryAcceptsDisabledFeatures:各功能关着(0)时它的从属参数不校验,与 assetop 的口径一致;
// 唤醒显式关闭时并发 / 超时不校验。
func TestAssetOpRecoveryAcceptsDisabledFeatures(t *testing.T) {
	no := false
	cases := []struct {
		name   string
		mutate func(a *AssetOpConf)
	}{
		{"默认值", func(*AssetOpConf) {}},
		{"停放档关闭", func(a *AssetOpConf) { a.OfflineAfterAttempts, a.OfflineBackoffMs = 0, 0 }},
		{"自动放弃关闭", func(a *AssetOpConf) {
			a.AbandonAfterDays, a.AbandonSafetyGapMs, a.AbandonBatch, a.AbandonIntervalMs = 0, 0, 0, 0
		}},
		{"对账关闭", func(a *AssetOpConf) { a.AuditIntervalMs = 0 }},
		{"唤醒关闭时不查并发与超时", func(a *AssetOpConf) {
			a.Wake = AssetOpWakeConf{Enabled: &no, Concurrency: 999, TimeoutMs: 1}
		}},
		{"唤醒显式取边界值", func(a *AssetOpConf) {
			a.Wake = AssetOpWakeConf{Concurrency: maxWakeConcurrency, TimeoutMs: minWakeTimeoutMs}
		}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			c := validConfig()
			c.AssetOp = enabledAssetOp()
			tc.mutate(&c.AssetOp)
			if err := c.Validate(); err != nil {
				t.Fatalf("应通过校验: %v", err)
			}
		})
	}
}

// TestAssetOpCleanupBounds:清理与通道开关独立(同 guild),只看 CleanupEnabled。
func TestAssetOpCleanupBounds(t *testing.T) {
	cases := []struct {
		name   string
		mutate func(a *AssetOpConf)
	}{
		{"间隔为 0", func(a *AssetOpConf) { a.CleanupIntervalMinutes = 0 }},
		{"间隔过长", func(a *AssetOpConf) { a.CleanupIntervalMinutes = maxCleanupIntervalMinutes + 1 }},
		{"保留期不足 7 天", func(a *AssetOpConf) { a.TerminalRetentionDays = minRetentionDays - 1 }},
		{"保留期过长", func(a *AssetOpConf) { a.TerminalRetentionDays = maxRetentionDays + 1 }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			c := validConfig()
			c.AssetOp = AssetOpConf{CleanupEnabled: true, CleanupIntervalMinutes: 10, TerminalRetentionDays: 30}
			tc.mutate(&c.AssetOp)
			if err := c.Validate(); err == nil {
				t.Fatal("CleanupEnabled 时越界应被拒绝(通道关着也一样)")
			}
			c.AssetOp.CleanupEnabled = false
			if err := c.Validate(); err != nil {
				t.Errorf("CleanupEnabled=false 时清理参数不该被校验: %v", err)
			}
		})
	}
}
