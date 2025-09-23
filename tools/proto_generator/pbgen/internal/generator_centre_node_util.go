package internal

import (
	"pbgen/internal/config"
	"pbgen/utils"
	"strings"
)

func isCentreMethodHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CenterProtoDirIndex]) {
		return false
	}

	if utils.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}

func isCentrePlayerHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.DbProtoDirIndex) ||
		utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.LoginProtoDirIndex) {
		return false
	}

	if !utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.CenterProtoDirIndex) {
		return false
	}

	return utils.ContainsPlayerKeyword(firstMethodInfo.Service())
}

func isCentreMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.CenterProtoDirIndex) {
		return false
	}

	if utils.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}

func isCentrePlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.CenterProtoDirIndex) {
		return false
	}

	if !utils.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	return true
}
