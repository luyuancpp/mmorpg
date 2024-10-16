// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/component/buff_comp.proto

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

type BuffContextPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	DamageValue float32 `protobuf:"fixed32,1,opt,name=damage_value,json=damageValue,proto3" json:"damage_value,omitempty"`
}

func (x *BuffContextPBComponent) Reset() {
	*x = BuffContextPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_buff_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BuffContextPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffContextPBComponent) ProtoMessage() {}

func (x *BuffContextPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_buff_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffContextPBComponent.ProtoReflect.Descriptor instead.
func (*BuffContextPBComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_buff_comp_proto_rawDescGZIP(), []int{0}
}

func (x *BuffContextPBComponent) GetDamageValue() float32 {
	if x != nil {
		return x.DamageValue
	}
	return 0
}

type BuffPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	BuffId        uint64 `protobuf:"varint,1,opt,name=buff_id,json=buffId,proto3" json:"buff_id,omitempty"`
	BuffTableId   uint32 `protobuf:"varint,2,opt,name=buff_table_id,json=buffTableId,proto3" json:"buff_table_id,omitempty"`
	AbilityId     uint32 `protobuf:"varint,3,opt,name=ability_id,json=abilityId,proto3" json:"ability_id,omitempty"`
	ParentEntity  uint64 `protobuf:"varint,4,opt,name=parent_entity,json=parentEntity,proto3" json:"parent_entity,omitempty"`
	Layer         uint32 `protobuf:"varint,5,opt,name=layer,proto3" json:"layer,omitempty"`
	Caster        uint64 `protobuf:"varint,6,opt,name=caster,proto3" json:"caster,omitempty"`
	Triggerdamage bool   `protobuf:"varint,7,opt,name=triggerdamage,proto3" json:"triggerdamage,omitempty"`
}

func (x *BuffPbComponent) Reset() {
	*x = BuffPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_buff_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BuffPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffPbComponent) ProtoMessage() {}

func (x *BuffPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_buff_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffPbComponent.ProtoReflect.Descriptor instead.
func (*BuffPbComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_buff_comp_proto_rawDescGZIP(), []int{1}
}

func (x *BuffPbComponent) GetBuffId() uint64 {
	if x != nil {
		return x.BuffId
	}
	return 0
}

func (x *BuffPbComponent) GetBuffTableId() uint32 {
	if x != nil {
		return x.BuffTableId
	}
	return 0
}

func (x *BuffPbComponent) GetAbilityId() uint32 {
	if x != nil {
		return x.AbilityId
	}
	return 0
}

func (x *BuffPbComponent) GetParentEntity() uint64 {
	if x != nil {
		return x.ParentEntity
	}
	return 0
}

func (x *BuffPbComponent) GetLayer() uint32 {
	if x != nil {
		return x.Layer
	}
	return 0
}

func (x *BuffPbComponent) GetCaster() uint64 {
	if x != nil {
		return x.Caster
	}
	return 0
}

func (x *BuffPbComponent) GetTriggerdamage() bool {
	if x != nil {
		return x.Triggerdamage
	}
	return false
}

type DamageEventPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SkillId    uint64  `protobuf:"varint,1,opt,name=skill_id,json=skillId,proto3" json:"skill_id,omitempty"`
	AttackerId uint64  `protobuf:"varint,2,opt,name=attacker_id,json=attackerId,proto3" json:"attacker_id,omitempty"`
	Target     uint64  `protobuf:"varint,3,opt,name=target,proto3" json:"target,omitempty"`
	Damage     float64 `protobuf:"fixed64,4,opt,name=damage,proto3" json:"damage,omitempty"`
	DamageType uint32  `protobuf:"varint,5,opt,name=damage_type,json=damageType,proto3" json:"damage_type,omitempty"`
}

func (x *DamageEventPbComponent) Reset() {
	*x = DamageEventPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_buff_comp_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *DamageEventPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*DamageEventPbComponent) ProtoMessage() {}

