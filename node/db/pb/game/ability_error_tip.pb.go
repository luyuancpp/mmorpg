// Proto file for ability_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: ability_error_tip.proto

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

type AbilityError int32

const (
	AbilityError_kAbility_errorOK        AbilityError = 0
	AbilityError_kAbilityUnInterruptible AbilityError = 88
	AbilityError_kAbilityInvalidTargetId AbilityError = 89
	AbilityError_kAbilityInvalidTarget   AbilityError = 90
)

// Enum value maps for AbilityError.
var (
	AbilityError_name = map[int32]string{
		0:  "kAbility_errorOK",
		88: "kAbilityUnInterruptible",
		89: "kAbilityInvalidTargetId",
		90: "kAbilityInvalidTarget",
	}
	AbilityError_value = map[string]int32{
		"kAbility_errorOK":        0,
		"kAbilityUnInterruptible": 88,
		"kAbilityInvalidTargetId": 89,
		"kAbilityInvalidTarget":   90,
	}
)

func (x AbilityError) Enum() *AbilityError {
	p := new(AbilityError)
	*p = x
	return p
}

func (x AbilityError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (AbilityError) Descriptor() protoreflect.EnumDescriptor {
	return file_ability_error_tip_proto_enumTypes[0].Descriptor()
}

func (AbilityError) Type() protoreflect.EnumType {
	return &file_ability_error_tip_proto_enumTypes[0]
}

func (x AbilityError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use AbilityError.Descriptor instead.
func (AbilityError) EnumDescriptor() ([]byte, []int) {
	return file_ability_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_ability_error_tip_proto protoreflect.FileDescriptor

var file_ability_error_tip_proto_rawDesc = []byte{
	0x0a, 0x17, 0x61, 0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f,
	0x74, 0x69, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x7a, 0x0a, 0x0d, 0x61, 0x62, 0x69,
	0x6c, 0x69, 0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x41,
	0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00,
	0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x41, 0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x55, 0x6e, 0x49, 0x6e,
	0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x10, 0x58, 0x12, 0x1b, 0x0a,
	0x17, 0x6b, 0x41, 0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64,
	0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x49, 0x64, 0x10, 0x59, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x41,
	0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x54, 0x61, 0x72,
	0x67, 0x65, 0x74, 0x10, 0x5a, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_ability_error_tip_proto_rawDescOnce sync.Once
	file_ability_error_tip_proto_rawDescData = file_ability_error_tip_proto_rawDesc
)

func file_ability_error_tip_proto_rawDescGZIP() []byte {
	file_ability_error_tip_proto_rawDescOnce.Do(func() {
		file_ability_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_ability_error_tip_proto_rawDescData)
	})
	return file_ability_error_tip_proto_rawDescData
}

var file_ability_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_ability_error_tip_proto_goTypes = []any{
	(AbilityError)(0), // 0: ability_error
}
var file_ability_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_ability_error_tip_proto_init() }
func file_ability_error_tip_proto_init() {
	if File_ability_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_ability_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_ability_error_tip_proto_goTypes,
		DependencyIndexes: file_ability_error_tip_proto_depIdxs,
		EnumInfos:         file_ability_error_tip_proto_enumTypes,
	}.Build()
	File_ability_error_tip_proto = out.File
	file_ability_error_tip_proto_rawDesc = nil
	file_ability_error_tip_proto_goTypes = nil
	file_ability_error_tip_proto_depIdxs = nil
}
