// Proto file for scene

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: scene_operator.proto

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

type Scene int32

const (
	Scene_kSceneOK       Scene = 0
	Scene_kEnterSceneC2S Scene = 0
)

// Enum value maps for Scene.
var (
	Scene_name = map[int32]string{
		0: "kSceneOK",
		// Duplicate value: 0: "kEnterSceneC2S",
	}
	Scene_value = map[string]int32{
		"kSceneOK":       0,
		"kEnterSceneC2S": 0,
	}
)

func (x Scene) Enum() *Scene {
	p := new(Scene)
	*p = x
	return p
}

func (x Scene) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (Scene) Descriptor() protoreflect.EnumDescriptor {
	return file_scene_operator_proto_enumTypes[0].Descriptor()
}

func (Scene) Type() protoreflect.EnumType {
	return &file_scene_operator_proto_enumTypes[0]
}

func (x Scene) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use Scene.Descriptor instead.
func (Scene) EnumDescriptor() ([]byte, []int) {
	return file_scene_operator_proto_rawDescGZIP(), []int{0}
}

var File_scene_operator_proto protoreflect.FileDescriptor

var file_scene_operator_proto_rawDesc = []byte{
	0x0a, 0x14, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x6f, 0x70, 0x65, 0x72, 0x61, 0x74, 0x6f, 0x72,
	0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0x2d, 0x0a, 0x05, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x12,
	0x0c, 0x0a, 0x08, 0x6b, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x12, 0x0a,
	0x0e, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x43, 0x32, 0x53, 0x10,
	0x00, 0x1a, 0x02, 0x10, 0x01, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_scene_operator_proto_rawDescOnce sync.Once
	file_scene_operator_proto_rawDescData = file_scene_operator_proto_rawDesc
)

func file_scene_operator_proto_rawDescGZIP() []byte {
	file_scene_operator_proto_rawDescOnce.Do(func() {
		file_scene_operator_proto_rawDescData = protoimpl.X.CompressGZIP(file_scene_operator_proto_rawDescData)
	})
	return file_scene_operator_proto_rawDescData
}

var file_scene_operator_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_scene_operator_proto_goTypes = []any{
	(Scene)(0), // 0: scene
}
var file_scene_operator_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_scene_operator_proto_init() }
func file_scene_operator_proto_init() {
	if File_scene_operator_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_scene_operator_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_scene_operator_proto_goTypes,
		DependencyIndexes: file_scene_operator_proto_depIdxs,
		EnumInfos:         file_scene_operator_proto_enumTypes,
	}.Build()
	File_scene_operator_proto = out.File
	file_scene_operator_proto_rawDesc = nil
	file_scene_operator_proto_goTypes = nil
	file_scene_operator_proto_depIdxs = nil
}
