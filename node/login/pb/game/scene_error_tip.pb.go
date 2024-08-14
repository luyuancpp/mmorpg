// Proto file for scene_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: scene_error_tip.proto

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

type SceneError int32

const (
	SceneError_kScene_errorOK                         SceneError = 0
	SceneError_kEnterSceneNotFound                    SceneError = 26
	SceneError_kEnterSceneNotFull                     SceneError = 27
	SceneError_kEnterSceneMainFull                    SceneError = 28
	SceneError_kEnterGameNodeCrash                    SceneError = 29
	SceneError_kEnterSceneServerType                  SceneError = 30
	SceneError_kEnterSceneParamError                  SceneError = 31
	SceneError_kEnterSceneSceneFull                   SceneError = 32
	SceneError_kEnterSceneSceneNotFound               SceneError = 33
	SceneError_kEnterSceneYouInCurrentScene           SceneError = 34
	SceneError_kEnterSceneEnterCrossRoomScene         SceneError = 35
	SceneError_kEnterSceneGsInfoNull                  SceneError = 36
	SceneError_kEnterSceneGsFull                      SceneError = 37
	SceneError_kEnterSceneYourSceneIsNull             SceneError = 38
	SceneError_kEnterSceneChangeSceneOffLine          SceneError = 39
	SceneError_kEnterSceneChangingScene               SceneError = 40
	SceneError_kEnterSceneChangingGs                  SceneError = 41
	SceneError_kChangeScenePlayerQueueNotFound        SceneError = 42
	SceneError_kChangeScenePlayerQueueFull            SceneError = 43
	SceneError_kChangeScenePlayerQueueComponentGsNull SceneError = 44
	SceneError_kChangeScenePlayerQueueComponentEmpty  SceneError = 45
	SceneError_kChangeSceneEnQueueNotSameGs           SceneError = 46
	SceneError_kInvalidEnterSceneParameters           SceneError = 47
	SceneError_kCheckEnterSceneCreator                SceneError = 48
)

// Enum value maps for SceneError.
var (
	SceneError_name = map[int32]string{
		0:  "kScene_errorOK",
		26: "kEnterSceneNotFound",
		27: "kEnterSceneNotFull",
		28: "kEnterSceneMainFull",
		29: "kEnterGameNodeCrash",
		30: "kEnterSceneServerType",
		31: "kEnterSceneParamError",
		32: "kEnterSceneSceneFull",
		33: "kEnterSceneSceneNotFound",
		34: "kEnterSceneYouInCurrentScene",
		35: "kEnterSceneEnterCrossRoomScene",
		36: "kEnterSceneGsInfoNull",
		37: "kEnterSceneGsFull",
		38: "kEnterSceneYourSceneIsNull",
		39: "kEnterSceneChangeSceneOffLine",
		40: "kEnterSceneChangingScene",
		41: "kEnterSceneChangingGs",
		42: "kChangeScenePlayerQueueNotFound",
		43: "kChangeScenePlayerQueueFull",
		44: "kChangeScenePlayerQueueComponentGsNull",
		45: "kChangeScenePlayerQueueComponentEmpty",
		46: "kChangeSceneEnQueueNotSameGs",
		47: "kInvalidEnterSceneParameters",
		48: "kCheckEnterSceneCreator",
	}
	SceneError_value = map[string]int32{
		"kScene_errorOK":                         0,
		"kEnterSceneNotFound":                    26,
		"kEnterSceneNotFull":                     27,
		"kEnterSceneMainFull":                    28,
		"kEnterGameNodeCrash":                    29,
		"kEnterSceneServerType":                  30,
		"kEnterSceneParamError":                  31,
		"kEnterSceneSceneFull":                   32,
		"kEnterSceneSceneNotFound":               33,
		"kEnterSceneYouInCurrentScene":           34,
		"kEnterSceneEnterCrossRoomScene":         35,
		"kEnterSceneGsInfoNull":                  36,
		"kEnterSceneGsFull":                      37,
		"kEnterSceneYourSceneIsNull":             38,
		"kEnterSceneChangeSceneOffLine":          39,
		"kEnterSceneChangingScene":               40,
		"kEnterSceneChangingGs":                  41,
		"kChangeScenePlayerQueueNotFound":        42,
		"kChangeScenePlayerQueueFull":            43,
		"kChangeScenePlayerQueueComponentGsNull": 44,
		"kChangeScenePlayerQueueComponentEmpty":  45,
		"kChangeSceneEnQueueNotSameGs":           46,
		"kInvalidEnterSceneParameters":           47,
		"kCheckEnterSceneCreator":                48,
	}
)

func (x SceneError) Enum() *SceneError {
	p := new(SceneError)
	*p = x
	return p
}

