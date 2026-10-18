package taskresult

import (
	"strings"
	"testing"
)

// 旧频道字面值是与已部署的旧版 go/db / login 之间的线上契约:改了它,混跑期两端就互相收不到。
func TestLegacyNotifyChannelIsWireContract(t *testing.T) {
	if LegacyNotifyChannel != "task:result:notify" {
		t.Fatalf("LegacyNotifyChannel = %q, want %q (wire contract with deployed binaries)", LegacyNotifyChannel, "task:result:notify")
	}
}

func TestValidReplyInstance(t *testing.T) {
	cases := []struct {
		name string
		id   string
		want bool
	}{
		{"UUID", "0b6c2f5e-6a3d-4c1b-9f2e-8d7a6b5c4d3e", true},
		{"单字符", "a", true},
		{"大小写与数字", "Login-Zone1-AZ09", true},
		{"正好上限", strings.Repeat("a", MaxReplyInstanceLen), true},
		{"空串", "", false},
		{"超过上限", strings.Repeat("a", MaxReplyInstanceLen+1), false},
		{"冒号", "a:b", false},
		{"星号", "a*", false},
		{"问号", "a?", false},
		{"左方括号", "a[", false},
		{"右方括号", "a]", false},
		{"空格", "a b", false},
		{"换行", "a\n", false},
		{"下划线", "a_b", false},
		{"非 ASCII", "登录", false},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if got := ValidReplyInstance(c.id); got != c.want {
				t.Fatalf("ValidReplyInstance(%q) = %v, want %v", c.id, got, c.want)
			}
		})
	}
}

func TestInstanceNotifyChannel(t *testing.T) {
	const id = "0b6c2f5e-6a3d-4c1b-9f2e-8d7a6b5c4d3e"
	ch, ok := InstanceNotifyChannel(id)
	if !ok || ch != "task:result:notify:"+id {
		t.Fatalf("InstanceNotifyChannel(%q) = (%q, %v), want (%q, true)", id, ch, ok, "task:result:notify:"+id)
	}
	if ch == LegacyNotifyChannel {
		t.Fatal("instance channel must differ from the legacy channel")
	}

	for _, bad := range []string{"", "a:b", "*", strings.Repeat("a", MaxReplyInstanceLen+1)} {
		if ch, ok := InstanceNotifyChannel(bad); ok || ch != "" {
			t.Fatalf("InstanceNotifyChannel(%q) = (%q, %v), want (\"\", false)", bad, ch, ok)
		}
	}
}
