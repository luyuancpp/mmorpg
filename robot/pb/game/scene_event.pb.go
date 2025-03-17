// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v6.31.0--dev
// source: proto/logic/event/scene_event.proto

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

type OnSceneCreate struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *OnSceneCreate) Reset() {
	*x = OnSceneCreate{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *OnSceneCreate) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*OnSceneCreate) ProtoMessage() {}

func (x *OnSceneCreate) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use OnSceneCreate.ProtoReflect.Descriptor instead.
func (*OnSceneCreate) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{0}
}

func (x *OnSceneCreate) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type OnDestroyScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *OnDestroyScene) Reset() {
	*x = OnDestroyScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *OnDestroyScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*OnDestroyScene) ProtoMessage() {}

func (x *OnDestroyScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use OnDestroyScene.ProtoReflect.Descriptor instead.
func (*OnDestroyScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{1}
}

func (x *OnDestroyScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type BeforeEnterScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *BeforeEnterScene) Reset() {
	*x = BeforeEnterScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BeforeEnterScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BeforeEnterScene) ProtoMessage() {}

func (x *BeforeEnterScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BeforeEnterScene.ProtoReflect.Descriptor instead.
func (*BeforeEnterScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{2}
}

func (x *BeforeEnterScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type AfterEnterScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *AfterEnterScene) Reset() {
	*x = AfterEnterScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AfterEnterScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AfterEnterScene) ProtoMessage() {}

func (x *AfterEnterScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AfterEnterScene.ProtoReflect.Descriptor instead.
func (*AfterEnterScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{3}
}

func (x *AfterEnterScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type BeforeLeaveScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *BeforeLeaveScene) Reset() {
	*x = BeforeLeaveScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BeforeLeaveScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BeforeLeaveScene) ProtoMessage() {}

func (x *BeforeLeaveScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BeforeLeaveScene.ProtoReflect.Descriptor instead.
func (*BeforeLeaveScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{4}
}

func (x *BeforeLeaveScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type AfterLeaveScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *AfterLeaveScene) Reset() {
	*x = AfterLeaveScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AfterLeaveScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AfterLeaveScene) ProtoMessage() {}

func (x *AfterLeaveScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AfterLeaveScene.ProtoReflect.Descriptor instead.
func (*AfterLeaveScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{5}
}

func (x *AfterLeaveScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

type S2CEnterScene struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
}

func (x *S2CEnterScene) Reset() {
	*x = S2CEnterScene{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_event_scene_event_proto_msgTypes[6]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *S2CEnterScene) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*S2CEnterScene) ProtoMessage() {}

func (x *S2CEnterScene) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_event_scene_event_proto_msgTypes[6]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use S2CEnterScene.ProtoReflect.Descriptor instead.
func (*S2CEnterScene) Descriptor() ([]byte, []int) {
	return file_proto_logic_event_scene_event_proto_rawDescGZIP(), []int{6}
}

func (x *S2CEnterScene) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

var File_proto_logic_event_scene_event_proto protoreflect.FileDescriptor

var file_proto_logic_event_scene_event_proto_rawDesc = []byte{
	0x0a, 0x23, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x65, 0x76,
	0x65, 0x6e, 0x74, 0x2f, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x27, 0x0a, 0x0d, 0x4f, 0x6e, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x22, 0x28,
	0x0a, 0x0e, 0x4f, 0x6e, 0x44, 0x65, 0x73, 0x74, 0x72, 0x6f, 0x79, 0x53, 0x63, 0x65, 0x6e, 0x65,
	0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04,
	0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x22, 0x2a, 0x0a, 0x10, 0x42, 0x65, 0x66, 0x6f,
	0x72, 0x65, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x16, 0x0a, 0x06,
	0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e,
	0x74, 0x69, 0x74, 0x79, 0x22, 0x29, 0x0a, 0x0f, 0x41, 0x66, 0x74, 0x65, 0x72, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74,
	0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x22,
	0x2a, 0x0a, 0x10, 0x42, 0x65, 0x66, 0x6f, 0x72, 0x65, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x22, 0x29, 0x0a, 0x0f, 0x41,
	0x66, 0x74, 0x65, 0x72, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x16,
	0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06,
	0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x22, 0x27, 0x0a, 0x0d, 0x53, 0x32, 0x43, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74,
	0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x42,
	0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x33,
}

var (
	file_proto_logic_event_scene_event_proto_rawDescOnce sync.Once
	file_proto_logic_event_scene_event_proto_rawDescData = file_proto_logic_event_scene_event_proto_rawDesc
)

func file_proto_logic_event_scene_event_proto_rawDescGZIP() []byte {
	file_proto_logic_event_scene_event_proto_rawDescOnce.Do(func() {
		file_proto_logic_event_scene_event_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_event_scene_event_proto_rawDescData)
	})
	return file_proto_logic_event_scene_event_proto_rawDescData
}

var file_proto_logic_event_scene_event_proto_msgTypes = make([]protoimpl.MessageInfo, 7)
var file_proto_logic_event_scene_event_proto_goTypes = []any{
	(*OnSceneCreate)(nil),    // 0: OnSceneCreate
	(*OnDestroyScene)(nil),   // 1: OnDestroyScene
	(*BeforeEnterScene)(nil), // 2: BeforeEnterScene
	(*AfterEnterScene)(nil),  // 3: AfterEnterScene
	(*BeforeLeaveScene)(nil), // 4: BeforeLeaveScene
	(*AfterLeaveScene)(nil),  // 5: AfterLeaveScene
	(*S2CEnterScene)(nil),    // 6: S2CEnterScene
}
var file_proto_logic_event_scene_event_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_logic_event_scene_event_proto_init() }
func file_proto_logic_event_scene_event_proto_init() {
	if File_proto_logic_event_scene_event_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_event_scene_event_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*OnSceneCreate); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*OnDestroyScene); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*BeforeEnterScene); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*AfterEnterScene); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*BeforeLeaveScene); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*AfterLeaveScene); i {
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
		file_proto_logic_event_scene_event_proto_msgTypes[6].Exporter = func(v any, i int) any {
			switch v := v.(*S2CEnterScene); i {
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
			RawDescriptor: file_proto_logic_event_scene_event_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   7,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_event_scene_event_proto_goTypes,
		DependencyIndexes: file_proto_logic_event_scene_event_proto_depIdxs,
		MessageInfos:      file_proto_logic_event_scene_event_proto_msgTypes,
	}.Build()
	File_proto_logic_event_scene_event_proto = out.File
	file_proto_logic_event_scene_event_proto_rawDesc = nil
	file_proto_logic_event_scene_event_proto_goTypes = nil
	file_proto_logic_event_scene_event_proto_depIdxs = nil
}
