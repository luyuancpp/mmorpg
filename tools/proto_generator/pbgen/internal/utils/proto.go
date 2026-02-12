package utils

import (
	"path"
	_config "pbgen/internal/config"
	"strings"
)

func GetDomainByProtoPath(protoPath string) (string, bool) {
	for domain, meta := range _config.Global.DomainMeta {
		if meta.Source == "" {
			continue
		}

		key := "/" + strings.ToLower(meta.Source) + "/"
		if strings.Contains(key, protoPath) {
			return domain, true
		}
	}

	return "", false
}

// IsGRPC 判断传入的元数据是否使用 gRPC 协议
// 入参：meta - 业务域的元数据对象（类型与 _config.Global.DomainMeta[domain] 一致）
// 出参：bool - true 表示是 gRPC，false 表示不是
func IsGRPC(meta _config.DomainMeta) bool { // 注意：DomainMeta 需要替换成你实际的元数据类型
	return strings.ToLower(meta.Rpc) == "grpc"
}

// 原函数改造后
func HasGrpcService(protoPath string) bool {
	domain, ok := GetDomainByProtoPath(protoPath)
	if !ok {
		return false
	}

	meta := _config.Global.DomainMeta[domain]
	// 直接调用抽离后的函数，逻辑更清晰
	return IsGRPC(meta)
}

func HasEtcdService(protoPath string) bool {
	domain, ok := GetDomainByProtoPath(protoPath)
	if !ok {
		return false
	}

	return domain == "etcd"
}

func BuildModelGoPath(protoPath string) string {
	basePath := strings.ToLower(path.Base(protoPath))
	language := "go" // 写死为 go
	return _config.Global.Paths.OutputRoot + "/" + language + "/" + basePath + "/" + _config.Global.Naming.Model
}

// GetGRPCSubdirectories 从ProtoDirectoryNames中筛选出service/go/grpc下的子目录
// 返回相对路径列表（如：["service/go/grpc/player_locator/", "service/go/grpc/login/"]）
func GetGRPCSubdirectories() []string {
	var grpcDirs []string
	grpcBase := _config.Global.DirectoryNames.GoGrpcBaseDirName // 基础路径前缀

	for _, dir := range _config.Global.PathLists.ProtoDirectories {
		// 检查目录是否以service/go/grpc/开头，且不是基础路径本身
		if strings.HasPrefix(dir, grpcBase) && dir != grpcBase {
			// 确保路径格式统一（以/结尾）
			if !strings.HasSuffix(dir, "/") {
				dir += "/"
			}
			grpcDirs = append(grpcDirs, dir)
		}
	}

	return grpcDirs
}

// GetGRPCSubdirectoryNames 只返回service/go/grpc下的子目录名称（不含完整路径）
// 返回示例：["player_locator", "login", "db", "chat", "team", "mail"]
func GetGRPCSubdirectoryNames() []string {
	var names []string

	for domain, meta := range _config.Global.DomainMeta {

		// 是否支持 grpc
		if meta.Rpc != "grpc" && meta.Rpc != "both" {
			continue
		}

		if domain != "" {
			names = append(names, domain)
		}
	}

	return names
}
