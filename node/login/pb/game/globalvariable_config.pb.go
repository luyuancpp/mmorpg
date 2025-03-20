// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: globalvariable_config.proto

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

type GlobalVariableTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id       uint32  `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Touint32 uint32  `protobuf:"varint,2,opt,name=touint32,proto3" json:"touint32,omitempty"`
	Toint32  int32   `protobuf:"varint,3,opt,name=toint32,proto3" json:"toint32,omitempty"`
	Tostring string  `protobuf:"bytes,4,opt,name=tostring,proto3" json:"tostring,omitempty"`
	Tofloat  float32 `protobuf:"fixed32,5,opt,name=tofloat,proto3" json:"tofloat,omitempty"`
	Todouble float64 `protobuf:"fixed64,6,opt,name=todouble,proto3" json:"todouble,omitempty"`
}

func (x *GlobalVariableTable) Reset() {
	*x = GlobalVariableTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_globalvariable_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GlobalVariableTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GlobalVariableTable) ProtoMessage() {}

func (x *GlobalVariableTable) ProtoReflect() protoreflect.Message {
	mi := &file_globalvariable_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GlobalVariableTable.ProtoReflect.Descriptor instead.
func (*GlobalVariableTable) Descriptor() ([]byte, []int) {
	return file_globalvariable_config_proto_rawDescGZIP(), []int{0}
}

func (x *GlobalVariableTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *GlobalVariableTable) GetTouint32() uint32 {
	if x != nil {
		return x.Touint32
	}
	return 0
}

func (x *GlobalVariableTable) GetToint32() int32 {
	if x != nil {
		return x.Toint32
	}
	return 0
}

func (x *GlobalVariableTable) GetTostring() string {
	if x != nil {
		return x.Tostring
	}
	return ""
}

func (x *GlobalVariableTable) GetTofloat() float32 {
	if x != nil {
		return x.Tofloat
	}
	return 0
}

func (x *GlobalVariableTable) GetTodouble() float64 {
	if x != nil {
		return x.Todouble
	}
	return 0
}

type GlobalVariableTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*GlobalVariableTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *GlobalVariableTabledData) Reset() {
	*x = GlobalVariableTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_globalvariable_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GlobalVariableTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GlobalVariableTabledData) ProtoMessage() {}

func (x *GlobalVariableTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_globalvariable_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GlobalVariableTabledData.ProtoReflect.Descriptor instead.
func (*GlobalVariableTabledData) Descriptor() ([]byte, []int) {
	return file_globalvariable_config_proto_rawDescGZIP(), []int{1}
}

func (x *GlobalVariableTabledData) GetData() []*GlobalVariableTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_globalvariable_config_proto protoreflect.FileDescriptor

var file_globalvariable_config_proto_rawDesc = []byte{
	0x0a, 0x1b, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x76, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65,
	0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0xad, 0x01,
	0x0a, 0x13, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x56, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65,
	0x54, 0x61, 0x62, 0x6c, 0x65, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x1a, 0x0a, 0x08, 0x74, 0x6f, 0x75, 0x69, 0x6e, 0x74, 0x33,
	0x32, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08, 0x74, 0x6f, 0x75, 0x69, 0x6e, 0x74, 0x33,
	0x32, 0x12, 0x18, 0x0a, 0x07, 0x74, 0x6f, 0x69, 0x6e, 0x74, 0x33, 0x32, 0x18, 0x03, 0x20, 0x01,
	0x28, 0x05, 0x52, 0x07, 0x74, 0x6f, 0x69, 0x6e, 0x74, 0x33, 0x32, 0x12, 0x1a, 0x0a, 0x08, 0x74,
	0x6f, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x18, 0x04, 0x20, 0x01, 0x28, 0x09, 0x52, 0x08, 0x74,
	0x6f, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x12, 0x18, 0x0a, 0x07, 0x74, 0x6f, 0x66, 0x6c, 0x6f,
	0x61, 0x74, 0x18, 0x05, 0x20, 0x01, 0x28, 0x02, 0x52, 0x07, 0x74, 0x6f, 0x66, 0x6c, 0x6f, 0x61,
	0x74, 0x12, 0x1a, 0x0a, 0x08, 0x74, 0x6f, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x18, 0x06, 0x20,
	0x01, 0x28, 0x01, 0x52, 0x08, 0x74, 0x6f, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x22, 0x44, 0x0a,
	0x18, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x56, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65, 0x54,
	0x61, 0x62, 0x6c, 0x65, 0x64, 0x44, 0x61, 0x74, 0x61, 0x12, 0x28, 0x0a, 0x04, 0x64, 0x61, 0x74,
	0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x14, 0x2e, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c,
	0x56, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x52, 0x04, 0x64,
	0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_globalvariable_config_proto_rawDescOnce sync.Once
	file_globalvariable_config_proto_rawDescData = file_globalvariable_config_proto_rawDesc
)

func file_globalvariable_config_proto_rawDescGZIP() []byte {
	file_globalvariable_config_proto_rawDescOnce.Do(func() {
		file_globalvariable_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_globalvariable_config_proto_rawDescData)
	})
	return file_globalvariable_config_proto_rawDescData
}

var file_globalvariable_config_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_globalvariable_config_proto_goTypes = []any{
	(*GlobalVariableTable)(nil),      // 0: GlobalVariableTable
	(*GlobalVariableTabledData)(nil), // 1: GlobalVariableTabledData
}
var file_globalvariable_config_proto_depIdxs = []int32{
	0, // 0: GlobalVariableTabledData.data:type_name -> GlobalVariableTable
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_globalvariable_config_proto_init() }
func file_globalvariable_config_proto_init() {
	if File_globalvariable_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_globalvariable_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*GlobalVariableTable); i {
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
		file_globalvariable_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*GlobalVariableTabledData); i {
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
			RawDescriptor: file_globalvariable_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_globalvariable_config_proto_goTypes,
		DependencyIndexes: file_globalvariable_config_proto_depIdxs,
		MessageInfos:      file_globalvariable_config_proto_msgTypes,
	}.Build()
	File_globalvariable_config_proto = out.File
	file_globalvariable_config_proto_rawDesc = nil
	file_globalvariable_config_proto_goTypes = nil
	file_globalvariable_config_proto_depIdxs = nil
}
