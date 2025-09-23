package internal

import (
	"pbgen/internal/config"
	"pbgen/utils"
	"strings"
)

// / game server
func IsRoomNodeMethodHandler(methods *RPCMethods) bool {
	if len(*methods) == 0 {
		return false
	}
	first := (*methods)[0]
	if !strings.Contains(first.Path(), config.ProtoDirectoryNames[config.RoomProtoDirIndex]) {
		return false
	}
	return !utils.ContainsPlayerKeyword(first.Service())
}

func IsRoomNodePlayerHandler(methods *RPCMethods) bool {
	if len(*methods) <= 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.RoomProtoDirIndex]) {
		return false
	}

	if utils.HasGrpcService(firstMethodInfo.Path()) {
		return false
	}

	return utils.ContainsPlayerKeyword(firstMethodInfo.Service())
}

func ReturnNoHandler(methods *RPCMethods) bool {
	return false
}

func isRoomNodePlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.RoomProtoDirIndex) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	if utils.HasGrpcService(strings.ToLower(firstMethodInfo.Path())) {
		return false
	}
	return utils.ContainsPlayerKeyword(firstMethodInfo.Service())
}

func isRoomNodeMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if utils.IsPathInProtoDirs(firstMethodInfo.Path(), config.RoomProtoDirIndex) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	if utils.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}
