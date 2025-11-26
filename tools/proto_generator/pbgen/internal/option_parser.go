package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

// 工具函数：判断服务是否设置了 OptionIsClientProtocolService 为 true
func IsClientProtocolService(serviceDesc *descriptorpb.ServiceDescriptorProto) bool {
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

func IsPlayerService(serviceDesc *descriptorpb.ServiceDescriptorProto) bool {
	opts := serviceDesc.GetOptions()
	if opts == nil {
		return false
	}

	// 读取 OptionIsClientProtocolService 扩展选项
	extValue := proto.GetExtension(opts, messageoption.E_OptionIsPlayerService)

	// 转换为 bool 类型并判断是否为 true
	isClientProtocolSvc, ok := extValue.(bool)
	return ok && isClientProtocolSvc
}

// --------------------------
// 解析文件级默认节点类型 OptionFileDefaultNode
// --------------------------
func GetFileDefaultNode(fileDesc *descriptorpb.FileDescriptorProto) messageoption.NodeType {
	opts := fileDesc.GetOptions()
	if opts == nil {
		return messageoption.NodeType_NODE_UNSPECIFIED // 默认未指定
	}

	// 读取文件级 OptionFileDefaultNode
	extValue := proto.GetExtension(opts, messageoption.E_OptionFileDefaultNode)

	// 转换为 NodeType 枚举类型
	nodeType, ok := extValue.(messageoption.NodeType)
	if !ok {
		return messageoption.NodeType_NODE_UNSPECIFIED
	}

	return nodeType
}

// 判断文件是否归属指定节点类型
func IsFileBelongToNode(fileDesc *descriptorpb.FileDescriptorProto, targetNode messageoption.NodeType) bool {
	defaultNode := GetFileDefaultNode(fileDesc)
	return defaultNode == targetNode
}
