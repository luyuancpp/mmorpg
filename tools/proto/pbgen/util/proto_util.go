package util

import (
	"path"
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

// GetGrpcLanguageFromPath 从给定路径中提取gRPC语言名称
// 如果路径中不包含任何支持的gRPC语言，则返回空字符串
func GetGrpcLanguageFromPath(protoPath string) string {
	basePath := strings.ToLower(protoPath)

	// 遍历所有支持的gRPC语言
	for _, language := range config.GrpcLanguages {
		lowerLang := strings.ToLower(language)
		// 检查路径中是否包含 "service/语言名/grpc" 模式
		if strings.Contains(basePath, "service/"+lowerLang+"/grpc") {
			return language // 返回原始语言名称（保持大小写）
		}
	}

	// 未找到匹配的语言
	return ""
}

// HasGrpcService 检查proto路径是否包含gRPC服务定义
func HasGrpcService(protoPath string) bool {
	// 利用提取语言的函数来判断是否存在gRPC服务
	return GetGrpcLanguageFromPath(protoPath) != ""
}

func CheckEtcdServiceExistence(protoPath string) bool {
	basePath := strings.ToLower(protoPath)
	return strings.Contains(basePath, "middleware/etcd/")
}

func BuildModelPath(protoPath string) string {
	basePath := strings.ToLower(path.Base(protoPath))
	language := GetGrpcLanguageFromPath(protoPath)
	return config.OutputRoot + "/" + language + "/" + basePath + "/" + config.ModelPath
}
