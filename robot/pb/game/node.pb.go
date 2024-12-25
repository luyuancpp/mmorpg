// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: proto/logic/constants/node.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type ENodeType int32

const (
	ENodeType_kCentreNode   ENodeType = 0 //场景切换中心服务器
	ENodeType_kGameNode     ENodeType = 1 //逻辑服务器
	ENodeType_kLoginNode    ENodeType = 2 //登录服务器
	ENodeType_kGateNode     ENodeType = 3 //网关服务器
	ENodeType_kDatabaseNode ENodeType = 4 //数据库服务器
)

// Enum value maps for ENodeType.
var (
	ENodeType_name = map[int32]string{
		0: "kCentreNode",
		1: "kGameNode",
		2: "kLoginNode",
		3: "kGateNode",
		4: "kDatabaseNode",
	}
	ENodeType_value = map[string]int32{
		"kCentreNode":   0,
		"kGameNode":     1,
		"kLoginNode":    2,
		"kGateNode":     3,
		"kDatabaseNode": 4,
	}
)

func (x ENodeType) Enum() *ENodeType {
	p := new(ENodeType)
	*p = x
	return p
}

func (x ENodeType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (ENodeType) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_logic_constants_node_proto_enumTypes[0].Descriptor()
}

func (ENodeType) Type() protoreflect.EnumType {
	return &file_proto_logic_constants_node_proto_enumTypes[0]
}

func (x ENodeType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use ENodeType.Descriptor instead.
func (ENodeType) EnumDescriptor() ([]byte, []int) {
	return file_proto_logic_constants_node_proto_rawDescGZIP(), []int{0}
}

type EGameNodeType int32

const (
	EGameNodeType_kMainSceneNode      EGameNodeType = 0 //主世界场景
	EGameNodeType_kRoomNode           EGameNodeType = 1 //副本场景
	EGameNodeType_kMainSceneCrossNode EGameNodeType = 2 //跨服主世界场景
	EGameNodeType_kRoomSceneCrossNode EGameNodeType = 3 //跨服副本场景
)

// Enum value maps for EGameNodeType.
var (
	EGameNodeType_name = map[int32]string{
		0: "kMainSceneNode",
		1: "kRoomNode",
		2: "kMainSceneCrossNode",
		3: "kRoomSceneCrossNode",
	}
	EGameNodeType_value = map[string]int32{
		"kMainSceneNode":      0,
		"kRoomNode":           1,
		"kMainSceneCrossNode": 2,
		"kRoomSceneCrossNode": 3,
	}
)

func (x EGameNodeType) Enum() *EGameNodeType {
	p := new(EGameNodeType)
	*p = x
	return p
}

func (x EGameNodeType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (EGameNodeType) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_logic_constants_node_proto_enumTypes[1].Descriptor()
}

func (EGameNodeType) Type() protoreflect.EnumType {
	return &file_proto_logic_constants_node_proto_enumTypes[1]
}

func (x EGameNodeType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use EGameNodeType.Descriptor instead.
func (EGameNodeType) EnumDescriptor() ([]byte, []int) {
	return file_proto_logic_constants_node_proto_rawDescGZIP(), []int{1}
}

var File_proto_logic_constants_node_proto protoreflect.FileDescriptor

var file_proto_logic_constants_node_proto_rawDesc = []byte{
	0x0a, 0x20, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6e, 0x73, 0x74, 0x61, 0x6e, 0x74, 0x73, 0x2f, 0x6e, 0x6f, 0x64, 0x65, 0x2e, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x2a, 0x5d, 0x0a, 0x09, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x54, 0x79, 0x70, 0x65, 0x12,
	0x0f, 0x0a, 0x0b, 0x6b, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x00,
	0x12, 0x0d, 0x0a, 0x09, 0x6b, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x01, 0x12,
	0x0e, 0x0a, 0x0a, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x02, 0x12,
	0x0d, 0x0a, 0x09, 0x6b, 0x47, 0x61, 0x74, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x03, 0x12, 0x11,
	0x0a, 0x0d, 0x6b, 0x44, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x10,
	0x04, 0x2a, 0x64, 0x0a, 0x0d, 0x65, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x54, 0x79,
	0x70, 0x65, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x4d, 0x61, 0x69, 0x6e, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x4e, 0x6f, 0x64, 0x65, 0x10, 0x00, 0x12, 0x0d, 0x0a, 0x09, 0x6b, 0x52, 0x6f, 0x6f, 0x6d, 0x4e,
	0x6f, 0x64, 0x65, 0x10, 0x01, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x4d, 0x61, 0x69, 0x6e, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x43, 0x72, 0x6f, 0x73, 0x73, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x02, 0x12, 0x17,
	0x0a, 0x13, 0x6b, 0x52, 0x6f, 0x6f, 0x6d, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x43, 0x72, 0x6f, 0x73,
	0x73, 0x4e, 0x6f, 0x64, 0x65, 0x10, 0x03, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61,
	0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_constants_node_proto_rawDescOnce sync.Once
	file_proto_logic_constants_node_proto_rawDescData = file_proto_logic_constants_node_proto_rawDesc
)

func file_proto_logic_constants_node_proto_rawDescGZIP() []byte {
	file_proto_logic_constants_node_proto_rawDescOnce.Do(func() {
		file_proto_logic_constants_node_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_constants_node_proto_rawDescData)
	})
	return file_proto_logic_constants_node_proto_rawDescData
}

var file_proto_logic_constants_node_proto_enumTypes = make([]protoimpl.EnumInfo, 2)
var file_proto_logic_constants_node_proto_goTypes = []any{
	(ENodeType)(0),     // 0: eNodeType
	(EGameNodeType)(0), // 1: eGameNodeType
}
var file_proto_logic_constants_node_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_logic_constants_node_proto_init() }
func file_proto_logic_constants_node_proto_init() {
	if File_proto_logic_constants_node_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_proto_logic_constants_node_proto_rawDesc,
			NumEnums:      2,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_constants_node_proto_goTypes,
		DependencyIndexes: file_proto_logic_constants_node_proto_depIdxs,
		EnumInfos:         file_proto_logic_constants_node_proto_enumTypes,
	}.Build()
	File_proto_logic_constants_node_proto = out.File
	file_proto_logic_constants_node_proto_rawDesc = nil
	file_proto_logic_constants_node_proto_goTypes = nil
	file_proto_logic_constants_node_proto_depIdxs = nil
}
