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
		if strings.Contains(strings.ToLower(protoPath), key) {
			return domain, true
		}
	}

	return "", false
}

// IsGRPC returns true if the domain uses gRPC protocol.
func IsGRPC(meta _config.DomainMeta) bool {
	return strings.ToLower(meta.Rpc.Type) == "grpc"
}

func HasGrpcService(protoPath string) bool {
	domain, ok := GetDomainByProtoPath(protoPath)
	if !ok {
		return false
	}

	meta := _config.Global.DomainMeta[domain]
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

// GetGRPCSubdirectories returns relative paths of subdirectories under service/go/grpc.
func GetGRPCSubdirectories() []string {
	var grpcDirs []string
	grpcBase := _config.Global.DirectoryNames.GoGrpcBaseDirName // 基础路径前缀

	for _, dir := range _config.Global.PathLists.ProtoDirectories {
		if strings.HasPrefix(dir, grpcBase) && dir != grpcBase {
			if !strings.HasSuffix(dir, "/") {
				dir += "/"
			}
			grpcDirs = append(grpcDirs, dir)
		}
	}

	return grpcDirs
}

// GetGRPCSubdirectoryNames returns just the directory names under service/go/grpc.
func GetGRPCSubdirectoryNames() []string {
	var names []string

	for domain, meta := range _config.Global.DomainMeta {

		if meta.Rpc.Type != "grpc" && meta.Rpc.Type != "both" {
			continue
		}

		if domain != "" {
			names = append(names, domain)
		}
	}

	return names
}
