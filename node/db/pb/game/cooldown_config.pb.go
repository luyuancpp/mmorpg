// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: cooldown_config.proto

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

type CooldownRow struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id       uint32 `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Duration uint32 `protobuf:"varint,2,opt,name=duration,proto3" json:"duration,omitempty"`
}

func (x *CooldownRow) Reset() {
	*x = CooldownRow{}
	if protoimpl.UnsafeEnabled {
		mi := &file_cooldown_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CooldownRow) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CooldownRow) ProtoMessage() {}

func (x *CooldownRow) ProtoReflect() protoreflect.Message {
	mi := &file_cooldown_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CooldownRow.ProtoReflect.Descriptor instead.
func (*CooldownRow) Descriptor() ([]byte, []int) {
	return file_cooldown_config_proto_rawDescGZIP(), []int{0}
}

func (x *CooldownRow) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *CooldownRow) GetDuration() uint32 {
	if x != nil {
		return x.Duration
	}
	return 0
}

type CooldownTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*CooldownRow `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *CooldownTable) Reset() {
	*x = CooldownTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_cooldown_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CooldownTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CooldownTable) ProtoMessage() {}

func (x *CooldownTable) ProtoReflect() protoreflect.Message {
	mi := &file_cooldown_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CooldownTable.ProtoReflect.Descriptor instead.
func (*CooldownTable) Descriptor() ([]byte, []int) {
	return file_cooldown_config_proto_rawDescGZIP(), []int{1}
}

func (x *CooldownTable) GetData() []*CooldownRow {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_cooldown_config_proto protoreflect.FileDescriptor

var file_cooldown_config_proto_rawDesc = []byte{
	0x0a, 0x15, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69,
	0x67, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x3a, 0x0a, 0x0c, 0x63, 0x6f, 0x6f, 0x6c, 0x64,
	0x6f, 0x77, 0x6e, 0x5f, 0x72, 0x6f, 0x77, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x1a, 0x0a, 0x08, 0x64, 0x75, 0x72, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08, 0x64, 0x75, 0x72, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x22, 0x33, 0x0a, 0x0e, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x5f,
	0x74, 0x61, 0x62, 0x6c, 0x65, 0x12, 0x21, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20,
	0x03, 0x28, 0x0b, 0x32, 0x0d, 0x2e, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x5f, 0x72,
	0x6f, 0x77, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67,
	0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_cooldown_config_proto_rawDescOnce sync.Once
	file_cooldown_config_proto_rawDescData = file_cooldown_config_proto_rawDesc
)

func file_cooldown_config_proto_rawDescGZIP() []byte {
	file_cooldown_config_proto_rawDescOnce.Do(func() {
		file_cooldown_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_cooldown_config_proto_rawDescData)
	})
	return file_cooldown_config_proto_rawDescData
}

var file_cooldown_config_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_cooldown_config_proto_goTypes = []any{
	(*CooldownRow)(nil),   // 0: cooldown_row
	(*CooldownTable)(nil), // 1: cooldown_table
}
var file_cooldown_config_proto_depIdxs = []int32{
	0, // 0: cooldown_table.data:type_name -> cooldown_row
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_cooldown_config_proto_init() }
func file_cooldown_config_proto_init() {
	if File_cooldown_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_cooldown_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*CooldownRow); i {
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
		file_cooldown_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*CooldownTable); i {
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
			RawDescriptor: file_cooldown_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_cooldown_config_proto_goTypes,
		DependencyIndexes: file_cooldown_config_proto_depIdxs,
		MessageInfos:      file_cooldown_config_proto_msgTypes,
	}.Build()
	File_cooldown_config_proto = out.File
	file_cooldown_config_proto_rawDesc = nil
	file_cooldown_config_proto_goTypes = nil
	file_cooldown_config_proto_depIdxs = nil
}
