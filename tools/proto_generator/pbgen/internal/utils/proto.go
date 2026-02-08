package utils

import (
	"path"
	_config "pbgen/internal/config"
	"strings"
)

func GetDomainByProtoPath(protoPath string) (string, bool) {
	lower := strings.ToLower(protoPath)

	for domain, meta := range _config.Global.DomainMeta {
		if meta.Source == "" {
			continue
		}

		key := "/" + strings.ToLower(meta.Source) + "/"
		if strings.Contains(lower, key) {
			return domain, true
		}
	}

	return "", false
}

func HasGrpcService(protoPath string) bool {
	domain, ok := GetDomainByProtoPath(protoPath)
	if !ok {
		return false
	}

	meta := _config.Global.DomainMeta[domain]
	return strings.ToLower(meta.Rpc) == "grpc"
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
	grpcBase := _config.Global.DirectoryNames.GoGrpcBaseDirName

	for _, dir := range GetGRPCSubdirectories() {
		// 去除基础路径前缀
		relative := strings.TrimPrefix(dir, grpcBase)
		// 去除末尾的/
		name := strings.TrimSuffix(relative, "/")
		if name != "" {
			names = append(names, name)
		}
	}

	return names
}
