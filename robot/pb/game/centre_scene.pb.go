// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/server/centre_scene.proto

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

type RegisterSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SceneNodeId uint32                  `protobuf:"varint,1,opt,name=scene_node_id,json=sceneNodeId,proto3" json:"scene_node_id,omitempty"`
	ScenesInfo  []*SceneInfoPBComponent `protobuf:"bytes,2,rep,name=scenes_info,json=scenesInfo,proto3" json:"scenes_info,omitempty"`
}

func (x *RegisterSceneRequest) Reset() {
	*x = RegisterSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_server_centre_scene_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RegisterSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RegisterSceneRequest) ProtoMessage() {}

func (x *RegisterSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_server_centre_scene_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RegisterSceneRequest.ProtoReflect.Descriptor instead.
func (*RegisterSceneRequest) Descriptor() ([]byte, []int) {
	return file_proto_logic_server_centre_scene_proto_rawDescGZIP(), []int{0}
}

func (x *RegisterSceneRequest) GetSceneNodeId() uint32 {
	if x != nil {
		return x.SceneNodeId
	}
	return 0
}

func (x *RegisterSceneRequest) GetScenesInfo() []*SceneInfoPBComponent {
	if x != nil {
		return x.ScenesInfo
	}
	return nil
}

type RegisterSceneResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *RegisterSceneResponse) Reset() {
	*x = RegisterSceneResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_server_centre_scene_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RegisterSceneResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RegisterSceneResponse) ProtoMessage() {}

func (x *RegisterSceneResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_server_centre_scene_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RegisterSceneResponse.ProtoReflect.Descriptor instead.
func (*RegisterSceneResponse) Descriptor() ([]byte, []int) {
	return file_proto_logic_server_centre_scene_proto_rawDescGZIP(), []int{1}
}

type UnRegisterSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SceneNodeId uint32 `protobuf:"varint,1,opt,name=scene_node_id,json=sceneNodeId,proto3" json:"scene_node_id,omitempty"`
	Scene       uint32 `protobuf:"varint,2,opt,name=scene,proto3" json:"scene,omitempty"`
	Reason      uint32 `protobuf:"varint,3,opt,name=reason,proto3" json:"reason,omitempty"`
}

func (x *UnRegisterSceneRequest) Reset() {
	*x = UnRegisterSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_server_centre_scene_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *UnRegisterSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*UnRegisterSceneRequest) ProtoMessage() {}

