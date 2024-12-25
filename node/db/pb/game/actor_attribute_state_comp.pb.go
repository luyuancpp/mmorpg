// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: proto/logic/component/actor_attribute_state_comp.proto

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

type CombatStateFlagsPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	StateFlags map[uint32]bool `protobuf:"bytes,1,rep,name=state_flags,json=stateFlags,proto3" json:"state_flags,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"` // 单独的属性同步映射
}

func (x *CombatStateFlagsPbComponent) Reset() {
	*x = CombatStateFlagsPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CombatStateFlagsPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CombatStateFlagsPbComponent) ProtoMessage() {}

func (x *CombatStateFlagsPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CombatStateFlagsPbComponent.ProtoReflect.Descriptor instead.
func (*CombatStateFlagsPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_attribute_state_comp_proto_rawDescGZIP(), []int{0}
}

func (x *CombatStateFlagsPbComponent) GetStateFlags() map[uint32]bool {
	if x != nil {
		return x.StateFlags
	}
	return nil
}

// 计算属性，服务器端计算
type CalculatedAttributesPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	AttackPower  uint64 `protobuf:"varint,1,opt,name=attack_power,json=attackPower,proto3" json:"attack_power,omitempty"`    // 攻击力
	DefensePower uint64 `protobuf:"varint,2,opt,name=defense_power,json=defensePower,proto3" json:"defense_power,omitempty"` // 防御力
	IsDead       bool   `protobuf:"varint,3,opt,name=isDead,proto3" json:"isDead,omitempty"`                                 // 独立的生死状态
}

func (x *CalculatedAttributesPbComponent) Reset() {
	*x = CalculatedAttributesPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CalculatedAttributesPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CalculatedAttributesPbComponent) ProtoMessage() {}

func (x *CalculatedAttributesPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CalculatedAttributesPbComponent.ProtoReflect.Descriptor instead.
func (*CalculatedAttributesPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_attribute_state_comp_proto_rawDescGZIP(), []int{1}
}

func (x *CalculatedAttributesPbComponent) GetAttackPower() uint64 {
	if x != nil {
		return x.AttackPower
	}
	return 0
}

func (x *CalculatedAttributesPbComponent) GetDefensePower() uint64 {
	if x != nil {
		return x.DefensePower
	}
	return 0
}

func (x *CalculatedAttributesPbComponent) GetIsDead() bool {
	if x != nil {
		return x.IsDead
	}
	return false
}

// 衍生属性，服务器端计算
type DerivedAttributesPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	MaxHealth uint64 `protobuf:"varint,1,opt,name=max_health,json=maxHealth,proto3" json:"max_health,omitempty"` // 最大生命值
}

func (x *DerivedAttributesPbComponent) Reset() {
	*x = DerivedAttributesPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *DerivedAttributesPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*DerivedAttributesPbComponent) ProtoMessage() {}

