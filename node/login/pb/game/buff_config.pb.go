// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: buff_config.proto

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

type BuffRow struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id             uint32          `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Nocaster       uint32          `protobuf:"varint,2,opt,name=nocaster,proto3" json:"nocaster,omitempty"`
	Tag            map[string]bool `protobuf:"bytes,3,rep,name=tag,proto3" json:"tag,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
	Immunetag      map[string]bool `protobuf:"bytes,4,rep,name=immunetag,proto3" json:"immunetag,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
	Level          uint32          `protobuf:"varint,5,opt,name=level,proto3" json:"level,omitempty"`
	Maxlayer       uint32          `protobuf:"varint,6,opt,name=maxlayer,proto3" json:"maxlayer,omitempty"`
	Duration       uint32          `protobuf:"varint,7,opt,name=duration,proto3" json:"duration,omitempty"`
	Forceinterrupt uint32          `protobuf:"varint,8,opt,name=forceinterrupt,proto3" json:"forceinterrupt,omitempty"`
	Interval       uint32          `protobuf:"varint,9,opt,name=interval,proto3" json:"interval,omitempty"`
}

func (x *BuffRow) Reset() {
	*x = BuffRow{}
	if protoimpl.UnsafeEnabled {
		mi := &file_buff_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BuffRow) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffRow) ProtoMessage() {}

func (x *BuffRow) ProtoReflect() protoreflect.Message {
	mi := &file_buff_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffRow.ProtoReflect.Descriptor instead.
func (*BuffRow) Descriptor() ([]byte, []int) {
	return file_buff_config_proto_rawDescGZIP(), []int{0}
}

func (x *BuffRow) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *BuffRow) GetNocaster() uint32 {
	if x != nil {
		return x.Nocaster
	}
	return 0
}

func (x *BuffRow) GetTag() map[string]bool {
	if x != nil {
		return x.Tag
	}
	return nil
}

func (x *BuffRow) GetImmunetag() map[string]bool {
	if x != nil {
		return x.Immunetag
	}
	return nil
}

func (x *BuffRow) GetLevel() uint32 {
	if x != nil {
		return x.Level
	}
	return 0
}

func (x *BuffRow) GetMaxlayer() uint32 {
	if x != nil {
		return x.Maxlayer
	}
	return 0
}

func (x *BuffRow) GetDuration() uint32 {
	if x != nil {
		return x.Duration
	}
	return 0
}

func (x *BuffRow) GetForceinterrupt() uint32 {
	if x != nil {
		return x.Forceinterrupt
	}
	return 0
}

func (x *BuffRow) GetInterval() uint32 {
	if x != nil {
		return x.Interval
	}
	return 0
}

type BuffTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*BuffRow `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *BuffTable) Reset() {
	*x = BuffTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_buff_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BuffTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffTable) ProtoMessage() {}

func (x *BuffTable) ProtoReflect() protoreflect.Message {
	mi := &file_buff_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffTable.ProtoReflect.Descriptor instead.
func (*BuffTable) Descriptor() ([]byte, []int) {
	return file_buff_config_proto_rawDescGZIP(), []int{1}
}

func (x *BuffTable) GetData() []*BuffRow {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_buff_config_proto protoreflect.FileDescriptor

var file_buff_config_proto_rawDesc = []byte{
	0x0a, 0x11, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x22, 0x9c, 0x03, 0x0a, 0x08, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x72, 0x6f, 0x77,
	0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64,
	0x12, 0x1a, 0x0a, 0x08, 0x6e, 0x6f, 0x63, 0x61, 0x73, 0x74, 0x65, 0x72, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x08, 0x6e, 0x6f, 0x63, 0x61, 0x73, 0x74, 0x65, 0x72, 0x12, 0x24, 0x0a, 0x03,
	0x74, 0x61, 0x67, 0x18, 0x03, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x12, 0x2e, 0x62, 0x75, 0x66, 0x66,
	0x5f, 0x72, 0x6f, 0x77, 0x2e, 0x54, 0x61, 0x67, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x03, 0x74,
	0x61, 0x67, 0x12, 0x36, 0x0a, 0x09, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x18,
	0x04, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x18, 0x2e, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x72, 0x6f, 0x77,
	0x2e, 0x49, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52,
	0x09, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x12, 0x14, 0x0a, 0x05, 0x6c, 0x65,
	0x76, 0x65, 0x6c, 0x18, 0x05, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x05, 0x6c, 0x65, 0x76, 0x65, 0x6c,
	0x12, 0x1a, 0x0a, 0x08, 0x6d, 0x61, 0x78, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x18, 0x06, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x08, 0x6d, 0x61, 0x78, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x12, 0x1a, 0x0a, 0x08,
	0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x07, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08,
	0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x26, 0x0a, 0x0e, 0x66, 0x6f, 0x72, 0x63,
	0x65, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x18, 0x08, 0x20, 0x01, 0x28, 0x0d,
	0x52, 0x0e, 0x66, 0x6f, 0x72, 0x63, 0x65, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74,
	0x12, 0x1a, 0x0a, 0x08, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6c, 0x18, 0x09, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x08, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6c, 0x1a, 0x36, 0x0a, 0x08,
	0x54, 0x61, 0x67, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x3a, 0x02, 0x38, 0x01, 0x1a, 0x3c, 0x0a, 0x0e, 0x49, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61,
	0x67, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75,
	0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02,
	0x38, 0x01, 0x22, 0x2b, 0x0a, 0x0a, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65,
	0x12, 0x1d, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x09,
	0x2e, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x72, 0x6f, 0x77, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42,
	0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x33,
}

var (
	file_buff_config_proto_rawDescOnce sync.Once
	file_buff_config_proto_rawDescData = file_buff_config_proto_rawDesc
)

func file_buff_config_proto_rawDescGZIP() []byte {
	file_buff_config_proto_rawDescOnce.Do(func() {
		file_buff_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_buff_config_proto_rawDescData)
	})
	return file_buff_config_proto_rawDescData
}

var file_buff_config_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_buff_config_proto_goTypes = []any{
	(*BuffRow)(nil),   // 0: buff_row
	(*BuffTable)(nil), // 1: buff_table
	nil,               // 2: buff_row.TagEntry
	nil,               // 3: buff_row.ImmunetagEntry
}
var file_buff_config_proto_depIdxs = []int32{
	2, // 0: buff_row.tag:type_name -> buff_row.TagEntry
	3, // 1: buff_row.immunetag:type_name -> buff_row.ImmunetagEntry
	0, // 2: buff_table.data:type_name -> buff_row
	3, // [3:3] is the sub-list for method output_type
	3, // [3:3] is the sub-list for method input_type
	3, // [3:3] is the sub-list for extension type_name
	3, // [3:3] is the sub-list for extension extendee
	0, // [0:3] is the sub-list for field type_name
}

func init() { file_buff_config_proto_init() }
func file_buff_config_proto_init() {
	if File_buff_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_buff_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*BuffRow); i {
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
		file_buff_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*BuffTable); i {
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
			RawDescriptor: file_buff_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_buff_config_proto_goTypes,
		DependencyIndexes: file_buff_config_proto_depIdxs,
		MessageInfos:      file_buff_config_proto_msgTypes,
	}.Build()
	File_buff_config_proto = out.File
	file_buff_config_proto_rawDesc = nil
	file_buff_config_proto_goTypes = nil
	file_buff_config_proto_depIdxs = nil
}
