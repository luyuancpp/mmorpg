package internal

import (
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// / game server
func IsSceneMethodHandler(methods *RPCMethods) bool {
	if len(*methods) == 0 {
		return false
	}
	first := (*methods)[0]
	if !strings.Contains(first.Path(), config.ProtoDirectoryNames[config.SceneProtoDirIndex]) {
		return false
	}
	return !util.ContainsPlayerKeyword(first.Service())
}

func IsGsPlayerHandler(methods *RPCMethods) bool {
	if len(*methods) <= 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.SceneProtoDirIndex]) {
		return false
	}

	return util.ContainsPlayerKeyword(firstMethodInfo.Service())
}

func ReturnNoHandler(methods *RPCMethods) bool {
	return false
}

func isGsPlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if util.IsPathInProtoDirs(firstMethodInfo.Path(), config.SceneProtoDirIndex) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	baseDirName := strings.ToLower(filepath.Base(firstMethodInfo.Path())) // 提取最后一级目录名作为 key

	if config.GrpcServices[baseDirName] {
		return false
	}
	return util.ContainsPlayerKeyword(firstMethodInfo.Service())
}

func isGsMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if util.IsPathInProtoDirs(firstMethodInfo.Path(), config.SceneProtoDirIndex) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	if util.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}
