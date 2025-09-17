package util

import (
	"pbgen/internal/config"
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

func CheckGrpcServiceExistence(protoPath string) bool {
	basePath := strings.ToLower(protoPath)

	// gRPC 支持的语言列表
	grpcLanguages := []string{"go", "python", "java", "csharp", "ruby", "node", "php", "objc", "swift"}

	// 检查是否包含任何支持的语言标识符
	for _, language := range grpcLanguages {
		if strings.Contains(basePath, "service/"+language+"/grpc") {
			return true
		}
	}
	return false
}

func CheckEtcdServiceExistence(protoPath string) bool {
	basePath := strings.ToLower(protoPath)
	return strings.Contains(basePath, "middleware/etcd/")
}
