package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"reflect"
	"strings"
)

func boolFromExtensionValue(extValue interface{}) (bool, bool) {
	if extValue == nil {
		return false, false
	}
	v := reflect.ValueOf(extValue)
	if v.Kind() == reflect.Pointer {
		if v.IsNil() {
			return false, false
		}
		v = v.Elem()
	}
	if v.Kind() != reflect.Bool {
		return false, false
	}
	return v.Bool(), true
}

func nodeTypeFromExtensionValue(extValue interface{}) (messageoption.NodeType, bool) {
	if extValue == nil {
		return messageoption.NodeType_NODE_UNSPECIFIED, false
	}
	v := reflect.ValueOf(extValue)
	if v.Kind() == reflect.Pointer {
		if v.IsNil() {
			return messageoption.NodeType_NODE_UNSPECIFIED, false
		}
		v = v.Elem()
	}

	switch v.Kind() {
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return messageoption.NodeType(v.Int()), true
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		return messageoption.NodeType(v.Uint()), true
	default:
		return messageoption.NodeType_NODE_UNSPECIFIED, false
	}
}

func inferNodeByProtoPath(fileDesc *descriptorpb.FileDescriptorProto) messageoption.NodeType {
	if fileDesc == nil {
		return messageoption.NodeType_NODE_UNSPECIFIED
	}

	path := strings.ToLower(fileDesc.GetName())
	switch {
	case strings.Contains(path, "/scene/"):
		return messageoption.NodeType_NODE_SCENE
	case strings.Contains(path, "/centre/"):
		return messageoption.NodeType_NODE_CENTRE
	case strings.Contains(path, "/gate/"):
		return messageoption.NodeType_NODE_GATE
	case strings.Contains(path, "/login/"):
		return messageoption.NodeType_NODE_LOGIN
	case strings.Contains(path, "/db/"):
		return messageoption.NodeType_NODE_DB
	default:
		return messageoption.NodeType_NODE_UNSPECIFIED
	}
}

// IsClientProtocolService returns true if the service has OptionIsClientProtocolService set.
func IsClientProtocolService(serviceDesc *descriptorpb.ServiceDescriptorProto) bool {
	opts := serviceDesc.GetOptions()
	if opts == nil {
		return false
	}

	extValue := proto.GetExtension(opts, messageoption.E_OptionIsClientProtocolService)
	v, ok := boolFromExtensionValue(extValue)
	return ok && v
}

func IsPlayerService(serviceDesc *descriptorpb.ServiceDescriptorProto) bool {
	opts := serviceDesc.GetOptions()
	if opts == nil {
		return false
	}

	extValue := proto.GetExtension(opts, messageoption.E_OptionIsPlayerService)
	v, ok := boolFromExtensionValue(extValue)
	return ok && v
}
// GetFileDefaultNode returns the file-level default node type, falling back to path inference.
func GetFileDefaultNode(fileDesc *descriptorpb.FileDescriptorProto) messageoption.NodeType {
	opts := fileDesc.GetOptions()
	if opts == nil {
		return messageoption.NodeType_NODE_UNSPECIFIED // 默认未指定
	}
	extValue := proto.GetExtension(opts, messageoption.E_OptionFileDefaultNode)
	if node, ok := nodeTypeFromExtensionValue(extValue); ok {
		return node
	}

	return inferNodeByProtoPath(fileDesc)
}

// IsFileBelongToNode returns true if the file's default node matches the target.
func IsFileBelongToNode(fileDesc *descriptorpb.FileDescriptorProto, targetNode messageoption.NodeType) bool {
	defaultNode := GetFileDefaultNode(fileDesc)
	return defaultNode == targetNode
}