func (x *DamageEventPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_buff_comp_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use DamageEventPbComponent.ProtoReflect.Descriptor instead.
func (*DamageEventPbComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_buff_comp_proto_rawDescGZIP(), []int{2}
}

func (x *DamageEventPbComponent) GetSkillId() uint64 {
	if x != nil {
		return x.SkillId
	}
	return 0
}

func (x *DamageEventPbComponent) GetAttackerId() uint64 {
	if x != nil {
		return x.AttackerId
	}
	return 0
}

func (x *DamageEventPbComponent) GetTarget() uint64 {
	if x != nil {
		return x.Target
	}
	return 0
}

func (x *DamageEventPbComponent) GetDamage() float64 {
	if x != nil {
		return x.Damage
	}
	return 0
}

func (x *DamageEventPbComponent) GetDamageType() uint32 {
	if x != nil {
		return x.DamageType
	}
	return 0
}

type PeriodicBuffPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PeriodicTimer float64 `protobuf:"fixed64,1,opt,name=periodic_timer,json=periodicTimer,proto3" json:"periodic_timer,omitempty"`
	TicksDone     uint32  `protobuf:"varint,2,opt,name=ticks_done,json=ticksDone,proto3" json:"ticks_done,omitempty"`
}

func (x *PeriodicBuffPbComponent) Reset() {
	*x = PeriodicBuffPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_buff_comp_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PeriodicBuffPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PeriodicBuffPbComponent) ProtoMessage() {}

func (x *PeriodicBuffPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_buff_comp_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PeriodicBuffPbComponent.ProtoReflect.Descriptor instead.
func (*PeriodicBuffPbComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_buff_comp_proto_rawDescGZIP(), []int{3}
}

func (x *PeriodicBuffPbComponent) GetPeriodicTimer() float64 {
	if x != nil {
		return x.PeriodicTimer
	}
	return 0
}

func (x *PeriodicBuffPbComponent) GetTicksDone() uint32 {
	if x != nil {
		return x.TicksDone
	}
	return 0
}

var File_logic_component_buff_comp_proto protoreflect.FileDescriptor

var file_logic_component_buff_comp_proto_rawDesc = []byte{
	0x0a, 0x1f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x2f, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x22, 0x3b, 0x0a, 0x16, 0x42, 0x75, 0x66, 0x66, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x78, 0x74,
	0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x21, 0x0a, 0x0c, 0x64,
	0x61, 0x6d, 0x61, 0x67, 0x65, 0x5f, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x02, 0x52, 0x0b, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65, 0x56, 0x61, 0x6c, 0x75, 0x65, 0x22, 0xe6,
	0x01, 0x0a, 0x0f, 0x42, 0x75, 0x66, 0x66, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65,
	0x6e, 0x74, 0x12, 0x17, 0x0a, 0x07, 0x62, 0x75, 0x66, 0x66, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x06, 0x62, 0x75, 0x66, 0x66, 0x49, 0x64, 0x12, 0x22, 0x0a, 0x0d, 0x62,
	0x75, 0x66, 0x66, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x0b, 0x62, 0x75, 0x66, 0x66, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64, 0x12,
	0x1d, 0x0a, 0x0a, 0x61, 0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x09, 0x61, 0x62, 0x69, 0x6c, 0x69, 0x74, 0x79, 0x49, 0x64, 0x12, 0x23,
	0x0a, 0x0d, 0x70, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x5f, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18,
	0x04, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0c, 0x70, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x45, 0x6e, 0x74,
	0x69, 0x74, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x18, 0x05, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x05, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x12, 0x16, 0x0a, 0x06, 0x63, 0x61, 0x73,
	0x74, 0x65, 0x72, 0x18, 0x06, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x63, 0x61, 0x73, 0x74, 0x65,
	0x72, 0x12, 0x24, 0x0a, 0x0d, 0x74, 0x72, 0x69, 0x67, 0x67, 0x65, 0x72, 0x64, 0x61, 0x6d, 0x61,
	0x67, 0x65, 0x18, 0x07, 0x20, 0x01, 0x28, 0x08, 0x52, 0x0d, 0x74, 0x72, 0x69, 0x67, 0x67, 0x65,
	0x72, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65, 0x22, 0xa5, 0x01, 0x0a, 0x16, 0x44, 0x61, 0x6d, 0x61,
	0x67, 0x65, 0x45, 0x76, 0x65, 0x6e, 0x74, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65,
	0x6e, 0x74, 0x12, 0x19, 0x0a, 0x08, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x69, 0x64, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x04, 0x52, 0x07, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x49, 0x64, 0x12, 0x1f, 0x0a,
	0x0b, 0x61, 0x74, 0x74, 0x61, 0x63, 0x6b, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x0a, 0x61, 0x74, 0x74, 0x61, 0x63, 0x6b, 0x65, 0x72, 0x49, 0x64, 0x12, 0x16,
	0x0a, 0x06, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x18, 0x03, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06,
	0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x12, 0x16, 0x0a, 0x06, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65,
	0x18, 0x04, 0x20, 0x01, 0x28, 0x01, 0x52, 0x06, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65, 0x12, 0x1f,
	0x0a, 0x0b, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x18, 0x05, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x0a, 0x64, 0x61, 0x6d, 0x61, 0x67, 0x65, 0x54, 0x79, 0x70, 0x65, 0x22,
	0x5f, 0x0a, 0x17, 0x50, 0x65, 0x72, 0x69, 0x6f, 0x64, 0x69, 0x63, 0x42, 0x75, 0x66, 0x66, 0x50,
	0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x25, 0x0a, 0x0e, 0x70, 0x65,
	0x72, 0x69, 0x6f, 0x64, 0x69, 0x63, 0x5f, 0x74, 0x69, 0x6d, 0x65, 0x72, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x01, 0x52, 0x0d, 0x70, 0x65, 0x72, 0x69, 0x6f, 0x64, 0x69, 0x63, 0x54, 0x69, 0x6d, 0x65,
	0x72, 0x12, 0x1d, 0x0a, 0x0a, 0x74, 0x69, 0x63, 0x6b, 0x73, 0x5f, 0x64, 0x6f, 0x6e, 0x65, 0x18,
	0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x74, 0x69, 0x63, 0x6b, 0x73, 0x44, 0x6f, 0x6e, 0x65,
	0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x33,
}

var (
	file_logic_component_buff_comp_proto_rawDescOnce sync.Once
	file_logic_component_buff_comp_proto_rawDescData = file_logic_component_buff_comp_proto_rawDesc
)

func file_logic_component_buff_comp_proto_rawDescGZIP() []byte {
	file_logic_component_buff_comp_proto_rawDescOnce.Do(func() {
		file_logic_component_buff_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_component_buff_comp_proto_rawDescData)
	})
	return file_logic_component_buff_comp_proto_rawDescData
}

