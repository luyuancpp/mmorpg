// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/component/player_network_comp.proto

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

type PlayerNodeInfoPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	GateSessionId uint64 `protobuf:"varint,1,opt,name=gate_session_id,json=gateSessionId,proto3" json:"gate_session_id,omitempty"` //通过session id 就能算出 gate_node_id
	CentreNodeId  uint32 `protobuf:"varint,2,opt,name=centre_node_id,json=centreNodeId,proto3" json:"centre_node_id,omitempty"`
	GameNodeId    uint32 `protobuf:"varint,3,opt,name=game_node_id,json=gameNodeId,proto3" json:"game_node_id,omitempty"`
}

func (x *PlayerNodeInfoPBComponent) Reset() {
	*x = PlayerNodeInfoPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_player_network_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerNodeInfoPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerNodeInfoPBComponent) ProtoMessage() {}

func (x *PlayerNodeInfoPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_player_network_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerNodeInfoPBComponent.ProtoReflect.Descriptor instead.
func (*PlayerNodeInfoPBComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_player_network_comp_proto_rawDescGZIP(), []int{0}
}

func (x *PlayerNodeInfoPBComponent) GetGateSessionId() uint64 {
	if x != nil {
		return x.GateSessionId
	}
	return 0
}

func (x *PlayerNodeInfoPBComponent) GetCentreNodeId() uint32 {
	if x != nil {
		return x.CentreNodeId
	}
	return 0
}

func (x *PlayerNodeInfoPBComponent) GetGameNodeId() uint32 {
	if x != nil {
		return x.GameNodeId
	}
	return 0
}

type PlayerSessionPBComp struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *PlayerSessionPBComp) Reset() {
	*x = PlayerSessionPBComp{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_player_network_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerSessionPBComp) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerSessionPBComp) ProtoMessage() {}

func (x *PlayerSessionPBComp) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_player_network_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerSessionPBComp.ProtoReflect.Descriptor instead.
func (*PlayerSessionPBComp) Descriptor() ([]byte, []int) {
	return file_logic_component_player_network_comp_proto_rawDescGZIP(), []int{1}
}

func (x *PlayerSessionPBComp) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

var File_logic_component_player_network_comp_proto protoreflect.FileDescriptor

var file_logic_component_player_network_comp_proto_rawDesc = []byte{
	0x0a, 0x29, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b,
	0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x8b, 0x01, 0x0a, 0x19,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x6e, 0x66, 0x6f, 0x50, 0x42,
	0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x26, 0x0a, 0x0f, 0x67, 0x61, 0x74,
	0x65, 0x5f, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x0d, 0x67, 0x61, 0x74, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49,
	0x64, 0x12, 0x24, 0x0a, 0x0e, 0x63, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x5f, 0x6e, 0x6f, 0x64, 0x65,
	0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x63, 0x65, 0x6e, 0x74, 0x72,
	0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x12, 0x20, 0x0a, 0x0c, 0x67, 0x61, 0x6d, 0x65, 0x5f,
	0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x67,
	0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x22, 0x32, 0x0a, 0x13, 0x50, 0x6c, 0x61,
	0x79, 0x65, 0x72, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70,
	0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x42, 0x09, 0x5a,
	0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_component_player_network_comp_proto_rawDescOnce sync.Once
	file_logic_component_player_network_comp_proto_rawDescData = file_logic_component_player_network_comp_proto_rawDesc
)

func file_logic_component_player_network_comp_proto_rawDescGZIP() []byte {
	file_logic_component_player_network_comp_proto_rawDescOnce.Do(func() {
		file_logic_component_player_network_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_component_player_network_comp_proto_rawDescData)
	})
	return file_logic_component_player_network_comp_proto_rawDescData
}

var file_logic_component_player_network_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_logic_component_player_network_comp_proto_goTypes = []any{
	(*PlayerNodeInfoPBComponent)(nil), // 0: PlayerNodeInfoPBComponent
	(*PlayerSessionPBComp)(nil),       // 1: PlayerSessionPBComp
}
var file_logic_component_player_network_comp_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_logic_component_player_network_comp_proto_init() }
func file_logic_component_player_network_comp_proto_init() {
	if File_logic_component_player_network_comp_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_logic_component_player_network_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*PlayerNodeInfoPBComponent); i {
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
		file_logic_component_player_network_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*PlayerSessionPBComp); i {
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
			RawDescriptor: file_logic_component_player_network_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_logic_component_player_network_comp_proto_goTypes,
		DependencyIndexes: file_logic_component_player_network_comp_proto_depIdxs,
		MessageInfos:      file_logic_component_player_network_comp_proto_msgTypes,
	}.Build()
	File_logic_component_player_network_comp_proto = out.File
	file_logic_component_player_network_comp_proto_rawDesc = nil
	file_logic_component_player_network_comp_proto_goTypes = nil
	file_logic_component_player_network_comp_proto_depIdxs = nil
}
