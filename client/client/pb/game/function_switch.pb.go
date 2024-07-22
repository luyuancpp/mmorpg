// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/constants/function_switch.proto

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

type EFunctionSwitchBitType int32

const (
	EFunctionSwitchBitType_kFunctionSwitchTeam    EFunctionSwitchBitType = 0 //
	EFunctionSwitchBitType_kFunctionSwitchFriends EFunctionSwitchBitType = 1 //
)

// Enum value maps for EFunctionSwitchBitType.
var (
	EFunctionSwitchBitType_name = map[int32]string{
		0: "kFunctionSwitchTeam",
		1: "kFunctionSwitchFriends",
	}
	EFunctionSwitchBitType_value = map[string]int32{
		"kFunctionSwitchTeam":    0,
		"kFunctionSwitchFriends": 1,
	}
)

func (x EFunctionSwitchBitType) Enum() *EFunctionSwitchBitType {
	p := new(EFunctionSwitchBitType)
	*p = x
	return p
}

func (x EFunctionSwitchBitType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (EFunctionSwitchBitType) Descriptor() protoreflect.EnumDescriptor {
	return file_logic_constants_function_switch_proto_enumTypes[0].Descriptor()
}

func (EFunctionSwitchBitType) Type() protoreflect.EnumType {
	return &file_logic_constants_function_switch_proto_enumTypes[0]
}

func (x EFunctionSwitchBitType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use EFunctionSwitchBitType.Descriptor instead.
func (EFunctionSwitchBitType) EnumDescriptor() ([]byte, []int) {
	return file_logic_constants_function_switch_proto_rawDescGZIP(), []int{0}
}

var File_logic_constants_function_switch_proto protoreflect.FileDescriptor

var file_logic_constants_function_switch_proto_rawDesc = []byte{
	0x0a, 0x25, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6e, 0x73, 0x74, 0x61, 0x6e, 0x74,
	0x73, 0x2f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x5f, 0x73, 0x77, 0x69, 0x74, 0x63,
	0x68, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x4d, 0x0a, 0x16, 0x65, 0x46, 0x75, 0x6e, 0x63,
	0x74, 0x69, 0x6f, 0x6e, 0x53, 0x77, 0x69, 0x74, 0x63, 0x68, 0x42, 0x69, 0x74, 0x54, 0x79, 0x70,
	0x65, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x46, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x77,
	0x69, 0x74, 0x63, 0x68, 0x54, 0x65, 0x61, 0x6d, 0x10, 0x00, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x46,
	0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x77, 0x69, 0x74, 0x63, 0x68, 0x46, 0x72, 0x69,
	0x65, 0x6e, 0x64, 0x73, 0x10, 0x01, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d,
	0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_constants_function_switch_proto_rawDescOnce sync.Once
	file_logic_constants_function_switch_proto_rawDescData = file_logic_constants_function_switch_proto_rawDesc
)

func file_logic_constants_function_switch_proto_rawDescGZIP() []byte {
	file_logic_constants_function_switch_proto_rawDescOnce.Do(func() {
		file_logic_constants_function_switch_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_constants_function_switch_proto_rawDescData)
	})
	return file_logic_constants_function_switch_proto_rawDescData
}

var file_logic_constants_function_switch_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_logic_constants_function_switch_proto_goTypes = []any{
	(EFunctionSwitchBitType)(0), // 0: eFunctionSwitchBitType
}
var file_logic_constants_function_switch_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_logic_constants_function_switch_proto_init() }
func file_logic_constants_function_switch_proto_init() {
	if File_logic_constants_function_switch_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_logic_constants_function_switch_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_logic_constants_function_switch_proto_goTypes,
		DependencyIndexes: file_logic_constants_function_switch_proto_depIdxs,
		EnumInfos:         file_logic_constants_function_switch_proto_enumTypes,
	}.Build()
	File_logic_constants_function_switch_proto = out.File
	file_logic_constants_function_switch_proto_rawDesc = nil
	file_logic_constants_function_switch_proto_goTypes = nil
	file_logic_constants_function_switch_proto_depIdxs = nil
}
