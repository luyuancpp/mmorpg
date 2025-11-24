package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/config"
	"pbgen/utils"
	"strings"
)

// 工具函数：判断服务是否设置了 OptionIsClientProtocolService 为 true
func isClientProtocolService(serviceDesc *descriptorpb.ServiceDescriptorProto) bool {
	opts := serviceDesc.GetOptions()
	if opts == nil {
		return false
	}

	// 读取 OptionIsClientProtocolService 扩展选项
	extValue := proto.GetExtension(opts, messageoption.E_OptionIsClientProtocolService)

	// 转换为 bool 类型并判断是否为 true
	isClientProtocolSvc, ok := extValue.(bool)
	return ok && isClientProtocolSvc
}

// / game server
func IsRoomNodeMethodHandler(methods *RPCMethods) bool {
	if len(*methods) == 0 {
		return false
	}
	firstMethodInfo := (*methods)[0]
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.RoomProtoDirIndex]) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
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

	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
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

	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
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

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}
