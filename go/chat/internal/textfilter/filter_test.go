package textfilter

import (
	"strings"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestMaskSourceSemantics(t *testing.T) {
	for _, tc := range []struct {
		name    string
		words   []string
		content string
		want    string
	}{
		{"空表保留原文", nil, "你好World", "你好World"},
		{"中文按字符而非UTF8字节屏蔽", []string{"广告"}, "这是广告和广告", "这是**和**"},
		{"补充平面字符只变一星", []string{"😀"}, "A😀B", "A*B"},
		{"大小写敏感", []string{"Spam"}, "Spam spam SPAM", "**** spam SPAM"},
		{"字面匹配不是正则", []string{"a.b"}, "a.b axb", "*** axb"},
		{"跳过空词", []string{"", "广告", ""}, "广告", "**"},
		{"保留词表顺序", []string{"ab", "abc"}, "abc", "**c"},
		{"反向顺序先屏蔽长词", []string{"abc", "ab"}, "abc", "***"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			filter, err := New(tc.words)
			require.NoError(t, err)
			got := filter.Mask(tc.content)
			require.Equal(t, tc.want, got)
			require.LessOrEqual(t, len(got), len(tc.content), "过滤不能放大存储或带宽字节数")
		})
	}
}

func TestDictionarySnapshotDoesNotFollowCallerChanges(t *testing.T) {
	words := []string{"广告"}
	filter, err := New(words)
	require.NoError(t, err)
	words[0] = "正常"
	require.Equal(t, "正常**", filter.Mask("正常广告"))
}

func TestDictionaryLimits(t *testing.T) {
	_, err := New(make([]string, MaxWords))
	require.NoError(t, err)
	_, err = New(make([]string, MaxWords+1))
	require.Error(t, err)
	_, err = New([]string{strings.Repeat("x", MaxWordBytes)})
	require.NoError(t, err)
	_, err = New([]string{strings.Repeat("x", MaxWordBytes+1)})
	require.Error(t, err)
	words := make([]string, MaxTotalBytes/MaxWordBytes)
	for i := range words {
		words[i] = strings.Repeat("x", MaxWordBytes)
	}
	_, err = New(words)
	require.NoError(t, err)
	_, err = New(append(words, "x"))
	require.Error(t, err)
	_, err = New([]string{string([]byte{0xff})})
	require.Error(t, err)
}
