package config

// 模式白名单(Config.validate 里的 switch)与 main.go 的分发是两份手写清单,只靠人对齐。
// 2026-10 team-backfill-smoke 与 features-smoke 都在 main.go 里有分发、却没进白名单:
// config.Load 阶段就以 unknown mode 退出,那两条分发与各自的子校验永远走不到,而且没有任何测试变红。
// 这里从 main.go 抽出全部 `cfg.Mode == "..."`,逐个确认白名单放行。

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func TestEveryDispatchedModePassesWhitelist(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "main.go"))
	if err != nil {
		t.Fatalf("read robot/main.go: %v", err)
	}
	matches := regexp.MustCompile(`cfg\.Mode == "([a-z0-9-]+)"`).FindAllStringSubmatch(string(src), -1)
	// 夹具前提:main.go 的分发写法变了(比如改成 switch)就抽不到,本用例会空转通过 —— 宁可在这里红。
	if len(matches) < 10 {
		t.Fatalf("extracted only %d dispatched modes from robot/main.go; the dispatch shape changed, update this test", len(matches))
	}
	seen := make(map[string]bool, len(matches))
	for _, m := range matches {
		mode := m[1]
		if seen[mode] {
			continue
		}
		seen[mode] = true
		// 只核对白名单这一关:模式自己的子校验(各 smoke 的 validate)缺字段时会报别的错,那不是本用例要查的。
		c := &Config{GatewayAddr: "http://127.0.0.1:8081", RobotCount: 1, AccountFmt: "robot_%04d", Mode: mode}
		if err := c.validate(); err != nil && strings.Contains(err.Error(), "unknown mode") {
			t.Errorf("mode %q is dispatched in robot/main.go but rejected by the whitelist in Config.validate: %v", mode, err)
		}
	}
}

func TestUnknownModeIsRejected(t *testing.T) {
	c := &Config{GatewayAddr: "http://127.0.0.1:8081", RobotCount: 1, AccountFmt: "robot_%04d", Mode: "no-such-mode"}
	err := c.validate()
	if err == nil || !strings.Contains(err.Error(), "unknown mode") {
		t.Fatalf("validate(no-such-mode) = %v, want an unknown mode error", err)
	}
}
