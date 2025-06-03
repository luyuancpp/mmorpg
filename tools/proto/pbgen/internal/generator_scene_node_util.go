package internal

import (
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// / game server
func IsGsMethodHandler(methods *RPCMethods) bool {
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

	if !strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	return true
}

func ReturnNoHandler(methods *RPCMethods) bool {
	return false
}

func isGsPlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if util.IsPathInOtherProtoDirs(firstMethodInfo.Path(), config.SceneProtoDirIndex) {
		return false
	}

	// Check if the file base name does not contain the GameNodePrefixName
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePlayerPrefixName)
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
