// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/server_player/game_scene_server_player.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	emptypb "google.golang.org/protobuf/types/known/emptypb"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type GsEnterSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SessionId uint64 `protobuf:"varint,1,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	SceneId   uint64 `protobuf:"varint,2,opt,name=scene_id,json=sceneId,proto3" json:"scene_id,omitempty"`
}

func (x *GsEnterSceneRequest) Reset() {
	*x = GsEnterSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GsEnterSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GsEnterSceneRequest) ProtoMessage() {}

func (x *GsEnterSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GsEnterSceneRequest.ProtoReflect.Descriptor instead.
func (*GsEnterSceneRequest) Descriptor() ([]byte, []int) {
	return file_logic_server_player_game_scene_server_player_proto_rawDescGZIP(), []int{0}
}

func (x *GsEnterSceneRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *GsEnterSceneRequest) GetSceneId() uint64 {
	if x != nil {
		return x.SceneId
	}
	return 0
}

type GsLeaveSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ChangeGs bool `protobuf:"varint,1,opt,name=change_gs,json=changeGs,proto3" json:"change_gs,omitempty"`
}

func (x *GsLeaveSceneRequest) Reset() {
	*x = GsLeaveSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GsLeaveSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GsLeaveSceneRequest) ProtoMessage() {}

func (x *GsLeaveSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GsLeaveSceneRequest.ProtoReflect.Descriptor instead.
func (*GsLeaveSceneRequest) Descriptor() ([]byte, []int) {
	return file_logic_server_player_game_scene_server_player_proto_rawDescGZIP(), []int{1}
}

func (x *GsLeaveSceneRequest) GetChangeGs() bool {
	if x != nil {
		return x.ChangeGs
	}
	return false
}

type EnterSceneS2CRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *EnterSceneS2CRequest) Reset() {
	*x = EnterSceneS2CRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterSceneS2CRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterSceneS2CRequest) ProtoMessage() {}

func (x *EnterSceneS2CRequest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterSceneS2CRequest.ProtoReflect.Descriptor instead.
func (*EnterSceneS2CRequest) Descriptor() ([]byte, []int) {
	return file_logic_server_player_game_scene_server_player_proto_rawDescGZIP(), []int{2}
}

type EnterScenerS2CResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
}

func (x *EnterScenerS2CResponse) Reset() {
	*x = EnterScenerS2CResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterScenerS2CResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterScenerS2CResponse) ProtoMessage() {}

