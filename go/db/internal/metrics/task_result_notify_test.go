package metrics

import (
	"os"
	"path/filepath"
	"regexp"
	"testing"
)

// TestTaskResultNotifySeriesPrecreated 钉住 db_task_result_notify_total 的 3×3 条预建序列与取值字面量:
// deploy/k8s/task-result-notify-alerts.yaml 用 increase() / rate() 按 target="instance"、result="no_receiver" 等字面值写,
// 序列从无到 1 时会漏掉第一次,常量一改名告警会静默落空 —— 这条用例让两种情况都先在这里红。
// 本包没有用例调 CountTaskResultNotify,所以 9 条都必须仍是 0。
func TestTaskResultNotifySeriesPrecreated(t *testing.T) {
	registerTaskResultNotify()
	assertExactSeries(t, "db_task_result_notify_total", []string{"target", "result"}, []string{
		"instance|ok", "instance|no_receiver", "instance|error",
		"legacy_unset|ok", "legacy_unset|no_receiver", "legacy_unset|error",
		"legacy_invalid|ok", "legacy_invalid|no_receiver", "legacy_invalid|error",
	}, true)
	for _, pin := range [][2]string{
		{TaskResultNotifyInstance, "instance"}, {TaskResultNotifyLegacyUnset, "legacy_unset"},
		{TaskResultNotifyLegacyInvalid, "legacy_invalid"},
		{TaskResultNotifyOK, "ok"}, {TaskResultNotifyNoReceiver, "no_receiver"}, {TaskResultNotifyError, "error"},
	} {
		if pin[0] != pin[1] {
			t.Fatalf("alert pinned label value changed: got %q, want %q", pin[0], pin[1])
		}
	}
}

// TestTaskResultNotifyNamesMatchStressSummarize 读 stress_summarize.ps1 2g 段的 $notifyTargets / $notifyResults,
// 与本包取值逐项比对:两边不一致时脚本整列静默变成 n/a,只有这里能先红。
func TestTaskResultNotifyNamesMatchStressSummarize(t *testing.T) {
	path := filepath.Join("..", "..", "..", "..", "tools", "scripts", "stress_summarize.ps1")
	raw, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	for _, tc := range []struct {
		variable string
		want     []string
	}{
		{"notifyTargets", taskResultNotifyTargets},
		{"notifyResults", taskResultNotifyResults},
	} {
		block := regexp.MustCompile(`(?s)\$` + tc.variable + `\s*=\s*@\(([^)]*)\)`).FindSubmatch(raw)
		if block == nil {
			t.Fatalf("$%s list not found in %s", tc.variable, path)
		}
		var got []string
		for _, m := range regexp.MustCompile(`'([a-z_]+)'`).FindAllSubmatch(block[1], -1) {
			got = append(got, string(m[1]))
		}
		if len(got) != len(tc.want) {
			t.Fatalf("stress_summarize.ps1 $%s = %v, want %v", tc.variable, got, tc.want)
		}
		for i := range got {
			if got[i] != tc.want[i] {
				t.Fatalf("stress_summarize.ps1 $%s[%d] = %q, want %q", tc.variable, i, got[i], tc.want[i])
			}
		}
	}
}
