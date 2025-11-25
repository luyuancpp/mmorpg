package utils

import (
	"path/filepath"
	"strings"
)

func GetLastTwoDirs(path string) string {
	// 先清理路径（处理相对路径、重复分隔符等）
	cleanPath := filepath.Clean(path)

	// 分割为组件
	parts := strings.Split(cleanPath, string(filepath.Separator))
	length := len(parts)

	// 处理边界情况
	if length <= 1 {
		return cleanPath
	} else if length == 2 {
		return cleanPath
	}

	// 取最后两个组件
	return filepath.Join(parts[length-2], parts[length-1])
}