var file_logic_component_buff_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_logic_component_buff_comp_proto_goTypes = []any{
	(*BuffContextPBComponent)(nil),  // 0: BuffContextPBComponent
	(*BuffPbComponent)(nil),         // 1: BuffPbComponent
	(*DamageEventPbComponent)(nil),  // 2: DamageEventPbComponent
	(*PeriodicBuffPbComponent)(nil), // 3: PeriodicBuffPbComponent
}
var file_logic_component_buff_comp_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_logic_component_buff_comp_proto_init() }
func file_logic_component_buff_comp_proto_init() {
	if File_logic_component_buff_comp_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_logic_component_buff_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*BuffContextPBComponent); i {
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
		file_logic_component_buff_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*BuffPbComponent); i {
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
		file_logic_component_buff_comp_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*DamageEventPbComponent); i {
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
		file_logic_component_buff_comp_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*PeriodicBuffPbComponent); i {
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
			RawDescriptor: file_logic_component_buff_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_logic_component_buff_comp_proto_goTypes,
		DependencyIndexes: file_logic_component_buff_comp_proto_depIdxs,
		MessageInfos:      file_logic_component_buff_comp_proto_msgTypes,
	}.Build()
	File_logic_component_buff_comp_proto = out.File
	file_logic_component_buff_comp_proto_rawDesc = nil
	file_logic_component_buff_comp_proto_goTypes = nil
	file_logic_component_buff_comp_proto_depIdxs = nil
}
