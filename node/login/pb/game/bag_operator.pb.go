// Proto file for bag

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v6.31.0--dev
// source: bag_operator.proto

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

type Bag int32

const (
	Bag_kBagOK              Bag = 0
	Bag_kUseItem            Bag = 1
	Bag_kActivityRewardItem Bag = 2
	Bag_kMissionRewardItem  Bag = 3
)

// Enum value maps for Bag.
var (
	Bag_name = map[int32]string{
		0: "kBagOK",
		1: "kUseItem",
		2: "kActivityRewardItem",
		3: "kMissionRewardItem",
	}
	Bag_value = map[string]int32{
		"kBagOK":              0,
		"kUseItem":            1,
		"kActivityRewardItem": 2,
		"kMissionRewardItem":  3,
	}
)

func (x Bag) Enum() *Bag {
	p := new(Bag)
	*p = x
	return p
}

func (x Bag) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (Bag) Descriptor() protoreflect.EnumDescriptor {
	return file_bag_operator_proto_enumTypes[0].Descriptor()
}

func (Bag) Type() protoreflect.EnumType {
	return &file_bag_operator_proto_enumTypes[0]
}

func (x Bag) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use Bag.Descriptor instead.
func (Bag) EnumDescriptor() ([]byte, []int) {
	return file_bag_operator_proto_rawDescGZIP(), []int{0}
}

var File_bag_operator_proto protoreflect.FileDescriptor

var file_bag_operator_proto_rawDesc = []byte{
	0x0a, 0x12, 0x62, 0x61, 0x67, 0x5f, 0x6f, 0x70, 0x65, 0x72, 0x61, 0x74, 0x6f, 0x72, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x50, 0x0a, 0x03, 0x62, 0x61, 0x67, 0x12, 0x0a, 0x0a, 0x06, 0x6b,
	0x42, 0x61, 0x67, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x0c, 0x0a, 0x08, 0x6b, 0x55, 0x73, 0x65, 0x49,
	0x74, 0x65, 0x6d, 0x10, 0x01, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x41, 0x63, 0x74, 0x69, 0x76, 0x69,
	0x74, 0x79, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x49, 0x74, 0x65, 0x6d, 0x10, 0x02, 0x12, 0x16,
	0x0a, 0x12, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64,
	0x49, 0x74, 0x65, 0x6d, 0x10, 0x03, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d,
	0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_bag_operator_proto_rawDescOnce sync.Once
	file_bag_operator_proto_rawDescData = file_bag_operator_proto_rawDesc
)

func file_bag_operator_proto_rawDescGZIP() []byte {
	file_bag_operator_proto_rawDescOnce.Do(func() {
		file_bag_operator_proto_rawDescData = protoimpl.X.CompressGZIP(file_bag_operator_proto_rawDescData)
	})
	return file_bag_operator_proto_rawDescData
}

var file_bag_operator_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_bag_operator_proto_goTypes = []any{
	(Bag)(0), // 0: bag
}
var file_bag_operator_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_bag_operator_proto_init() }
func file_bag_operator_proto_init() {
	if File_bag_operator_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_bag_operator_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_bag_operator_proto_goTypes,
		DependencyIndexes: file_bag_operator_proto_depIdxs,
		EnumInfos:         file_bag_operator_proto_enumTypes,
	}.Build()
	File_bag_operator_proto = out.File
	file_bag_operator_proto_rawDesc = nil
	file_bag_operator_proto_goTypes = nil
	file_bag_operator_proto_depIdxs = nil
}
