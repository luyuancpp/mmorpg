// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: messagelimiter_config.proto

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

type MessageLimiterTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id          uint32 `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	MaxRequests uint32 `protobuf:"varint,2,opt,name=MaxRequests,proto3" json:"MaxRequests,omitempty"`
	TimeWindow  uint32 `protobuf:"varint,3,opt,name=TimeWindow,proto3" json:"TimeWindow,omitempty"`
	TipMessage  uint32 `protobuf:"varint,4,opt,name=TipMessage,proto3" json:"TipMessage,omitempty"`
}

func (x *MessageLimiterTable) Reset() {
	*x = MessageLimiterTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_messagelimiter_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *MessageLimiterTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*MessageLimiterTable) ProtoMessage() {}

func (x *MessageLimiterTable) ProtoReflect() protoreflect.Message {
	mi := &file_messagelimiter_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use MessageLimiterTable.ProtoReflect.Descriptor instead.
func (*MessageLimiterTable) Descriptor() ([]byte, []int) {
	return file_messagelimiter_config_proto_rawDescGZIP(), []int{0}
}

func (x *MessageLimiterTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *MessageLimiterTable) GetMaxRequests() uint32 {
	if x != nil {
		return x.MaxRequests
	}
	return 0
}

func (x *MessageLimiterTable) GetTimeWindow() uint32 {
	if x != nil {
		return x.TimeWindow
	}
	return 0
}

func (x *MessageLimiterTable) GetTipMessage() uint32 {
	if x != nil {
		return x.TipMessage
	}
	return 0
}

type MessageLimiterTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*MessageLimiterTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *MessageLimiterTabledData) Reset() {
	*x = MessageLimiterTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_messagelimiter_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *MessageLimiterTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*MessageLimiterTabledData) ProtoMessage() {}

func (x *MessageLimiterTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_messagelimiter_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use MessageLimiterTabledData.ProtoReflect.Descriptor instead.
func (*MessageLimiterTabledData) Descriptor() ([]byte, []int) {
	return file_messagelimiter_config_proto_rawDescGZIP(), []int{1}
}

func (x *MessageLimiterTabledData) GetData() []*MessageLimiterTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_messagelimiter_config_proto protoreflect.FileDescriptor

var file_messagelimiter_config_proto_rawDesc = []byte{
	0x0a, 0x1b, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x6c, 0x69, 0x6d, 0x69, 0x74, 0x65, 0x72,
	0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x87, 0x01,
	0x0a, 0x13, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x4c, 0x69, 0x6d, 0x69, 0x74, 0x65, 0x72,
	0x54, 0x61, 0x62, 0x6c, 0x65, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x20, 0x0a, 0x0b, 0x4d, 0x61, 0x78, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x73, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x4d, 0x61, 0x78, 0x52,
	0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x73, 0x12, 0x1e, 0x0a, 0x0a, 0x54, 0x69, 0x6d, 0x65, 0x57,
	0x69, 0x6e, 0x64, 0x6f, 0x77, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x54, 0x69, 0x6d,
	0x65, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x12, 0x1e, 0x0a, 0x0a, 0x54, 0x69, 0x70, 0x4d, 0x65,
	0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x54, 0x69, 0x70,
	0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x22, 0x44, 0x0a, 0x18, 0x4d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x4c, 0x69, 0x6d, 0x69, 0x74, 0x65, 0x72, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44,
	0x61, 0x74, 0x61, 0x12, 0x28, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28,
	0x0b, 0x32, 0x14, 0x2e, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x4c, 0x69, 0x6d, 0x69, 0x74,
	0x65, 0x72, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a,
	0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_messagelimiter_config_proto_rawDescOnce sync.Once
	file_messagelimiter_config_proto_rawDescData = file_messagelimiter_config_proto_rawDesc
)

func file_messagelimiter_config_proto_rawDescGZIP() []byte {
	file_messagelimiter_config_proto_rawDescOnce.Do(func() {
		file_messagelimiter_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_messagelimiter_config_proto_rawDescData)
	})
	return file_messagelimiter_config_proto_rawDescData
}

var file_messagelimiter_config_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_messagelimiter_config_proto_goTypes = []any{
	(*MessageLimiterTable)(nil),      // 0: MessageLimiterTable
	(*MessageLimiterTabledData)(nil), // 1: MessageLimiterTabledData
}
var file_messagelimiter_config_proto_depIdxs = []int32{
	0, // 0: MessageLimiterTabledData.data:type_name -> MessageLimiterTable
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_messagelimiter_config_proto_init() }
func file_messagelimiter_config_proto_init() {
	if File_messagelimiter_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_messagelimiter_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*MessageLimiterTable); i {
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
		file_messagelimiter_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*MessageLimiterTabledData); i {
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
			RawDescriptor: file_messagelimiter_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_messagelimiter_config_proto_goTypes,
		DependencyIndexes: file_messagelimiter_config_proto_depIdxs,
		MessageInfos:      file_messagelimiter_config_proto_msgTypes,
	}.Build()
	File_messagelimiter_config_proto = out.File
	file_messagelimiter_config_proto_rawDesc = nil
	file_messagelimiter_config_proto_goTypes = nil
	file_messagelimiter_config_proto_depIdxs = nil
}
