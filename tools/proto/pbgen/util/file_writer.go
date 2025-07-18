package util

import (
	"os"
	"path/filepath"
	"pbgen/config"
	"strings"
)

func IsProtoFile(fd os.DirEntry) bool {
	if fd.IsDir() || filepath.Ext(fd.Name()) != ".proto" {
		return false
	}
	return true
}

func IncludeName(path string, protoName string) string {
	pbcHeadName := strings.Replace(protoName, config.ProtoEx, config.ProtoPbhEx, 1)
	return config.IncludeBegin + strings.Replace(path, config.ProtoDir, "", 1) + pbcHeadName + "\"\n"
}