func (x *DerivedAttributesPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use DerivedAttributesPbComponent.ProtoReflect.Descriptor instead.
func (*DerivedAttributesPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_attribute_state_comp_proto_rawDescGZIP(), []int{2}
}

func (x *DerivedAttributesPbComponent) GetMaxHealth() uint64 {
	if x != nil {
		return x.MaxHealth
	}
	return 0
}

var File_proto_logic_component_actor_attribute_state_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_actor_attribute_state_comp_proto_rawDesc = []byte{
	0x0a, 0x36, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x61, 0x74,
	0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x63, 0x6f,
	0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x17, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x22, 0xab, 0x01, 0x0a, 0x1b, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53, 0x74, 0x61, 0x74,
	0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x12, 0x4d, 0x0a, 0x0b, 0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x66, 0x6c, 0x61, 0x67, 0x73,
	0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x2c, 0x2e, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53,
	0x74, 0x61, 0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x45,
	0x6e, 0x74, 0x72, 0x79, 0x52, 0x0a, 0x73, 0x74, 0x61, 0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73,
	0x1a, 0x3d, 0x0a, 0x0f, 0x53, 0x74, 0x61, 0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x45, 0x6e,
	0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d,
	0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x22,
	0x81, 0x01, 0x0a, 0x1f, 0x43, 0x61, 0x6c, 0x63, 0x75, 0x6c, 0x61, 0x74, 0x65, 0x64, 0x41, 0x74,
	0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e,
	0x65, 0x6e, 0x74, 0x12, 0x21, 0x0a, 0x0c, 0x61, 0x74, 0x74, 0x61, 0x63, 0x6b, 0x5f, 0x70, 0x6f,
	0x77, 0x65, 0x72, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0b, 0x61, 0x74, 0x74, 0x61, 0x63,
	0x6b, 0x50, 0x6f, 0x77, 0x65, 0x72, 0x12, 0x23, 0x0a, 0x0d, 0x64, 0x65, 0x66, 0x65, 0x6e, 0x73,
	0x65, 0x5f, 0x70, 0x6f, 0x77, 0x65, 0x72, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0c, 0x64,
	0x65, 0x66, 0x65, 0x6e, 0x73, 0x65, 0x50, 0x6f, 0x77, 0x65, 0x72, 0x12, 0x16, 0x0a, 0x06, 0x69,
	0x73, 0x44, 0x65, 0x61, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x08, 0x52, 0x06, 0x69, 0x73, 0x44,
	0x65, 0x61, 0x64, 0x22, 0x3d, 0x0a, 0x1c, 0x44, 0x65, 0x72, 0x69, 0x76, 0x65, 0x64, 0x41, 0x74,
	0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e,
	0x65, 0x6e, 0x74, 0x12, 0x1d, 0x0a, 0x0a, 0x6d, 0x61, 0x78, 0x5f, 0x68, 0x65, 0x61, 0x6c, 0x74,
	0x68, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x6d, 0x61, 0x78, 0x48, 0x65, 0x61, 0x6c,
	0x74, 0x68, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_component_actor_attribute_state_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_actor_attribute_state_comp_proto_rawDescData = file_proto_logic_component_actor_attribute_state_comp_proto_rawDesc
)

func file_proto_logic_component_actor_attribute_state_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_actor_attribute_state_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_actor_attribute_state_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_actor_attribute_state_comp_proto_rawDescData)
	})
	return file_proto_logic_component_actor_attribute_state_comp_proto_rawDescData
}

var file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_proto_logic_component_actor_attribute_state_comp_proto_goTypes = []any{
	(*CombatStateFlagsPbComponent)(nil),     // 0: CombatStateFlagsPbComponent
	(*CalculatedAttributesPbComponent)(nil), // 1: CalculatedAttributesPbComponent
	(*DerivedAttributesPbComponent)(nil),    // 2: DerivedAttributesPbComponent
	nil,                                     // 3: CombatStateFlagsPbComponent.StateFlagsEntry
}
var file_proto_logic_component_actor_attribute_state_comp_proto_depIdxs = []int32{
	3, // 0: CombatStateFlagsPbComponent.state_flags:type_name -> CombatStateFlagsPbComponent.StateFlagsEntry
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_proto_logic_component_actor_attribute_state_comp_proto_init() }
func file_proto_logic_component_actor_attribute_state_comp_proto_init() {
	if File_proto_logic_component_actor_attribute_state_comp_proto != nil {
		return
	}
	file_proto_common_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*CombatStateFlagsPbComponent); i {
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
		file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*CalculatedAttributesPbComponent); i {
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
		file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*DerivedAttributesPbComponent); i {
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
			RawDescriptor: file_proto_logic_component_actor_attribute_state_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_actor_attribute_state_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_actor_attribute_state_comp_proto_depIdxs,
		MessageInfos:      file_proto_logic_component_actor_attribute_state_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_actor_attribute_state_comp_proto = out.File
	file_proto_logic_component_actor_attribute_state_comp_proto_rawDesc = nil
	file_proto_logic_component_actor_attribute_state_comp_proto_goTypes = nil
	file_proto_logic_component_actor_attribute_state_comp_proto_depIdxs = nil
}
