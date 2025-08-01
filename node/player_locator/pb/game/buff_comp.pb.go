// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/logic/component/buff_comp.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
	unsafe "unsafe"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type BuffContextPBComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	DamageValue   float32                `protobuf:"fixed32,1,opt,name=damage_value,json=damageValue,proto3" json:"damage_value,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *BuffContextPBComponent) Reset() {
	*x = BuffContextPBComponent{}
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BuffContextPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffContextPBComponent) ProtoMessage() {}

func (x *BuffContextPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[0]
	if x != nil {
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
	return file_proto_logic_component_buff_comp_proto_rawDescGZIP(), []int{0}
}

func (x *BuffContextPBComponent) GetDamageValue() float32 {
	if x != nil {
		return x.DamageValue
	}
	return 0
}

type BuffPeriodicBuffPbComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	PeriodicTimer float64                `protobuf:"fixed64,1,opt,name=periodic_timer,json=periodicTimer,proto3" json:"periodic_timer,omitempty"`
	TicksDone     uint32                 `protobuf:"varint,2,opt,name=ticks_done,json=ticksDone,proto3" json:"ticks_done,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *BuffPeriodicBuffPbComponent) Reset() {
	*x = BuffPeriodicBuffPbComponent{}
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[1]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BuffPeriodicBuffPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffPeriodicBuffPbComponent) ProtoMessage() {}

