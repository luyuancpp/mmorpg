package util

import (
	"pbgen/config"
	"strings"
)

func IsPathInOtherProtoDirs(path string, selfIndex int) bool {
	for i, dirName := range config.ProtoDirectoryNames {
		if i == selfIndex {
			// 跳过自己目录
			continue
		}
		if strings.Contains(path, dirName) {
			return true
		}
	}
	return false
}

func IsPathInProtoDirs(path string, selfIndex int) bool {
	return strings.Contains(config.ProtoDirectoryNames[selfIndex], path) || strings.Contains(path, config.ProtoDirectoryNames[selfIndex])
}
