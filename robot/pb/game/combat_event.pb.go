// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/logic/event/combat_event.proto

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

type BeKillEvent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	Caster        uint64                 `protobuf:"varint,1,opt,name=caster,proto3" json:"caster,omitempty"`
	Target        uint64                 `protobuf:"varint,2,opt,name=target,proto3" json:"target,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *BeKillEvent) Reset() {
	*x = BeKillEvent{}
	mi := &file_proto_logic_event_combat_event_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BeKillEvent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BeKillEvent) ProtoMessage() {}

func (x *BeKillEvent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_combat_event_proto_msgTypes[0]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BeKillEvent.ProtoReflect.Descriptor instead.
func (*BeKillEvent) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_combat_event_proto_rawDescGZIP(), []int{0}
}

func (x *BeKillEvent) GetCaster() uint64 {
	if x != nil {
		return x.Caster
	}
	return 0
}

func (x *BeKillEvent) GetTarget() uint64 {
	if x != nil {
		return x.Target
	}
	return 0
}

var File_proto_logic_event_combat_event_proto protoreflect.FileDescriptor

const file_proto_logic_event_combat_event_proto_rawDesc = "" +
	"\n" +
	"$proto/logic/event/combat_event.proto\"=\n" +
	"\vBeKillEvent\x12\x16\n" +
	"\x06caster\x18\x01 \x01(\x04R\x06caster\x12\x16\n" +
	"\x06target\x18\x02 \x01(\x04R\x06targetB\tZ\apb/gameb\x06proto3"

var (
	file_proto_logic_event_combat_event_proto_rawDescOnce sync.Once
	file_proto_logic_event_combat_event_proto_rawDescData []byte
)

func file_proto_logic_event_combat_event_proto_rawDescGZIP() []byte {
	file_proto_logic_event_combat_event_proto_rawDescOnce.Do(func() {
		file_proto_logic_event_combat_event_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_logic_event_combat_event_proto_rawDesc), len(file_proto_logic_event_combat_event_proto_rawDesc)))
	})
	return file_proto_logic_event_combat_event_proto_rawDescData
}

var file_proto_logic_event_combat_event_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_proto_logic_event_combat_event_proto_goTypes = []any{
	(*BeKillEvent)(nil), // 0: BeKillEvent
}
var file_proto_logic_event_combat_event_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_logic_event_combat_event_proto_init() }
func file_proto_logic_event_combat_event_proto_init() {
	if File_proto_logic_event_combat_event_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_logic_event_combat_event_proto_rawDesc), len(file_proto_logic_event_combat_event_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_event_combat_event_proto_goTypes,
		DependencyIndexes: file_proto_logic_event_combat_event_proto_depIdxs,
		MessageInfos:      file_proto_logic_event_combat_event_proto_msgTypes,
	}.Build()
	File_proto_logic_event_combat_event_proto = out.File
	file_proto_logic_event_combat_event_proto_goTypes = nil
	file_proto_logic_event_combat_event_proto_depIdxs = nil
}
