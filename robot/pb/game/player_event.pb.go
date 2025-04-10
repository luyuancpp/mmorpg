// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/event/player_event.proto

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

type RegisterPlayerEvent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ActorEntity uint64 `protobuf:"varint,1,opt,name=actor_entity,json=actorEntity,proto3" json:"actor_entity,omitempty"`
}

func (x *RegisterPlayerEvent) Reset() {
	*x = RegisterPlayerEvent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_player_event_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RegisterPlayerEvent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RegisterPlayerEvent) ProtoMessage() {}

func (x *RegisterPlayerEvent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_player_event_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RegisterPlayerEvent.ProtoReflect.Descriptor instead.
func (*RegisterPlayerEvent) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_player_event_proto_rawDescGZIP(), []int{0}
}

func (x *RegisterPlayerEvent) GetActorEntity() uint64 {
	if x != nil {
		return x.ActorEntity
	}
	return 0
}

type PlayerUpgradeEvent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ActorEntity uint64 `protobuf:"varint,1,opt,name=actor_entity,json=actorEntity,proto3" json:"actor_entity,omitempty"`
	NewLevel    uint32 `protobuf:"varint,2,opt,name=new_level,json=newLevel,proto3" json:"new_level,omitempty"`
}

func (x *PlayerUpgradeEvent) Reset() {
	*x = PlayerUpgradeEvent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_player_event_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerUpgradeEvent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerUpgradeEvent) ProtoMessage() {}

func (x *PlayerUpgradeEvent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_player_event_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerUpgradeEvent.ProtoReflect.Descriptor instead.
func (*PlayerUpgradeEvent) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_player_event_proto_rawDescGZIP(), []int{1}
}

func (x *PlayerUpgradeEvent) GetActorEntity() uint64 {
	if x != nil {
		return x.ActorEntity
	}
	return 0
}

func (x *PlayerUpgradeEvent) GetNewLevel() uint32 {
	if x != nil {
		return x.NewLevel
	}
	return 0
}

type InitializePlayerComponentsEvent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ActorEntity uint64 `protobuf:"varint,1,opt,name=actor_entity,json=actorEntity,proto3" json:"actor_entity,omitempty"`
}

func (x *InitializePlayerComponentsEvent) Reset() {
	*x = InitializePlayerComponentsEvent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_player_event_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *InitializePlayerComponentsEvent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*InitializePlayerComponentsEvent) ProtoMessage() {}

func (x *InitializePlayerComponentsEvent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_player_event_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use InitializePlayerComponentsEvent.ProtoReflect.Descriptor instead.
func (*InitializePlayerComponentsEvent) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_player_event_proto_rawDescGZIP(), []int{2}
}

func (x *InitializePlayerComponentsEvent) GetActorEntity() uint64 {
	if x != nil {
		return x.ActorEntity
	}
	return 0
}

var File_proto_logic_event_player_event_proto protoreflect.FileDescriptor

var file_proto_logic_event_player_event_proto_rawDesc = []byte{
	0x0a, 0x24, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x65, 0x76,
	0x65, 0x6e, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x65, 0x76, 0x65, 0x6e, 0x74,
	0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x38, 0x0a, 0x13, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74,
	0x65, 0x72, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x45, 0x76, 0x65, 0x6e, 0x74, 0x12, 0x21, 0x0a,
	0x0c, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x0b, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x45, 0x6e, 0x74, 0x69, 0x74, 0x79,
	0x22, 0x54, 0x0a, 0x12, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x55, 0x70, 0x67, 0x72, 0x61, 0x64,
	0x65, 0x45, 0x76, 0x65, 0x6e, 0x74, 0x12, 0x21, 0x0a, 0x0c, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f,
	0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0b, 0x61, 0x63,
	0x74, 0x6f, 0x72, 0x45, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x12, 0x1b, 0x0a, 0x09, 0x6e, 0x65, 0x77,
	0x5f, 0x6c, 0x65, 0x76, 0x65, 0x6c, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08, 0x6e, 0x65,
	0x77, 0x4c, 0x65, 0x76, 0x65, 0x6c, 0x22, 0x44, 0x0a, 0x1f, 0x49, 0x6e, 0x69, 0x74, 0x69, 0x61,
	0x6c, 0x69, 0x7a, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e,
	0x65, 0x6e, 0x74, 0x73, 0x45, 0x76, 0x65, 0x6e, 0x74, 0x12, 0x21, 0x0a, 0x0c, 0x61, 0x63, 0x74,
	0x6f, 0x72, 0x5f, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52,
	0x0b, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x45, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x42, 0x09, 0x5a, 0x07,
	0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_event_player_event_proto_rawDescOnce sync.Once
	file_proto_logic_event_player_event_proto_rawDescData = file_proto_logic_event_player_event_proto_rawDesc
)

func file_proto_logic_event_player_event_proto_rawDescGZIP() []byte {
	file_proto_logic_event_player_event_proto_rawDescOnce.Do(func() {
		file_proto_logic_event_player_event_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_event_player_event_proto_rawDescData)
	})
	return file_proto_logic_event_player_event_proto_rawDescData
}

var file_proto_logic_event_player_event_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_proto_logic_event_player_event_proto_goTypes = []any{
	(*RegisterPlayerEvent)(nil),             // 0: RegisterPlayerEvent
	(*PlayerUpgradeEvent)(nil),              // 1: PlayerUpgradeEvent
	(*InitializePlayerComponentsEvent)(nil), // 2: InitializePlayerComponentsEvent
}
var file_proto_logic_event_player_event_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_logic_event_player_event_proto_init() }
func file_proto_logic_event_player_event_proto_init() {
	if File_proto_logic_event_player_event_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_event_player_event_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*RegisterPlayerEvent); i {
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
		file_proto_logic_event_player_event_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*PlayerUpgradeEvent); i {
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
		file_proto_logic_event_player_event_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*InitializePlayerComponentsEvent); i {
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
			RawDescriptor: file_proto_logic_event_player_event_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_event_player_event_proto_goTypes,
		DependencyIndexes: file_proto_logic_event_player_event_proto_depIdxs,
		MessageInfos:      file_proto_logic_event_player_event_proto_msgTypes,
	}.Build()
	File_proto_logic_event_player_event_proto = out.File
	file_proto_logic_event_player_event_proto_rawDesc = nil
	file_proto_logic_event_player_event_proto_goTypes = nil
	file_proto_logic_event_player_event_proto_depIdxs = nil
}
