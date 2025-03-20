// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/component/skill_comp.proto

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

type SkillPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SkillTableId uint64 `protobuf:"varint,1,opt,name=skill_table_id,json=skillTableId,proto3" json:"skill_table_id,omitempty"`
}

func (x *SkillPBComponent) Reset() {
	*x = SkillPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_skill_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillPBComponent) ProtoMessage() {}

func (x *SkillPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_skill_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillPBComponent.ProtoReflect.Descriptor instead.
func (*SkillPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_skill_comp_proto_rawDescGZIP(), []int{0}
}

func (x *SkillPBComponent) GetSkillTableId() uint64 {
	if x != nil {
		return x.SkillTableId
	}
	return 0
}

type SkillContextPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Caster         uint64            `protobuf:"varint,1,opt,name=caster,proto3" json:"caster,omitempty"`                                                                                                         // 施法者
	Target         uint64            `protobuf:"varint,2,opt,name=target,proto3" json:"target,omitempty"`                                                                                                         // 目标（如果有）
	SkillId        uint64            `protobuf:"varint,3,opt,name=SkillId,proto3" json:"SkillId,omitempty"`                                                                                                       // 技能唯一id
	SkillTableId   uint32            `protobuf:"varint,4,opt,name=skillTableId,proto3" json:"skillTableId,omitempty"`                                                                                             // 技能表id
	CastPosition   *Transform        `protobuf:"bytes,5,opt,name=castPosition,proto3" json:"castPosition,omitempty"`                                                                                              // 施法位置
	CastTime       uint64            `protobuf:"varint,6,opt,name=castTime,proto3" json:"castTime,omitempty"`                                                                                                     // 施法时间
	State          string            `protobuf:"bytes,7,opt,name=state,proto3" json:"state,omitempty"`                                                                                                            // 当前状态
	AdditionalData map[string]uint32 `protobuf:"bytes,8,rep,name=additionalData,proto3" json:"additionalData,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"` // 附加数据
}

func (x *SkillContextPBComponent) Reset() {
	*x = SkillContextPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_skill_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillContextPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillContextPBComponent) ProtoMessage() {}

