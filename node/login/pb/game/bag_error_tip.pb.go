// Proto file for bag_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: bag_error_tip.proto

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

type BagError int32

const (
	BagError_kBag_errorOK                   BagError = 0
	BagError_kBagDeleteItemFindGuid         BagError = 77
	BagError_kBagDeleteItemAlreadyHasGuid   BagError = 78
	BagError_kBagAddItemHasNotBaseComponent BagError = 79
	BagError_kBagAddItemInvalidGuid         BagError = 80
	BagError_kBagAddItemInvalidParam        BagError = 81
	BagError_kBagAddItemBagFull             BagError = 82
	BagError_kBagItemNotStacked             BagError = 83
	BagError_kBagInsufficientItems          BagError = 84
	BagError_kBagDelItemPos                 BagError = 85
	BagError_kBagDelItemConfig              BagError = 86
	BagError_kBagDelItemGuid                BagError = 87
	BagError_kBagDelItemFindItem            BagError = 88
	BagError_kBagDelItemSize                BagError = 89
	BagError_kBagItemDeletionSizeMismatch   BagError = 90
	BagError_kBagInsufficientBagSpace       BagError = 91
)

// Enum value maps for BagError.
var (
	BagError_name = map[int32]string{
		0:  "kBag_errorOK",
		77: "kBagDeleteItemFindGuid",
		78: "kBagDeleteItemAlreadyHasGuid",
		79: "kBagAddItemHasNotBaseComponent",
		80: "kBagAddItemInvalidGuid",
		81: "kBagAddItemInvalidParam",
		82: "kBagAddItemBagFull",
		83: "kBagItemNotStacked",
		84: "kBagInsufficientItems",
		85: "kBagDelItemPos",
		86: "kBagDelItemConfig",
		87: "kBagDelItemGuid",
		88: "kBagDelItemFindItem",
		89: "kBagDelItemSize",
		90: "kBagItemDeletionSizeMismatch",
		91: "kBagInsufficientBagSpace",
	}
	BagError_value = map[string]int32{
		"kBag_errorOK":                   0,
		"kBagDeleteItemFindGuid":         77,
		"kBagDeleteItemAlreadyHasGuid":   78,
		"kBagAddItemHasNotBaseComponent": 79,
		"kBagAddItemInvalidGuid":         80,
		"kBagAddItemInvalidParam":        81,
		"kBagAddItemBagFull":             82,
		"kBagItemNotStacked":             83,
		"kBagInsufficientItems":          84,
		"kBagDelItemPos":                 85,
		"kBagDelItemConfig":              86,
		"kBagDelItemGuid":                87,
		"kBagDelItemFindItem":            88,
		"kBagDelItemSize":                89,
		"kBagItemDeletionSizeMismatch":   90,
		"kBagInsufficientBagSpace":       91,
	}
)

func (x BagError) Enum() *BagError {
	p := new(BagError)
	*p = x
	return p
}

