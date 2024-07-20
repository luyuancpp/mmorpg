// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.1
// 	protoc        v4.25.1
// source: logic_proto/server_proto/game_scene.proto

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

type GameSceneTest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *GameSceneTest) Reset() {
	*x = GameSceneTest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_proto_server_proto_game_scene_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameSceneTest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameSceneTest) ProtoMessage() {}

func (x *GameSceneTest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_proto_server_proto_game_scene_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameSceneTest.ProtoReflect.Descriptor instead.
func (*GameSceneTest) Descriptor() ([]byte, []int) {
	return file_logic_proto_server_proto_game_scene_proto_rawDescGZIP(), []int{0}
}

var File_logic_proto_server_proto_game_scene_proto protoreflect.FileDescriptor

var file_logic_proto_server_proto_game_scene_proto_rawDesc = []byte{
	0x0a, 0x29, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x73, 0x65,
	0x72, 0x76, 0x65, 0x72, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x5f,
	0x73, 0x63, 0x65, 0x6e, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x18, 0x63, 0x6f, 0x6d,
	0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x0f, 0x0a, 0x0d, 0x47, 0x61, 0x6d, 0x65, 0x53, 0x63, 0x65,
	0x6e, 0x65, 0x54, 0x65, 0x73, 0x74, 0x32, 0x32, 0x0a, 0x10, 0x47, 0x61, 0x6d, 0x65, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x1e, 0x0a, 0x04, 0x54, 0x65,
	0x73, 0x74, 0x12, 0x0e, 0x2e, 0x47, 0x61, 0x6d, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x54, 0x65,
	0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62,
	0x2f, 0x67, 0x61, 0x6d, 0x65, 0x80, 0x01, 0x01, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_proto_server_proto_game_scene_proto_rawDescOnce sync.Once
	file_logic_proto_server_proto_game_scene_proto_rawDescData = file_logic_proto_server_proto_game_scene_proto_rawDesc
)

func file_logic_proto_server_proto_game_scene_proto_rawDescGZIP() []byte {
	file_logic_proto_server_proto_game_scene_proto_rawDescOnce.Do(func() {
		file_logic_proto_server_proto_game_scene_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_proto_server_proto_game_scene_proto_rawDescData)
	})
	return file_logic_proto_server_proto_game_scene_proto_rawDescData
}

var file_logic_proto_server_proto_game_scene_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_logic_proto_server_proto_game_scene_proto_goTypes = []interface{}{
	(*GameSceneTest)(nil), // 0: GameSceneTest
	(*Empty)(nil),         // 1: Empty
}
var file_logic_proto_server_proto_game_scene_proto_depIdxs = []int32{
	0, // 0: GameSceneService.Test:input_type -> GameSceneTest
	1, // 1: GameSceneService.Test:output_type -> Empty
	1, // [1:2] is the sub-list for method output_type
	0, // [0:1] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_logic_proto_server_proto_game_scene_proto_init() }
func file_logic_proto_server_proto_game_scene_proto_init() {
	if File_logic_proto_server_proto_game_scene_proto != nil {
		return
	}
	file_common_proto_empty_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_logic_proto_server_proto_game_scene_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*GameSceneTest); i {
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
			RawDescriptor: file_logic_proto_server_proto_game_scene_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_logic_proto_server_proto_game_scene_proto_goTypes,
		DependencyIndexes: file_logic_proto_server_proto_game_scene_proto_depIdxs,
		MessageInfos:      file_logic_proto_server_proto_game_scene_proto_msgTypes,
	}.Build()
	File_logic_proto_server_proto_game_scene_proto = out.File
	file_logic_proto_server_proto_game_scene_proto_rawDesc = nil
	file_logic_proto_server_proto_game_scene_proto_goTypes = nil
	file_logic_proto_server_proto_game_scene_proto_depIdxs = nil
}
