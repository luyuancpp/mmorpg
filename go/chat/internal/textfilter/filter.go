// Package textfilter 保存聊天敏感词的不可变启动快照，不读取外部系统。
package textfilter

import (
	"fmt"
	"strings"
	"unicode/utf8"
)

const (
	MaxWords      = 1024
	MaxWordBytes  = 256
	MaxTotalBytes = 64 * 1024
)

type replacement struct {
	word string
	mask string
}

// Filter 按配置顺序进行字面子串屏蔽。与原 Go 聊天业务一致，区分大小写，
// 一个 Unicode 字符替换为一个 *；不作正则、大小写折叠或 Unicode 归一化。
type Filter struct {
	replacements []replacement
}

// New 在启动时约束词表的扫描成本与内存，超限直接拒绝配置。空词按原源语义跳过。
func New(words []string) (*Filter, error) {
	if len(words) > MaxWords {
		return nil, fmt.Errorf("SensitiveWords 最多 %d 条", MaxWords)
	}
	f := &Filter{replacements: make([]replacement, 0, len(words))}
	totalBytes := 0
	for i, word := range words {
		if !utf8.ValidString(word) || len(word) > MaxWordBytes {
			return nil, fmt.Errorf("SensitiveWords[%d] 必须是有效 UTF-8 且最多 %d 字节", i, MaxWordBytes)
		}
		totalBytes += len(word)
		if totalBytes > MaxTotalBytes {
			return nil, fmt.Errorf("SensitiveWords 总计最多 %d 字节", MaxTotalBytes)
		}
		if word != "" {
			f.replacements = append(f.replacements, replacement{word, strings.Repeat("*", utf8.RuneCountInString(word))})
		}
	}
	return f, nil
}

// Mask 返回屏蔽后的正文。空快照保持原文，既有未配置词表的部署行为不变。
func (f *Filter) Mask(content string) string {
	if f == nil {
		return content
	}
	for _, r := range f.replacements {
		content = strings.ReplaceAll(content, r.word, r.mask)
	}
	return content
}