func (x *BuffPeriodicBuffPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[1]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffPeriodicBuffPbComponent.ProtoReflect.Descriptor instead.
func (*BuffPeriodicBuffPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_buff_comp_proto_rawDescGZIP(), []int{1}
}

func (x *BuffPeriodicBuffPbComponent) GetPeriodicTimer() float64 {
	if x != nil {
		return x.PeriodicTimer
	}
	return 0
}

func (x *BuffPeriodicBuffPbComponent) GetTicksDone() uint32 {
	if x != nil {
		return x.TicksDone
	}
	return 0
}

type BuffPbComponent struct {
	state           protoimpl.MessageState       `protogen:"open.v1"`
	BuffId          uint64                       `protobuf:"varint,1,opt,name=buff_id,json=buffId,proto3" json:"buff_id,omitempty"`
	BuffTableId     uint32                       `protobuf:"varint,2,opt,name=buff_table_id,json=buffTableId,proto3" json:"buff_table_id,omitempty"`
	AbilityId       uint32                       `protobuf:"varint,3,opt,name=ability_id,json=abilityId,proto3" json:"ability_id,omitempty"`
	ParentEntity    uint64                       `protobuf:"varint,4,opt,name=parent_entity,json=parentEntity,proto3" json:"parent_entity,omitempty"`
	Layer           uint32                       `protobuf:"varint,5,opt,name=layer,proto3" json:"layer,omitempty"`
	Caster          uint64                       `protobuf:"varint,6,opt,name=caster,proto3" json:"caster,omitempty"`
	Triggerdamage   bool                         `protobuf:"varint,7,opt,name=triggerdamage,proto3" json:"triggerdamage,omitempty"`
	ProcessedCaster uint64                       `protobuf:"varint,8,opt,name=processed_caster,json=processedCaster,proto3" json:"processed_caster,omitempty"` // 根据表处理后的施法者
	Data            []byte                       `protobuf:"bytes,9,opt,name=data,proto3" json:"data,omitempty"`                                               // 各种类型buff的各种数据
	Periodic        *BuffPeriodicBuffPbComponent `protobuf:"bytes,10,opt,name=periodic,proto3" json:"periodic,omitempty"`
	SubBuffListId   map[uint64]bool              `protobuf:"bytes,11,rep,name=sub_buff_list_id,json=subBuffListId,proto3" json:"sub_buff_list_id,omitempty" protobuf_key:"varint,1,opt,name=key" protobuf_val:"varint,2,opt,name=value"`
	HasAddedSubBuff bool                         `protobuf:"varint,12,opt,name=has_added_sub_buff,json=hasAddedSubBuff,proto3" json:"has_added_sub_buff,omitempty"`
	unknownFields   protoimpl.UnknownFields
	sizeCache       protoimpl.SizeCache
}

func (x *BuffPbComponent) Reset() {
	*x = BuffPbComponent{}
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[2]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BuffPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffPbComponent) ProtoMessage() {}

func (x *BuffPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[2]
	if x != nil {
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
	return file_proto_logic_component_buff_comp_proto_rawDescGZIP(), []int{2}
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

func (x *BuffPbComponent) GetProcessedCaster() uint64 {
	if x != nil {
		return x.ProcessedCaster
	}
	return 0
}

func (x *BuffPbComponent) GetData() []byte {
	if x != nil {
		return x.Data
	}
	return nil
}

func (x *BuffPbComponent) GetPeriodic() *BuffPeriodicBuffPbComponent {
	if x != nil {
		return x.Periodic
	}
	return nil
}

func (x *BuffPbComponent) GetSubBuffListId() map[uint64]bool {
	if x != nil {
		return x.SubBuffListId
	}
	return nil
}

func (x *BuffPbComponent) GetHasAddedSubBuff() bool {
	if x != nil {
		return x.HasAddedSubBuff
	}
	return false
}

type DamageEventPbComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	SkillId       uint64                 `protobuf:"varint,1,opt,name=skill_id,json=skillId,proto3" json:"skill_id,omitempty"`
	AttackerId    uint64                 `protobuf:"varint,2,opt,name=attacker_id,json=attackerId,proto3" json:"attacker_id,omitempty"`
	Target        uint64                 `protobuf:"varint,3,opt,name=target,proto3" json:"target,omitempty"`
	Damage        float64                `protobuf:"fixed64,4,opt,name=damage,proto3" json:"damage,omitempty"`
	DamageType    uint32                 `protobuf:"varint,5,opt,name=damage_type,json=damageType,proto3" json:"damage_type,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *DamageEventPbComponent) Reset() {
	*x = DamageEventPbComponent{}
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[3]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *DamageEventPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*DamageEventPbComponent) ProtoMessage() {}

func (x *DamageEventPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[3]
	if x != nil {
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
	return file_proto_logic_component_buff_comp_proto_rawDescGZIP(), []int{3}
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

type BuffNoDamageOrSkillHitInLastSecondsPbComp struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	LastTime      uint64                 `protobuf:"varint,1,opt,name=last_time,json=lastTime,proto3" json:"last_time,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *BuffNoDamageOrSkillHitInLastSecondsPbComp) Reset() {
	*x = BuffNoDamageOrSkillHitInLastSecondsPbComp{}
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[4]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BuffNoDamageOrSkillHitInLastSecondsPbComp) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BuffNoDamageOrSkillHitInLastSecondsPbComp) ProtoMessage() {}

func (x *BuffNoDamageOrSkillHitInLastSecondsPbComp) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_buff_comp_proto_msgTypes[4]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BuffNoDamageOrSkillHitInLastSecondsPbComp.ProtoReflect.Descriptor instead.
func (*BuffNoDamageOrSkillHitInLastSecondsPbComp) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_buff_comp_proto_rawDescGZIP(), []int{4}
}

func (x *BuffNoDamageOrSkillHitInLastSecondsPbComp) GetLastTime() uint64 {
	if x != nil {
		return x.LastTime
	}
	return 0
}

var File_proto_logic_component_buff_comp_proto protoreflect.FileDescriptor

