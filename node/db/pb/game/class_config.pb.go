// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: class_config.proto

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

type ClassTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id    uint32   `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Skill []uint32 `protobuf:"varint,2,rep,packed,name=skill,proto3" json:"skill,omitempty"`
}

func (x *ClassTable) Reset() {
	*x = ClassTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_class_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ClassTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ClassTable) ProtoMessage() {}

func (x *ClassTable) ProtoReflect() protoreflect.Message {
	mi := &file_class_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ClassTable.ProtoReflect.Descriptor instead.
func (*ClassTable) Descriptor() ([]byte, []int) {
	return file_class_config_proto_rawDescGZIP(), []int{0}
}

func (x *ClassTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *ClassTable) GetSkill() []uint32 {
	if x != nil {
		return x.Skill
	}
	return nil
}

type ClassTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*ClassTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *ClassTabledData) Reset() {
	*x = ClassTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_class_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ClassTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ClassTabledData) ProtoMessage() {}

func (x *ClassTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_class_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ClassTabledData.ProtoReflect.Descriptor instead.
func (*ClassTabledData) Descriptor() ([]byte, []int) {
	return file_class_config_proto_rawDescGZIP(), []int{1}
}

func (x *ClassTabledData) GetData() []*ClassTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_class_config_proto protoreflect.FileDescriptor

var file_class_config_proto_rawDesc = []byte{
	0x0a, 0x12, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x22, 0x32, 0x0a, 0x0a, 0x43, 0x6c, 0x61, 0x73, 0x73, 0x54, 0x61, 0x62,
	0x6c, 0x65, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x02,
	0x69, 0x64, 0x12, 0x14, 0x0a, 0x05, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x18, 0x02, 0x20, 0x03, 0x28,
	0x0d, 0x52, 0x05, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x22, 0x32, 0x0a, 0x0f, 0x43, 0x6c, 0x61, 0x73,
	0x73, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44, 0x61, 0x74, 0x61, 0x12, 0x1f, 0x0a, 0x04, 0x64,
	0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x0b, 0x2e, 0x43, 0x6c, 0x61, 0x73,
	0x73, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07,
	0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_class_config_proto_rawDescOnce sync.Once
	file_class_config_proto_rawDescData = file_class_config_proto_rawDesc
)

func file_class_config_proto_rawDescGZIP() []byte {
	file_class_config_proto_rawDescOnce.Do(func() {
		file_class_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_class_config_proto_rawDescData)
	})
	return file_class_config_proto_rawDescData
}

var file_class_config_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_class_config_proto_goTypes = []any{
	(*ClassTable)(nil),      // 0: ClassTable
	(*ClassTabledData)(nil), // 1: ClassTabledData
}
var file_class_config_proto_depIdxs = []int32{
	0, // 0: ClassTabledData.data:type_name -> ClassTable
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_class_config_proto_init() }
func file_class_config_proto_init() {
	if File_class_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_class_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*ClassTable); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_class_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*ClassTabledData); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_class_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_class_config_proto_goTypes,
		DependencyIndexes: file_class_config_proto_depIdxs,
		MessageInfos:      file_class_config_proto_msgTypes,
	}.Build()
	File_class_config_proto = out.File
	file_class_config_proto_rawDesc = nil
	file_class_config_proto_goTypes = nil
	file_class_config_proto_depIdxs = nil
}