func (x *EnterScenerS2CResponse) ProtoReflect() protoreflect.Message {
	mi := &file_logic_server_player_game_scene_server_player_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterScenerS2CResponse.ProtoReflect.Descriptor instead.
func (*EnterScenerS2CResponse) Descriptor() ([]byte, []int) {
	return file_logic_server_player_game_scene_server_player_proto_rawDescGZIP(), []int{3}
}

func (x *EnterScenerS2CResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

var File_logic_server_player_game_scene_server_player_proto protoreflect.FileDescriptor

var file_logic_server_player_game_scene_server_player_proto_rawDesc = []byte{
	0x0a, 0x32, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x5f, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x5f, 0x73, 0x63, 0x65, 0x6e, 0x65,
	0x5f, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x5f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x10, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x74, 0x69, 0x70,
	0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x1b, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2f, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x62, 0x75, 0x66, 0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x4f, 0x0a, 0x13, 0x47, 0x73, 0x45, 0x6e, 0x74, 0x65, 0x72,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1d, 0x0a, 0x0a,
	0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04,
	0x52, 0x09, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x12, 0x19, 0x0a, 0x08, 0x73,
	0x63, 0x65, 0x6e, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x07, 0x73,
	0x63, 0x65, 0x6e, 0x65, 0x49, 0x64, 0x22, 0x32, 0x0a, 0x13, 0x47, 0x73, 0x4c, 0x65, 0x61, 0x76,
	0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a,
	0x09, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x5f, 0x67, 0x73, 0x18, 0x01, 0x20, 0x01, 0x28, 0x08,
	0x52, 0x08, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x47, 0x73, 0x22, 0x16, 0x0a, 0x14, 0x45, 0x6e,
	0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x53, 0x32, 0x43, 0x52, 0x65, 0x71, 0x75, 0x65,
	0x73, 0x74, 0x22, 0x4e, 0x0a, 0x16, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x72, 0x53, 0x32, 0x43, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a, 0x0d,
	0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x54, 0x69, 0x70, 0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65, 0x73,
	0x73, 0x61, 0x67, 0x65, 0x52, 0x0c, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x32, 0xd1, 0x01, 0x0a, 0x16, 0x47, 0x61, 0x6d, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x3a, 0x0a,
	0x0a, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x14, 0x2e, 0x47, 0x73,
	0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73,
	0x74, 0x1a, 0x16, 0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x62, 0x75, 0x66, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3a, 0x0a, 0x0a, 0x4c, 0x65, 0x61,
	0x76, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x14, 0x2e, 0x47, 0x73, 0x4c, 0x65, 0x61, 0x76,
	0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x16, 0x2e,
	0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x62, 0x75, 0x66, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3f, 0x0a, 0x0d, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x53, 0x32, 0x43, 0x12, 0x15, 0x2e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x53, 0x32, 0x43, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x17, 0x2e,
	0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x72, 0x53, 0x32, 0x43, 0x52, 0x65,
	0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d,
	0x65, 0x80, 0x01, 0x01, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_server_player_game_scene_server_player_proto_rawDescOnce sync.Once
	file_logic_server_player_game_scene_server_player_proto_rawDescData = file_logic_server_player_game_scene_server_player_proto_rawDesc
)

func file_logic_server_player_game_scene_server_player_proto_rawDescGZIP() []byte {
	file_logic_server_player_game_scene_server_player_proto_rawDescOnce.Do(func() {
		file_logic_server_player_game_scene_server_player_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_server_player_game_scene_server_player_proto_rawDescData)
	})
	return file_logic_server_player_game_scene_server_player_proto_rawDescData
}

var file_logic_server_player_game_scene_server_player_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_logic_server_player_game_scene_server_player_proto_goTypes = []any{
	(*GsEnterSceneRequest)(nil),    // 0: GsEnterSceneRequest
	(*GsLeaveSceneRequest)(nil),    // 1: GsLeaveSceneRequest
	(*EnterSceneS2CRequest)(nil),   // 2: EnterSceneS2CRequest
	(*EnterScenerS2CResponse)(nil), // 3: EnterScenerS2CResponse
	(*TipInfoMessage)(nil),         // 4: TipInfoMessage
	(*emptypb.Empty)(nil),          // 5: google.protobuf.Empty
}
var file_logic_server_player_game_scene_server_player_proto_depIdxs = []int32{
	4, // 0: EnterScenerS2CResponse.error_message:type_name -> TipInfoMessage
	0, // 1: GamePlayerSceneService.EnterScene:input_type -> GsEnterSceneRequest
	1, // 2: GamePlayerSceneService.LeaveScene:input_type -> GsLeaveSceneRequest
	2, // 3: GamePlayerSceneService.EnterSceneS2C:input_type -> EnterSceneS2CRequest
	5, // 4: GamePlayerSceneService.EnterScene:output_type -> google.protobuf.Empty
	5, // 5: GamePlayerSceneService.LeaveScene:output_type -> google.protobuf.Empty
	3, // 6: GamePlayerSceneService.EnterSceneS2C:output_type -> EnterScenerS2CResponse
	4, // [4:7] is the sub-list for method output_type
	1, // [1:4] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_logic_server_player_game_scene_server_player_proto_init() }
func file_logic_server_player_game_scene_server_player_proto_init() {
	if File_logic_server_player_game_scene_server_player_proto != nil {
		return
	}
	file_common_tip_proto_init()
	file_logic_component_scene_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_logic_server_player_game_scene_server_player_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*GsEnterSceneRequest); i {
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
		file_logic_server_player_game_scene_server_player_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*GsLeaveSceneRequest); i {
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
		file_logic_server_player_game_scene_server_player_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*EnterSceneS2CRequest); i {
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
		file_logic_server_player_game_scene_server_player_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*EnterScenerS2CResponse); i {
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
			RawDescriptor: file_logic_server_player_game_scene_server_player_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_logic_server_player_game_scene_server_player_proto_goTypes,
		DependencyIndexes: file_logic_server_player_game_scene_server_player_proto_depIdxs,
		MessageInfos:      file_logic_server_player_game_scene_server_player_proto_msgTypes,
	}.Build()
	File_logic_server_player_game_scene_server_player_proto = out.File
	file_logic_server_player_game_scene_server_player_proto_rawDesc = nil
	file_logic_server_player_game_scene_server_player_proto_goTypes = nil
	file_logic_server_player_game_scene_server_player_proto_depIdxs = nil
}
