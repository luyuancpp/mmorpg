// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/common/statistics.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
	unsafe "unsafe"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type MessageStatistics struct {
	state          protoimpl.MessageState `protogen:"open.v1"`
	Count          uint32                 `protobuf:"varint,1,opt,name=count,proto3" json:"count,omitempty"`
	FlowRateSecond uint32                 `protobuf:"varint,2,opt,name=flow_rate_second,json=flowRateSecond,proto3" json:"flow_rate_second,omitempty"`
	FlowRateTotal  uint64                 `protobuf:"varint,3,opt,name=flow_rate_total,json=flowRateTotal,proto3" json:"flow_rate_total,omitempty"`
	Second         uint32                 `protobuf:"varint,4,opt,name=second,proto3" json:"second,omitempty"`
	unknownFields  protoimpl.UnknownFields
	sizeCache      protoimpl.SizeCache
}

func (x *MessageStatistics) Reset() {
	*x = MessageStatistics{}
	mi := &file_proto_common_statistics_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *MessageStatistics) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*MessageStatistics) ProtoMessage() {}

func (x *MessageStatistics) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_statistics_proto_msgTypes[0]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use MessageStatistics.ProtoReflect.Descriptor instead.
func (*MessageStatistics) Descriptor() ([]byte, []int) {
	return file_proto_common_statistics_proto_rawDescGZIP(), []int{0}
}

func (x *MessageStatistics) GetCount() uint32 {
	if x != nil {
		return x.Count
	}
	return 0
}

func (x *MessageStatistics) GetFlowRateSecond() uint32 {
	if x != nil {
		return x.FlowRateSecond
	}
	return 0
}

func (x *MessageStatistics) GetFlowRateTotal() uint64 {
	if x != nil {
		return x.FlowRateTotal
	}
	return 0
}

func (x *MessageStatistics) GetSecond() uint32 {
	if x != nil {
		return x.Second
	}
	return 0
}

var File_proto_common_statistics_proto protoreflect.FileDescriptor

const file_proto_common_statistics_proto_rawDesc = "" +
	"\n" +
	"\x1dproto/common/statistics.proto\"\x93\x01\n" +
	"\x11MessageStatistics\x12\x14\n" +
	"\x05count\x18\x01 \x01(\rR\x05count\x12(\n" +
	"\x10flow_rate_second\x18\x02 \x01(\rR\x0eflowRateSecond\x12&\n" +
	"\x0fflow_rate_total\x18\x03 \x01(\x04R\rflowRateTotal\x12\x16\n" +
	"\x06second\x18\x04 \x01(\rR\x06secondB\tZ\apb/gameb\x06proto3"

var (
	file_proto_common_statistics_proto_rawDescOnce sync.Once
	file_proto_common_statistics_proto_rawDescData []byte
)

func file_proto_common_statistics_proto_rawDescGZIP() []byte {
	file_proto_common_statistics_proto_rawDescOnce.Do(func() {
		file_proto_common_statistics_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_common_statistics_proto_rawDesc), len(file_proto_common_statistics_proto_rawDesc)))
	})
	return file_proto_common_statistics_proto_rawDescData
}

var file_proto_common_statistics_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_proto_common_statistics_proto_goTypes = []any{
	(*MessageStatistics)(nil), // 0: MessageStatistics
}
var file_proto_common_statistics_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_common_statistics_proto_init() }
func file_proto_common_statistics_proto_init() {
	if File_proto_common_statistics_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_common_statistics_proto_rawDesc), len(file_proto_common_statistics_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_common_statistics_proto_goTypes,
		DependencyIndexes: file_proto_common_statistics_proto_depIdxs,
		MessageInfos:      file_proto_common_statistics_proto_msgTypes,
	}.Build()
	File_proto_common_statistics_proto = out.File
	file_proto_common_statistics_proto_goTypes = nil
	file_proto_common_statistics_proto_depIdxs = nil
}
