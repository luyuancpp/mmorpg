// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/component/player_login_comp.proto

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

type EnterGsType int32

const (
	EnterGsType_LOGIN_NONE      EnterGsType = 0 //正常进入，换场景进入
	EnterGsType_LOGIN_FIRST     EnterGsType = 1 //第一次登录
	EnterGsType_LOGIN_REPLACE   EnterGsType = 2 //顶号
	EnterGsType_LOGIN_RECONNECT EnterGsType = 3 //断线重连
)

// Enum value maps for EnterGsType.
var (
	EnterGsType_name = map[int32]string{
		0: "LOGIN_NONE",
		1: "LOGIN_FIRST",
		2: "LOGIN_REPLACE",
		3: "LOGIN_RECONNECT",
	}
	EnterGsType_value = map[string]int32{
		"LOGIN_NONE":      0,
		"LOGIN_FIRST":     1,
		"LOGIN_REPLACE":   2,
		"LOGIN_RECONNECT": 3,
	}
)

func (x EnterGsType) Enum() *EnterGsType {
	p := new(EnterGsType)
	*p = x
	return p
}

func (x EnterGsType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (EnterGsType) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_logic_component_player_login_comp_proto_enumTypes[0].Descriptor()
}

func (EnterGsType) Type() protoreflect.EnumType {
	return &file_proto_logic_component_player_login_comp_proto_enumTypes[0]
}

func (x EnterGsType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use EnterGsType.Descriptor instead.
func (EnterGsType) EnumDescriptor() ([]byte, []int) {
	return file_proto_logic_component_player_login_comp_proto_rawDescGZIP(), []int{0}
}

type EnterGameNodeInfoPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EnterGsType uint32 `protobuf:"varint,1,opt,name=enter_gs_type,json=enterGsType,proto3" json:"enter_gs_type,omitempty"`
}

func (x *EnterGameNodeInfoPBComponent) Reset() {
	*x = EnterGameNodeInfoPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_player_login_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterGameNodeInfoPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameNodeInfoPBComponent) ProtoMessage() {}

func (x *EnterGameNodeInfoPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_player_login_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterGameNodeInfoPBComponent.ProtoReflect.Descriptor instead.
func (*EnterGameNodeInfoPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_player_login_comp_proto_rawDescGZIP(), []int{0}
}

func (x *EnterGameNodeInfoPBComponent) GetEnterGsType() uint32 {
	if x != nil {
		return x.EnterGsType
	}
	return 0
}

var File_proto_logic_component_player_login_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_player_login_comp_proto_rawDesc = []byte{
	0x0a, 0x2d, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x6c,
	0x6f, 0x67, 0x69, 0x6e, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22,
	0x42, 0x0a, 0x1c, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65,
	0x49, 0x6e, 0x66, 0x6f, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12,
	0x22, 0x0a, 0x0d, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x5f, 0x67, 0x73, 0x5f, 0x74, 0x79, 0x70, 0x65,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x73, 0x54,
	0x79, 0x70, 0x65, 0x2a, 0x56, 0x0a, 0x0b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x73, 0x54, 0x79,
	0x70, 0x65, 0x12, 0x0e, 0x0a, 0x0a, 0x4c, 0x4f, 0x47, 0x49, 0x4e, 0x5f, 0x4e, 0x4f, 0x4e, 0x45,
	0x10, 0x00, 0x12, 0x0f, 0x0a, 0x0b, 0x4c, 0x4f, 0x47, 0x49, 0x4e, 0x5f, 0x46, 0x49, 0x52, 0x53,
	0x54, 0x10, 0x01, 0x12, 0x11, 0x0a, 0x0d, 0x4c, 0x4f, 0x47, 0x49, 0x4e, 0x5f, 0x52, 0x45, 0x50,
	0x4c, 0x41, 0x43, 0x45, 0x10, 0x02, 0x12, 0x13, 0x0a, 0x0f, 0x4c, 0x4f, 0x47, 0x49, 0x4e, 0x5f,
	0x52, 0x45, 0x43, 0x4f, 0x4e, 0x4e, 0x45, 0x43, 0x54, 0x10, 0x03, 0x42, 0x09, 0x5a, 0x07, 0x70,
	0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_component_player_login_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_player_login_comp_proto_rawDescData = file_proto_logic_component_player_login_comp_proto_rawDesc
)

func file_proto_logic_component_player_login_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_player_login_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_player_login_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_player_login_comp_proto_rawDescData)
	})
	return file_proto_logic_component_player_login_comp_proto_rawDescData
}

var file_proto_logic_component_player_login_comp_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_proto_logic_component_player_login_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_proto_logic_component_player_login_comp_proto_goTypes = []any{
	(EnterGsType)(0),                     // 0: EnterGsType
	(*EnterGameNodeInfoPBComponent)(nil), // 1: EnterGameNodeInfoPBComponent
}
var file_proto_logic_component_player_login_comp_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_logic_component_player_login_comp_proto_init() }
func file_proto_logic_component_player_login_comp_proto_init() {
	if File_proto_logic_component_player_login_comp_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_player_login_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*EnterGameNodeInfoPBComponent); i {
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
			RawDescriptor: file_proto_logic_component_player_login_comp_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_player_login_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_player_login_comp_proto_depIdxs,
		EnumInfos:         file_proto_logic_component_player_login_comp_proto_enumTypes,
		MessageInfos:      file_proto_logic_component_player_login_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_player_login_comp_proto = out.File
	file_proto_logic_component_player_login_comp_proto_rawDesc = nil
	file_proto_logic_component_player_login_comp_proto_goTypes = nil
	file_proto_logic_component_player_login_comp_proto_depIdxs = nil
}
