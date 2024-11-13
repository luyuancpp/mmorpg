// Proto file for entity_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: entity_error_tip.proto

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

type EntityError int32

const (
	EntityError_kEntity_errorOK          EntityError = 0
	EntityError_kEntityTransformNotFound EntityError = 98
)

// Enum value maps for EntityError.
var (
	EntityError_name = map[int32]string{
		0:  "kEntity_errorOK",
		98: "kEntityTransformNotFound",
	}
	EntityError_value = map[string]int32{
		"kEntity_errorOK":          0,
		"kEntityTransformNotFound": 98,
	}
)

func (x EntityError) Enum() *EntityError {
	p := new(EntityError)
	*p = x
	return p
}

func (x EntityError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (EntityError) Descriptor() protoreflect.EnumDescriptor {
	return file_entity_error_tip_proto_enumTypes[0].Descriptor()
}

func (EntityError) Type() protoreflect.EnumType {
	return &file_entity_error_tip_proto_enumTypes[0]
}

func (x EntityError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use EntityError.Descriptor instead.
func (EntityError) EnumDescriptor() ([]byte, []int) {
	return file_entity_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_entity_error_tip_proto protoreflect.FileDescriptor

var file_entity_error_tip_proto_rawDesc = []byte{
	0x0a, 0x16, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74,
	0x69, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x41, 0x0a, 0x0c, 0x65, 0x6e, 0x74, 0x69,
	0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x13, 0x0a, 0x0f, 0x6b, 0x45, 0x6e, 0x74,
	0x69, 0x74, 0x79, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x1c, 0x0a,
	0x18, 0x6b, 0x45, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x54, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72,
	0x6d, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64, 0x10, 0x62, 0x42, 0x09, 0x5a, 0x07, 0x70,
	0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_entity_error_tip_proto_rawDescOnce sync.Once
	file_entity_error_tip_proto_rawDescData = file_entity_error_tip_proto_rawDesc
)

func file_entity_error_tip_proto_rawDescGZIP() []byte {
	file_entity_error_tip_proto_rawDescOnce.Do(func() {
		file_entity_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_entity_error_tip_proto_rawDescData)
	})
	return file_entity_error_tip_proto_rawDescData
}

var file_entity_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_entity_error_tip_proto_goTypes = []any{
	(EntityError)(0), // 0: entity_error
}
var file_entity_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_entity_error_tip_proto_init() }
func file_entity_error_tip_proto_init() {
	if File_entity_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_entity_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_entity_error_tip_proto_goTypes,
		DependencyIndexes: file_entity_error_tip_proto_depIdxs,
		EnumInfos:         file_entity_error_tip_proto_enumTypes,
	}.Build()
	File_entity_error_tip_proto = out.File
	file_entity_error_tip_proto_rawDesc = nil
	file_entity_error_tip_proto_goTypes = nil
	file_entity_error_tip_proto_depIdxs = nil
}