func (x SceneError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (SceneError) Descriptor() protoreflect.EnumDescriptor {
	return file_scene_error_tip_proto_enumTypes[0].Descriptor()
}

func (SceneError) Type() protoreflect.EnumType {
	return &file_scene_error_tip_proto_enumTypes[0]
}

func (x SceneError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use SceneError.Descriptor instead.
func (SceneError) EnumDescriptor() ([]byte, []int) {
	return file_scene_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_scene_error_tip_proto protoreflect.FileDescriptor

var file_scene_error_tip_proto_rawDesc = []byte{
	0x0a, 0x15, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xe4, 0x05, 0x0a, 0x0b, 0x73, 0x63, 0x65, 0x6e,
	0x65, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x17, 0x0a, 0x13, 0x6b,
	0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75,
	0x6e, 0x64, 0x10, 0x1a, 0x12, 0x16, 0x0a, 0x12, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x4e, 0x6f, 0x74, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x1b, 0x12, 0x17, 0x0a, 0x13,
	0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x4d, 0x61, 0x69, 0x6e, 0x46,
	0x75, 0x6c, 0x6c, 0x10, 0x1c, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47,
	0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x43, 0x72, 0x61, 0x73, 0x68, 0x10, 0x1d, 0x12, 0x19,
	0x0a, 0x15, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x53, 0x65, 0x72,
	0x76, 0x65, 0x72, 0x54, 0x79, 0x70, 0x65, 0x10, 0x1e, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x45, 0x6e,
	0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x50, 0x61, 0x72, 0x61, 0x6d, 0x45, 0x72, 0x72,
	0x6f, 0x72, 0x10, 0x1f, 0x12, 0x18, 0x0a, 0x14, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x20, 0x12, 0x1c,
	0x0a, 0x18, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x53, 0x63, 0x65,
	0x6e, 0x65, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64, 0x10, 0x21, 0x12, 0x20, 0x0a, 0x1c,
	0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x59, 0x6f, 0x75, 0x49, 0x6e,
	0x43, 0x75, 0x72, 0x72, 0x65, 0x6e, 0x74, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x10, 0x22, 0x12, 0x22,
	0x0a, 0x1e, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x43, 0x72, 0x6f, 0x73, 0x73, 0x52, 0x6f, 0x6f, 0x6d, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x10, 0x23, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x47, 0x73, 0x49, 0x6e, 0x66, 0x6f, 0x4e, 0x75, 0x6c, 0x6c, 0x10, 0x24, 0x12, 0x15, 0x0a,
	0x11, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x47, 0x73, 0x46, 0x75,
	0x6c, 0x6c, 0x10, 0x25, 0x12, 0x1e, 0x0a, 0x1a, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x59, 0x6f, 0x75, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x49, 0x73, 0x4e, 0x75,
	0x6c, 0x6c, 0x10, 0x26, 0x12, 0x21, 0x0a, 0x1d, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x4f, 0x66,
	0x66, 0x4c, 0x69, 0x6e, 0x65, 0x10, 0x27, 0x12, 0x1c, 0x0a, 0x18, 0x6b, 0x45, 0x6e, 0x74, 0x65,
	0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x69, 0x6e, 0x67, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x10, 0x28, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53,
	0x63, 0x65, 0x6e, 0x65, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x69, 0x6e, 0x67, 0x47, 0x73, 0x10, 0x29,
	0x12, 0x23, 0x0a, 0x1f, 0x6b, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x51, 0x75, 0x65, 0x75, 0x65, 0x4e, 0x6f, 0x74, 0x46, 0x6f,
	0x75, 0x6e, 0x64, 0x10, 0x2a, 0x12, 0x1f, 0x0a, 0x1b, 0x6b, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x51, 0x75, 0x65, 0x75, 0x65,
	0x46, 0x75, 0x6c, 0x6c, 0x10, 0x2b, 0x12, 0x2a, 0x0a, 0x26, 0x6b, 0x43, 0x68, 0x61, 0x6e, 0x67,
	0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x51, 0x75, 0x65, 0x75,
	0x65, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x47, 0x73, 0x4e, 0x75, 0x6c, 0x6c,
	0x10, 0x2c, 0x12, 0x29, 0x0a, 0x25, 0x6b, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x53, 0x63, 0x65,
	0x6e, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x51, 0x75, 0x65, 0x75, 0x65, 0x43, 0x6f, 0x6d,
	0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x10, 0x2d, 0x12, 0x20, 0x0a,
	0x1c, 0x6b, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x45, 0x6e, 0x51,
	0x75, 0x65, 0x75, 0x65, 0x4e, 0x6f, 0x74, 0x53, 0x61, 0x6d, 0x65, 0x47, 0x73, 0x10, 0x2e, 0x12,
	0x20, 0x0a, 0x1c, 0x6b, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x45, 0x6e, 0x74, 0x65, 0x72,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x50, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x10,
	0x2f, 0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x43, 0x68, 0x65, 0x63, 0x6b, 0x45, 0x6e, 0x74, 0x65, 0x72,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x43, 0x72, 0x65, 0x61, 0x74, 0x6f, 0x72, 0x10, 0x30, 0x42, 0x09,
	0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x33,
}

var (
	file_scene_error_tip_proto_rawDescOnce sync.Once
	file_scene_error_tip_proto_rawDescData = file_scene_error_tip_proto_rawDesc
)

func file_scene_error_tip_proto_rawDescGZIP() []byte {
	file_scene_error_tip_proto_rawDescOnce.Do(func() {
		file_scene_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_scene_error_tip_proto_rawDescData)
	})
	return file_scene_error_tip_proto_rawDescData
}

var file_scene_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_scene_error_tip_proto_goTypes = []any{
	(SceneError)(0), // 0: scene_error
}
var file_scene_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_scene_error_tip_proto_init() }
func file_scene_error_tip_proto_init() {
	if File_scene_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_scene_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_scene_error_tip_proto_goTypes,
		DependencyIndexes: file_scene_error_tip_proto_depIdxs,
		EnumInfos:         file_scene_error_tip_proto_enumTypes,
	}.Build()
	File_scene_error_tip_proto = out.File
	file_scene_error_tip_proto_rawDesc = nil
	file_scene_error_tip_proto_goTypes = nil
	file_scene_error_tip_proto_depIdxs = nil
}