func (x *SkillContextPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_skill_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillContextPBComponent.ProtoReflect.Descriptor instead.
func (*SkillContextPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_skill_comp_proto_rawDescGZIP(), []int{1}
}

func (x *SkillContextPBComponent) GetCaster() uint64 {
	if x != nil {
		return x.Caster
	}
	return 0
}

func (x *SkillContextPBComponent) GetTarget() uint64 {
	if x != nil {
		return x.Target
	}
	return 0
}

func (x *SkillContextPBComponent) GetSkillId() uint64 {
	if x != nil {
		return x.SkillId
	}
	return 0
}

func (x *SkillContextPBComponent) GetSkillTableId() uint32 {
	if x != nil {
		return x.SkillTableId
	}
	return 0
}

func (x *SkillContextPBComponent) GetCastPosition() *Transform {
	if x != nil {
		return x.CastPosition
	}
	return nil
}

func (x *SkillContextPBComponent) GetCastTime() uint64 {
	if x != nil {
		return x.CastTime
	}
	return 0
}

func (x *SkillContextPBComponent) GetState() string {
	if x != nil {
		return x.State
	}
	return ""
}

func (x *SkillContextPBComponent) GetAdditionalData() map[string]uint32 {
	if x != nil {
		return x.AdditionalData
	}
	return nil
}

var File_proto_logic_component_skill_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_skill_comp_proto_rawDesc = []byte{
	0x0a, 0x26, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x63, 0x6f,
	0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x26, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f,
	0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x22, 0x38, 0x0a, 0x10, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x12, 0x24, 0x0a, 0x0e, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x74, 0x61,
	0x62, 0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0c, 0x73, 0x6b,
	0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64, 0x22, 0x82, 0x03, 0x0a, 0x17, 0x53,
	0x6b, 0x69, 0x6c, 0x6c, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x78, 0x74, 0x50, 0x42, 0x43, 0x6f, 0x6d,
	0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x16, 0x0a, 0x06, 0x63, 0x61, 0x73, 0x74, 0x65, 0x72,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x63, 0x61, 0x73, 0x74, 0x65, 0x72, 0x12, 0x16,
	0x0a, 0x06, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06,
	0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x12, 0x18, 0x0a, 0x07, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49,
	0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x04, 0x52, 0x07, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49, 0x64,
	0x12, 0x22, 0x0a, 0x0c, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64,
	0x18, 0x04, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62,
	0x6c, 0x65, 0x49, 0x64, 0x12, 0x2e, 0x0a, 0x0c, 0x63, 0x61, 0x73, 0x74, 0x50, 0x6f, 0x73, 0x69,
	0x74, 0x69, 0x6f, 0x6e, 0x18, 0x05, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0a, 0x2e, 0x54, 0x72, 0x61,
	0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x52, 0x0c, 0x63, 0x61, 0x73, 0x74, 0x50, 0x6f, 0x73, 0x69,
	0x74, 0x69, 0x6f, 0x6e, 0x12, 0x1a, 0x0a, 0x08, 0x63, 0x61, 0x73, 0x74, 0x54, 0x69, 0x6d, 0x65,
	0x18, 0x06, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x63, 0x61, 0x73, 0x74, 0x54, 0x69, 0x6d, 0x65,
	0x12, 0x14, 0x0a, 0x05, 0x73, 0x74, 0x61, 0x74, 0x65, 0x18, 0x07, 0x20, 0x01, 0x28, 0x09, 0x52,
	0x05, 0x73, 0x74, 0x61, 0x74, 0x65, 0x12, 0x54, 0x0a, 0x0e, 0x61, 0x64, 0x64, 0x69, 0x74, 0x69,
	0x6f, 0x6e, 0x61, 0x6c, 0x44, 0x61, 0x74, 0x61, 0x18, 0x08, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x2c,
	0x2e, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x78, 0x74, 0x50, 0x42, 0x43,
	0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x41, 0x64, 0x64, 0x69, 0x74, 0x69, 0x6f,
	0x6e, 0x61, 0x6c, 0x44, 0x61, 0x74, 0x61, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x0e, 0x61, 0x64,
	0x64, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x44, 0x61, 0x74, 0x61, 0x1a, 0x41, 0x0a, 0x13,
	0x41, 0x64, 0x64, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x44, 0x61, 0x74, 0x61, 0x45, 0x6e,
	0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09,
	0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x42,
	0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x33,
}

var (
	file_proto_logic_component_skill_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_skill_comp_proto_rawDescData = file_proto_logic_component_skill_comp_proto_rawDesc
)

func file_proto_logic_component_skill_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_skill_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_skill_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_skill_comp_proto_rawDescData)
	})
	return file_proto_logic_component_skill_comp_proto_rawDescData
}

var file_proto_logic_component_skill_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_proto_logic_component_skill_comp_proto_goTypes = []any{
	(*SkillPBComponent)(nil),        // 0: SkillPBComponent
	(*SkillContextPBComponent)(nil), // 1: SkillContextPBComponent
	nil,                             // 2: SkillContextPBComponent.AdditionalDataEntry
	(*Transform)(nil),               // 3: Transform
}
var file_proto_logic_component_skill_comp_proto_depIdxs = []int32{
	3, // 0: SkillContextPBComponent.castPosition:type_name -> Transform
	2, // 1: SkillContextPBComponent.additionalData:type_name -> SkillContextPBComponent.AdditionalDataEntry
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_proto_logic_component_skill_comp_proto_init() }
func file_proto_logic_component_skill_comp_proto_init() {
	if File_proto_logic_component_skill_comp_proto != nil {
		return
	}
	file_proto_logic_component_actor_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_skill_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*SkillPBComponent); i {
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
		file_proto_logic_component_skill_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*SkillContextPBComponent); i {
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
			RawDescriptor: file_proto_logic_component_skill_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_skill_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_skill_comp_proto_depIdxs,
		MessageInfos:      file_proto_logic_component_skill_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_skill_comp_proto = out.File
	file_proto_logic_component_skill_comp_proto_rawDesc = nil
	file_proto_logic_component_skill_comp_proto_goTypes = nil
	file_proto_logic_component_skill_comp_proto_depIdxs = nil
}