func (x BagError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (BagError) Descriptor() protoreflect.EnumDescriptor {
	return file_bag_error_tip_proto_enumTypes[0].Descriptor()
}

func (BagError) Type() protoreflect.EnumType {
	return &file_bag_error_tip_proto_enumTypes[0]
}

func (x BagError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use BagError.Descriptor instead.
func (BagError) EnumDescriptor() ([]byte, []int) {
	return file_bag_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_bag_error_tip_proto protoreflect.FileDescriptor

var file_bag_error_tip_proto_rawDesc = []byte{
	0x0a, 0x13, 0x62, 0x61, 0x67, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69, 0x70, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xb1, 0x03, 0x0a, 0x09, 0x62, 0x61, 0x67, 0x5f, 0x65, 0x72,
	0x72, 0x6f, 0x72, 0x12, 0x10, 0x0a, 0x0c, 0x6b, 0x42, 0x61, 0x67, 0x5f, 0x65, 0x72, 0x72, 0x6f,
	0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x42, 0x61, 0x67, 0x44, 0x65, 0x6c,
	0x65, 0x74, 0x65, 0x49, 0x74, 0x65, 0x6d, 0x46, 0x69, 0x6e, 0x64, 0x47, 0x75, 0x69, 0x64, 0x10,
	0x4d, 0x12, 0x20, 0x0a, 0x1c, 0x6b, 0x42, 0x61, 0x67, 0x44, 0x65, 0x6c, 0x65, 0x74, 0x65, 0x49,
	0x74, 0x65, 0x6d, 0x41, 0x6c, 0x72, 0x65, 0x61, 0x64, 0x79, 0x48, 0x61, 0x73, 0x47, 0x75, 0x69,
	0x64, 0x10, 0x4e, 0x12, 0x22, 0x0a, 0x1e, 0x6b, 0x42, 0x61, 0x67, 0x41, 0x64, 0x64, 0x49, 0x74,
	0x65, 0x6d, 0x48, 0x61, 0x73, 0x4e, 0x6f, 0x74, 0x42, 0x61, 0x73, 0x65, 0x43, 0x6f, 0x6d, 0x70,
	0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x10, 0x4f, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x42, 0x61, 0x67, 0x41,
	0x64, 0x64, 0x49, 0x74, 0x65, 0x6d, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x47, 0x75, 0x69,
	0x64, 0x10, 0x50, 0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x42, 0x61, 0x67, 0x41, 0x64, 0x64, 0x49, 0x74,
	0x65, 0x6d, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x50, 0x61, 0x72, 0x61, 0x6d, 0x10, 0x51,
	0x12, 0x16, 0x0a, 0x12, 0x6b, 0x42, 0x61, 0x67, 0x41, 0x64, 0x64, 0x49, 0x74, 0x65, 0x6d, 0x42,
	0x61, 0x67, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x52, 0x12, 0x16, 0x0a, 0x12, 0x6b, 0x42, 0x61, 0x67,
	0x49, 0x74, 0x65, 0x6d, 0x4e, 0x6f, 0x74, 0x53, 0x74, 0x61, 0x63, 0x6b, 0x65, 0x64, 0x10, 0x53,
	0x12, 0x19, 0x0a, 0x15, 0x6b, 0x42, 0x61, 0x67, 0x49, 0x6e, 0x73, 0x75, 0x66, 0x66, 0x69, 0x63,
	0x69, 0x65, 0x6e, 0x74, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x10, 0x54, 0x12, 0x12, 0x0a, 0x0e, 0x6b,
	0x42, 0x61, 0x67, 0x44, 0x65, 0x6c, 0x49, 0x74, 0x65, 0x6d, 0x50, 0x6f, 0x73, 0x10, 0x55, 0x12,
	0x15, 0x0a, 0x11, 0x6b, 0x42, 0x61, 0x67, 0x44, 0x65, 0x6c, 0x49, 0x74, 0x65, 0x6d, 0x43, 0x6f,
	0x6e, 0x66, 0x69, 0x67, 0x10, 0x56, 0x12, 0x13, 0x0a, 0x0f, 0x6b, 0x42, 0x61, 0x67, 0x44, 0x65,
	0x6c, 0x49, 0x74, 0x65, 0x6d, 0x47, 0x75, 0x69, 0x64, 0x10, 0x57, 0x12, 0x17, 0x0a, 0x13, 0x6b,
	0x42, 0x61, 0x67, 0x44, 0x65, 0x6c, 0x49, 0x74, 0x65, 0x6d, 0x46, 0x69, 0x6e, 0x64, 0x49, 0x74,
	0x65, 0x6d, 0x10, 0x58, 0x12, 0x13, 0x0a, 0x0f, 0x6b, 0x42, 0x61, 0x67, 0x44, 0x65, 0x6c, 0x49,
	0x74, 0x65, 0x6d, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x59, 0x12, 0x20, 0x0a, 0x1c, 0x6b, 0x42, 0x61,
	0x67, 0x49, 0x74, 0x65, 0x6d, 0x44, 0x65, 0x6c, 0x65, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x69, 0x7a,
	0x65, 0x4d, 0x69, 0x73, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x10, 0x5a, 0x12, 0x1c, 0x0a, 0x18, 0x6b,
	0x42, 0x61, 0x67, 0x49, 0x6e, 0x73, 0x75, 0x66, 0x66, 0x69, 0x63, 0x69, 0x65, 0x6e, 0x74, 0x42,
	0x61, 0x67, 0x53, 0x70, 0x61, 0x63, 0x65, 0x10, 0x5b, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f,
	0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_bag_error_tip_proto_rawDescOnce sync.Once
	file_bag_error_tip_proto_rawDescData = file_bag_error_tip_proto_rawDesc
)

func file_bag_error_tip_proto_rawDescGZIP() []byte {
	file_bag_error_tip_proto_rawDescOnce.Do(func() {
		file_bag_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_bag_error_tip_proto_rawDescData)
	})
	return file_bag_error_tip_proto_rawDescData
}

var file_bag_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_bag_error_tip_proto_goTypes = []any{
	(BagError)(0), // 0: bag_error
}
var file_bag_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_bag_error_tip_proto_init() }
func file_bag_error_tip_proto_init() {
	if File_bag_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_bag_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_bag_error_tip_proto_goTypes,
		DependencyIndexes: file_bag_error_tip_proto_depIdxs,
		EnumInfos:         file_bag_error_tip_proto_enumTypes,
	}.Build()
	File_bag_error_tip_proto = out.File
	file_bag_error_tip_proto_rawDesc = nil
	file_bag_error_tip_proto_goTypes = nil
	file_bag_error_tip_proto_depIdxs = nil
}