func (x *UnRegisterSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_server_centre_scene_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use UnRegisterSceneRequest.ProtoReflect.Descriptor instead.
func (*UnRegisterSceneRequest) Descriptor() ([]byte, []int) {
	return file_proto_logic_server_centre_scene_proto_rawDescGZIP(), []int{2}
}

func (x *UnRegisterSceneRequest) GetSceneNodeId() uint32 {
	if x != nil {
		return x.SceneNodeId
	}
	return 0
}

func (x *UnRegisterSceneRequest) GetScene() uint32 {
	if x != nil {
		return x.Scene
	}
	return 0
}

func (x *UnRegisterSceneRequest) GetReason() uint32 {
	if x != nil {
		return x.Reason
	}
	return 0
}

var File_proto_logic_server_centre_scene_proto protoreflect.FileDescriptor

var file_proto_logic_server_centre_scene_proto_rawDesc = []byte{
	0x0a, 0x25, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x73, 0x65,
	0x72, 0x76, 0x65, 0x72, 0x2f, 0x63, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x5f, 0x73, 0x63, 0x65, 0x6e,
	0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x18, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63,
	0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x1a, 0x19, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f,
	0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x72, 0x0a, 0x14,
	0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71,
	0x75, 0x65, 0x73, 0x74, 0x12, 0x22, 0x0a, 0x0d, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x6e, 0x6f,
	0x64, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x73, 0x63, 0x65,
	0x6e, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x12, 0x36, 0x0a, 0x0b, 0x73, 0x63, 0x65, 0x6e,
	0x65, 0x73, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x15, 0x2e,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x49, 0x6e, 0x66, 0x6f, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x52, 0x0a, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x73, 0x49, 0x6e, 0x66, 0x6f,
	0x22, 0x17, 0x0a, 0x15, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x22, 0x6a, 0x0a, 0x16, 0x55, 0x6e, 0x52,
	0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x12, 0x22, 0x0a, 0x0d, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x6e, 0x6f, 0x64,
	0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x73, 0x63, 0x65, 0x6e,
	0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x12, 0x14, 0x0a, 0x05, 0x73, 0x63, 0x65, 0x6e, 0x65,
	0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x05, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x16, 0x0a,
	0x06, 0x72, 0x65, 0x61, 0x73, 0x6f, 0x6e, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x06, 0x72,
	0x65, 0x61, 0x73, 0x6f, 0x6e, 0x32, 0x88, 0x01, 0x0a, 0x12, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x3e, 0x0a, 0x0d,
	0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x15, 0x2e,
	0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71,
	0x75, 0x65, 0x73, 0x74, 0x1a, 0x16, 0x2e, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53,
	0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x32, 0x0a, 0x0f,
	0x55, 0x6e, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12,
	0x17, 0x2e, 0x55, 0x6e, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79,
	0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x80, 0x01, 0x01, 0x62, 0x06,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_server_centre_scene_proto_rawDescOnce sync.Once
	file_proto_logic_server_centre_scene_proto_rawDescData = file_proto_logic_server_centre_scene_proto_rawDesc
)

func file_proto_logic_server_centre_scene_proto_rawDescGZIP() []byte {
	file_proto_logic_server_centre_scene_proto_rawDescOnce.Do(func() {
		file_proto_logic_server_centre_scene_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_server_centre_scene_proto_rawDescData)
	})
	return file_proto_logic_server_centre_scene_proto_rawDescData
}

var file_proto_logic_server_centre_scene_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_proto_logic_server_centre_scene_proto_goTypes = []any{
	(*RegisterSceneRequest)(nil),   // 0: RegisterSceneRequest
	(*RegisterSceneResponse)(nil),  // 1: RegisterSceneResponse
	(*UnRegisterSceneRequest)(nil), // 2: UnRegisterSceneRequest
	(*SceneInfoPBComponent)(nil),   // 3: SceneInfoPBComponent
	(*Empty)(nil),                  // 4: Empty
}
var file_proto_logic_server_centre_scene_proto_depIdxs = []int32{
	3, // 0: RegisterSceneRequest.scenes_info:type_name -> SceneInfoPBComponent
	0, // 1: CentreSceneService.RegisterScene:input_type -> RegisterSceneRequest
	2, // 2: CentreSceneService.UnRegisterScene:input_type -> UnRegisterSceneRequest
	1, // 3: CentreSceneService.RegisterScene:output_type -> RegisterSceneResponse
	4, // 4: CentreSceneService.UnRegisterScene:output_type -> Empty
	3, // [3:5] is the sub-list for method output_type
	1, // [1:3] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_proto_logic_server_centre_scene_proto_init() }
func file_proto_logic_server_centre_scene_proto_init() {
	if File_proto_logic_server_centre_scene_proto != nil {
		return
	}
	file_proto_common_empty_proto_init()
	file_proto_common_common_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_server_centre_scene_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*RegisterSceneRequest); i {
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
		file_proto_logic_server_centre_scene_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*RegisterSceneResponse); i {
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
		file_proto_logic_server_centre_scene_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*UnRegisterSceneRequest); i {
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
			RawDescriptor: file_proto_logic_server_centre_scene_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_proto_logic_server_centre_scene_proto_goTypes,
		DependencyIndexes: file_proto_logic_server_centre_scene_proto_depIdxs,
		MessageInfos:      file_proto_logic_server_centre_scene_proto_msgTypes,
	}.Build()
	File_proto_logic_server_centre_scene_proto = out.File
	file_proto_logic_server_centre_scene_proto_rawDesc = nil
	file_proto_logic_server_centre_scene_proto_goTypes = nil
	file_proto_logic_server_centre_scene_proto_depIdxs = nil
}