const file_proto_logic_component_buff_comp_proto_rawDesc = "" +
	"\n" +
	"%proto/logic/component/buff_comp.proto\";\n" +
	"\x16BuffContextPBComponent\x12!\n" +
	"\fdamage_value\x18\x01 \x01(\x02R\vdamageValue\"c\n" +
	"\x1bBuffPeriodicBuffPbComponent\x12%\n" +
	"\x0eperiodic_timer\x18\x01 \x01(\x01R\rperiodicTimer\x12\x1d\n" +
	"\n" +
	"ticks_done\x18\x02 \x01(\rR\tticksDone\"\x9c\x04\n" +
	"\x0fBuffPbComponent\x12\x17\n" +
	"\abuff_id\x18\x01 \x01(\x04R\x06buffId\x12\"\n" +
	"\rbuff_table_id\x18\x02 \x01(\rR\vbuffTableId\x12\x1d\n" +
	"\n" +
	"ability_id\x18\x03 \x01(\rR\tabilityId\x12#\n" +
	"\rparent_entity\x18\x04 \x01(\x04R\fparentEntity\x12\x14\n" +
	"\x05layer\x18\x05 \x01(\rR\x05layer\x12\x16\n" +
	"\x06caster\x18\x06 \x01(\x04R\x06caster\x12$\n" +
	"\rtriggerdamage\x18\a \x01(\bR\rtriggerdamage\x12)\n" +
	"\x10processed_caster\x18\b \x01(\x04R\x0fprocessedCaster\x12\x12\n" +
	"\x04data\x18\t \x01(\fR\x04data\x128\n" +
	"\bperiodic\x18\n" +
	" \x01(\v2\x1c.BuffPeriodicBuffPbComponentR\bperiodic\x12L\n" +
	"\x10sub_buff_list_id\x18\v \x03(\v2#.BuffPbComponent.SubBuffListIdEntryR\rsubBuffListId\x12+\n" +
	"\x12has_added_sub_buff\x18\f \x01(\bR\x0fhasAddedSubBuff\x1a@\n" +
	"\x12SubBuffListIdEntry\x12\x10\n" +
	"\x03key\x18\x01 \x01(\x04R\x03key\x12\x14\n" +
	"\x05value\x18\x02 \x01(\bR\x05value:\x028\x01\"\xa5\x01\n" +
	"\x16DamageEventPbComponent\x12\x19\n" +
	"\bskill_id\x18\x01 \x01(\x04R\askillId\x12\x1f\n" +
	"\vattacker_id\x18\x02 \x01(\x04R\n" +
	"attackerId\x12\x16\n" +
	"\x06target\x18\x03 \x01(\x04R\x06target\x12\x16\n" +
	"\x06damage\x18\x04 \x01(\x01R\x06damage\x12\x1f\n" +
	"\vdamage_type\x18\x05 \x01(\rR\n" +
	"damageType\"H\n" +
	")BuffNoDamageOrSkillHitInLastSecondsPbComp\x12\x1b\n" +
	"\tlast_time\x18\x01 \x01(\x04R\blastTimeB\tZ\apb/gameb\x06proto3"

var (
	file_proto_logic_component_buff_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_buff_comp_proto_rawDescData []byte
)

func file_proto_logic_component_buff_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_buff_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_buff_comp_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_logic_component_buff_comp_proto_rawDesc), len(file_proto_logic_component_buff_comp_proto_rawDesc)))
	})
	return file_proto_logic_component_buff_comp_proto_rawDescData
}

var file_proto_logic_component_buff_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 6)
var file_proto_logic_component_buff_comp_proto_goTypes = []any{
	(*BuffContextPBComponent)(nil),                    // 0: BuffContextPBComponent
	(*BuffPeriodicBuffPbComponent)(nil),               // 1: BuffPeriodicBuffPbComponent
	(*BuffPbComponent)(nil),                           // 2: BuffPbComponent
	(*DamageEventPbComponent)(nil),                    // 3: DamageEventPbComponent
	(*BuffNoDamageOrSkillHitInLastSecondsPbComp)(nil), // 4: BuffNoDamageOrSkillHitInLastSecondsPbComp
	nil, // 5: BuffPbComponent.SubBuffListIdEntry
}
var file_proto_logic_component_buff_comp_proto_depIdxs = []int32{
	1, // 0: BuffPbComponent.periodic:type_name -> BuffPeriodicBuffPbComponent
	5, // 1: BuffPbComponent.sub_buff_list_id:type_name -> BuffPbComponent.SubBuffListIdEntry
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_proto_logic_component_buff_comp_proto_init() }
func file_proto_logic_component_buff_comp_proto_init() {
	if File_proto_logic_component_buff_comp_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_logic_component_buff_comp_proto_rawDesc), len(file_proto_logic_component_buff_comp_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   6,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_buff_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_buff_comp_proto_depIdxs,
		MessageInfos:      file_proto_logic_component_buff_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_buff_comp_proto = out.File
	file_proto_logic_component_buff_comp_proto_goTypes = nil
	file_proto_logic_component_buff_comp_proto_depIdxs = nil
}
