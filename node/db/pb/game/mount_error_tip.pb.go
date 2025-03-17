// Proto file for mount_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v6.31.0--dev
// source: mount_error_tip.proto

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

type MountError int32

const (
	MountError_kMount_errorOK   MountError = 0
	MountError_kMountNotMounted MountError = 104
)

// Enum value maps for MountError.
var (
	MountError_name = map[int32]string{
		0:   "kMount_errorOK",
		104: "kMountNotMounted",
	}
	MountError_value = map[string]int32{
		"kMount_errorOK":   0,
		"kMountNotMounted": 104,
	}
)

func (x MountError) Enum() *MountError {
	p := new(MountError)
	*p = x
	return p
}

func (x MountError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (MountError) Descriptor() protoreflect.EnumDescriptor {
	return file_mount_error_tip_proto_enumTypes[0].Descriptor()
}

func (MountError) Type() protoreflect.EnumType {
	return &file_mount_error_tip_proto_enumTypes[0]
}

func (x MountError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use MountError.Descriptor instead.
func (MountError) EnumDescriptor() ([]byte, []int) {
	return file_mount_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_mount_error_tip_proto protoreflect.FileDescriptor

var file_mount_error_tip_proto_rawDesc = []byte{
	0x0a, 0x15, 0x6d, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x37, 0x0a, 0x0b, 0x6d, 0x6f, 0x75, 0x6e, 0x74,
	0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x4d, 0x6f, 0x75, 0x6e, 0x74,
	0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x4d,
	0x6f, 0x75, 0x6e, 0x74, 0x4e, 0x6f, 0x74, 0x4d, 0x6f, 0x75, 0x6e, 0x74, 0x65, 0x64, 0x10, 0x68,
	0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x33,
}

var (
	file_mount_error_tip_proto_rawDescOnce sync.Once
	file_mount_error_tip_proto_rawDescData = file_mount_error_tip_proto_rawDesc
)

func file_mount_error_tip_proto_rawDescGZIP() []byte {
	file_mount_error_tip_proto_rawDescOnce.Do(func() {
		file_mount_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_mount_error_tip_proto_rawDescData)
	})
	return file_mount_error_tip_proto_rawDescData
}

var file_mount_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_mount_error_tip_proto_goTypes = []any{
	(MountError)(0), // 0: mount_error
}
var file_mount_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_mount_error_tip_proto_init() }
func file_mount_error_tip_proto_init() {
	if File_mount_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_mount_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_mount_error_tip_proto_goTypes,
		DependencyIndexes: file_mount_error_tip_proto_depIdxs,
		EnumInfos:         file_mount_error_tip_proto_enumTypes,
	}.Build()
	File_mount_error_tip_proto = out.File
	file_mount_error_tip_proto_rawDesc = nil
	file_mount_error_tip_proto_goTypes = nil
	file_mount_error_tip_proto_depIdxs = nil
}
