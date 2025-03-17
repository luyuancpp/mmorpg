// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v6.31.0--dev
// source: proto/logic/component/actor_comp.proto

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

type Location struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	X float64 `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y float64 `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z float64 `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
}

func (x *Location) Reset() {
	*x = Location{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Location) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Location) ProtoMessage() {}

func (x *Location) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Location.ProtoReflect.Descriptor instead.
func (*Location) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{0}
}

func (x *Location) GetX() float64 {
	if x != nil {
		return x.X
	}
	return 0
}

func (x *Location) GetY() float64 {
	if x != nil {
		return x.Y
	}
	return 0
}

func (x *Location) GetZ() float64 {
	if x != nil {
		return x.Z
	}
	return 0
}

type Rotation struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	X float64 `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y float64 `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z float64 `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
}

func (x *Rotation) Reset() {
	*x = Rotation{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Rotation) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Rotation) ProtoMessage() {}

func (x *Rotation) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Rotation.ProtoReflect.Descriptor instead.
func (*Rotation) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{1}
}

func (x *Rotation) GetX() float64 {
	if x != nil {
		return x.X
	}
	return 0
}

func (x *Rotation) GetY() float64 {
	if x != nil {
		return x.Y
	}
	return 0
}

func (x *Rotation) GetZ() float64 {
	if x != nil {
		return x.Z
	}
	return 0
}

type Scale struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	X float64 `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y float64 `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z float64 `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
}

func (x *Scale) Reset() {
	*x = Scale{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Scale) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Scale) ProtoMessage() {}

func (x *Scale) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Scale.ProtoReflect.Descriptor instead.
func (*Scale) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{2}
}

func (x *Scale) GetX() float64 {
	if x != nil {
		return x.X
	}
	return 0
}

func (x *Scale) GetY() float64 {
	if x != nil {
		return x.Y
	}
	return 0
}

func (x *Scale) GetZ() float64 {
	if x != nil {
		return x.Z
	}
	return 0
}

type Transform struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Location *Vector3  `protobuf:"bytes,1,opt,name=location,proto3" json:"location,omitempty"`
	Rotation *Rotation `protobuf:"bytes,2,opt,name=rotation,proto3" json:"rotation,omitempty"`
	Scale    *Scale    `protobuf:"bytes,3,opt,name=scale,proto3" json:"scale,omitempty"`
}

func (x *Transform) Reset() {
	*x = Transform{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Transform) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Transform) ProtoMessage() {}

func (x *Transform) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Transform.ProtoReflect.Descriptor instead.
func (*Transform) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{3}
}

func (x *Transform) GetLocation() *Vector3 {
	if x != nil {
		return x.Location
	}
	return nil
}

func (x *Transform) GetRotation() *Rotation {
	if x != nil {
		return x.Rotation
	}
	return nil
}

func (x *Transform) GetScale() *Scale {
	if x != nil {
		return x.Scale
	}
	return nil
}

type Velocity struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	X float64 `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y float64 `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z float64 `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
}

func (x *Velocity) Reset() {
	*x = Velocity{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Velocity) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Velocity) ProtoMessage() {}

func (x *Velocity) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Velocity.ProtoReflect.Descriptor instead.
func (*Velocity) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{4}
}

func (x *Velocity) GetX() float64 {
	if x != nil {
		return x.X
	}
	return 0
}

func (x *Velocity) GetY() float64 {
	if x != nil {
		return x.Y
	}
	return 0
}

func (x *Velocity) GetZ() float64 {
	if x != nil {
		return x.Z
	}
	return 0
}

type Acceleration struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	X float64 `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y float64 `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z float64 `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
}

func (x *Acceleration) Reset() {
	*x = Acceleration{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Acceleration) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Acceleration) ProtoMessage() {}

func (x *Acceleration) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Acceleration.ProtoReflect.Descriptor instead.
func (*Acceleration) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{5}
}

func (x *Acceleration) GetX() float64 {
	if x != nil {
		return x.X
	}
	return 0
}

func (x *Acceleration) GetY() float64 {
	if x != nil {
		return x.Y
	}
	return 0
}

func (x *Acceleration) GetZ() float64 {
	if x != nil {
		return x.Z
	}
	return 0
}

// https://blog.codingnow.com/2008/11/aoi_server.html
// 视野半径
type ViewRadius struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Radius float64 `protobuf:"fixed64,1,opt,name=radius,proto3" json:"radius,omitempty"`
}

func (x *ViewRadius) Reset() {
	*x = ViewRadius{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[6]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ViewRadius) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ViewRadius) ProtoMessage() {}

func (x *ViewRadius) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[6]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ViewRadius.ProtoReflect.Descriptor instead.
func (*ViewRadius) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{6}
}

func (x *ViewRadius) GetRadius() float64 {
	if x != nil {
		return x.Radius
	}
	return 0
}

// 玩家基础属性组件，仅存储与战斗相关的基础属性
type BaseAttributesPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Strength   uint64 `protobuf:"varint,1,opt,name=strength,proto3" json:"strength,omitempty"`     // 力量，影响物理攻击
	Stamina    uint64 `protobuf:"varint,2,opt,name=stamina,proto3" json:"stamina,omitempty"`       // 耐力，影响生命值等
	Health     uint64 `protobuf:"varint,3,opt,name=health,proto3" json:"health,omitempty"`         // 当前生命值
	Mana       uint64 `protobuf:"varint,4,opt,name=mana,proto3" json:"mana,omitempty"`             // 当前法力值
	Critchance uint64 `protobuf:"varint,5,opt,name=critchance,proto3" json:"critchance,omitempty"` // 暴击几率
	Armor      uint64 `protobuf:"varint,6,opt,name=armor,proto3" json:"armor,omitempty"`           // 护甲
	Resistance uint64 `protobuf:"varint,7,opt,name=resistance,proto3" json:"resistance,omitempty"` // 抗性
}

func (x *BaseAttributesPbComponent) Reset() {
	*x = BaseAttributesPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[7]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BaseAttributesPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BaseAttributesPbComponent) ProtoMessage() {}

func (x *BaseAttributesPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[7]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BaseAttributesPbComponent.ProtoReflect.Descriptor instead.
func (*BaseAttributesPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{7}
}

func (x *BaseAttributesPbComponent) GetStrength() uint64 {
	if x != nil {
		return x.Strength
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetStamina() uint64 {
	if x != nil {
		return x.Stamina
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetHealth() uint64 {
	if x != nil {
		return x.Health
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetMana() uint64 {
	if x != nil {
		return x.Mana
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetCritchance() uint64 {
	if x != nil {
		return x.Critchance
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetArmor() uint64 {
	if x != nil {
		return x.Armor
	}
	return 0
}

func (x *BaseAttributesPbComponent) GetResistance() uint64 {
	if x != nil {
		return x.Resistance
	}
	return 0
}

// 玩家等级组件，专注于管理等级及与等级相关的功能
type LevelPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Level uint32 `protobuf:"varint,1,opt,name=level,proto3" json:"level,omitempty"` // 玩家等级
}

func (x *LevelPbComponent) Reset() {
	*x = LevelPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[8]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LevelPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LevelPbComponent) ProtoMessage() {}

func (x *LevelPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[8]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LevelPbComponent.ProtoReflect.Descriptor instead.
func (*LevelPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{8}
}

func (x *LevelPbComponent) GetLevel() uint32 {
	if x != nil {
		return x.Level
	}
	return 0
}

type ActorStatePbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	StateList map[uint32]bool `protobuf:"bytes,1,rep,name=state_list,json=stateList,proto3" json:"state_list,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
}

func (x *ActorStatePbComponent) Reset() {
	*x = ActorStatePbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_comp_proto_msgTypes[9]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ActorStatePbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ActorStatePbComponent) ProtoMessage() {}

func (x *ActorStatePbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[9]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ActorStatePbComponent.ProtoReflect.Descriptor instead.
func (*ActorStatePbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_comp_proto_rawDescGZIP(), []int{9}
}

func (x *ActorStatePbComponent) GetStateList() map[uint32]bool {
	if x != nil {
		return x.StateList
	}
	return nil
}

var File_proto_logic_component_actor_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_actor_comp_proto_rawDesc = []byte{
	0x0a, 0x26, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f,
	0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x17, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x22, 0x34, 0x0a, 0x08, 0x4c, 0x6f, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x0c, 0x0a,
	0x01, 0x78, 0x18, 0x01, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x78, 0x12, 0x0c, 0x0a, 0x01, 0x79,
	0x18, 0x02, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x79, 0x12, 0x0c, 0x0a, 0x01, 0x7a, 0x18, 0x03,
	0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x7a, 0x22, 0x34, 0x0a, 0x08, 0x52, 0x6f, 0x74, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x12, 0x0c, 0x0a, 0x01, 0x78, 0x18, 0x01, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01,
	0x78, 0x12, 0x0c, 0x0a, 0x01, 0x79, 0x18, 0x02, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x79, 0x12,
	0x0c, 0x0a, 0x01, 0x7a, 0x18, 0x03, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x7a, 0x22, 0x31, 0x0a,
	0x05, 0x53, 0x63, 0x61, 0x6c, 0x65, 0x12, 0x0c, 0x0a, 0x01, 0x78, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x01, 0x52, 0x01, 0x78, 0x12, 0x0c, 0x0a, 0x01, 0x79, 0x18, 0x02, 0x20, 0x01, 0x28, 0x01, 0x52,
	0x01, 0x79, 0x12, 0x0c, 0x0a, 0x01, 0x7a, 0x18, 0x03, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x7a,
	0x22, 0x76, 0x0a, 0x09, 0x54, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x12, 0x24, 0x0a,
	0x08, 0x6c, 0x6f, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32,
	0x08, 0x2e, 0x56, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x33, 0x52, 0x08, 0x6c, 0x6f, 0x63, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x12, 0x25, 0x0a, 0x08, 0x72, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x18,
	0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x09, 0x2e, 0x52, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e,
	0x52, 0x08, 0x72, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x1c, 0x0a, 0x05, 0x73, 0x63,
	0x61, 0x6c, 0x65, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x06, 0x2e, 0x53, 0x63, 0x61, 0x6c,
	0x65, 0x52, 0x05, 0x73, 0x63, 0x61, 0x6c, 0x65, 0x22, 0x34, 0x0a, 0x08, 0x56, 0x65, 0x6c, 0x6f,
	0x63, 0x69, 0x74, 0x79, 0x12, 0x0c, 0x0a, 0x01, 0x78, 0x18, 0x01, 0x20, 0x01, 0x28, 0x01, 0x52,
	0x01, 0x78, 0x12, 0x0c, 0x0a, 0x01, 0x79, 0x18, 0x02, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x79,
	0x12, 0x0c, 0x0a, 0x01, 0x7a, 0x18, 0x03, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x7a, 0x22, 0x38,
	0x0a, 0x0c, 0x41, 0x63, 0x63, 0x65, 0x6c, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x0c,
	0x0a, 0x01, 0x78, 0x18, 0x01, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x78, 0x12, 0x0c, 0x0a, 0x01,
	0x79, 0x18, 0x02, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x79, 0x12, 0x0c, 0x0a, 0x01, 0x7a, 0x18,
	0x03, 0x20, 0x01, 0x28, 0x01, 0x52, 0x01, 0x7a, 0x22, 0x24, 0x0a, 0x0a, 0x56, 0x69, 0x65, 0x77,
	0x52, 0x61, 0x64, 0x69, 0x75, 0x73, 0x12, 0x16, 0x0a, 0x06, 0x72, 0x61, 0x64, 0x69, 0x75, 0x73,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x01, 0x52, 0x06, 0x72, 0x61, 0x64, 0x69, 0x75, 0x73, 0x22, 0xd3,
	0x01, 0x0a, 0x19, 0x42, 0x61, 0x73, 0x65, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65,
	0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x1a, 0x0a, 0x08,
	0x73, 0x74, 0x72, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08,
	0x73, 0x74, 0x72, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x12, 0x18, 0x0a, 0x07, 0x73, 0x74, 0x61, 0x6d,
	0x69, 0x6e, 0x61, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x07, 0x73, 0x74, 0x61, 0x6d, 0x69,
	0x6e, 0x61, 0x12, 0x16, 0x0a, 0x06, 0x68, 0x65, 0x61, 0x6c, 0x74, 0x68, 0x18, 0x03, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x06, 0x68, 0x65, 0x61, 0x6c, 0x74, 0x68, 0x12, 0x12, 0x0a, 0x04, 0x6d, 0x61,
	0x6e, 0x61, 0x18, 0x04, 0x20, 0x01, 0x28, 0x04, 0x52, 0x04, 0x6d, 0x61, 0x6e, 0x61, 0x12, 0x1e,
	0x0a, 0x0a, 0x63, 0x72, 0x69, 0x74, 0x63, 0x68, 0x61, 0x6e, 0x63, 0x65, 0x18, 0x05, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x0a, 0x63, 0x72, 0x69, 0x74, 0x63, 0x68, 0x61, 0x6e, 0x63, 0x65, 0x12, 0x14,
	0x0a, 0x05, 0x61, 0x72, 0x6d, 0x6f, 0x72, 0x18, 0x06, 0x20, 0x01, 0x28, 0x04, 0x52, 0x05, 0x61,
	0x72, 0x6d, 0x6f, 0x72, 0x12, 0x1e, 0x0a, 0x0a, 0x72, 0x65, 0x73, 0x69, 0x73, 0x74, 0x61, 0x6e,
	0x63, 0x65, 0x18, 0x07, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0a, 0x72, 0x65, 0x73, 0x69, 0x73, 0x74,
	0x61, 0x6e, 0x63, 0x65, 0x22, 0x28, 0x0a, 0x10, 0x4c, 0x65, 0x76, 0x65, 0x6c, 0x50, 0x62, 0x43,
	0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x14, 0x0a, 0x05, 0x6c, 0x65, 0x76, 0x65,
	0x6c, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x05, 0x6c, 0x65, 0x76, 0x65, 0x6c, 0x22, 0x9b,
	0x01, 0x0a, 0x15, 0x41, 0x63, 0x74, 0x6f, 0x72, 0x53, 0x74, 0x61, 0x74, 0x65, 0x50, 0x62, 0x43,
	0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x44, 0x0a, 0x0a, 0x73, 0x74, 0x61, 0x74,
	0x65, 0x5f, 0x6c, 0x69, 0x73, 0x74, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x25, 0x2e, 0x41,
	0x63, 0x74, 0x6f, 0x72, 0x53, 0x74, 0x61, 0x74, 0x65, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x4c, 0x69, 0x73, 0x74, 0x45, 0x6e,
	0x74, 0x72, 0x79, 0x52, 0x09, 0x73, 0x74, 0x61, 0x74, 0x65, 0x4c, 0x69, 0x73, 0x74, 0x1a, 0x3c,
	0x0a, 0x0e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x4c, 0x69, 0x73, 0x74, 0x45, 0x6e, 0x74, 0x72, 0x79,
	0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b,
	0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28,
	0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x42, 0x09, 0x5a, 0x07,
	0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_component_actor_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_actor_comp_proto_rawDescData = file_proto_logic_component_actor_comp_proto_rawDesc
)

func file_proto_logic_component_actor_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_actor_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_actor_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_actor_comp_proto_rawDescData)
	})
	return file_proto_logic_component_actor_comp_proto_rawDescData
}

var file_proto_logic_component_actor_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 11)
var file_proto_logic_component_actor_comp_proto_goTypes = []any{
	(*Location)(nil),                  // 0: Location
	(*Rotation)(nil),                  // 1: Rotation
	(*Scale)(nil),                     // 2: Scale
	(*Transform)(nil),                 // 3: Transform
	(*Velocity)(nil),                  // 4: Velocity
	(*Acceleration)(nil),              // 5: Acceleration
	(*ViewRadius)(nil),                // 6: ViewRadius
	(*BaseAttributesPbComponent)(nil), // 7: BaseAttributesPbComponent
	(*LevelPbComponent)(nil),          // 8: LevelPbComponent
	(*ActorStatePbComponent)(nil),     // 9: ActorStatePbComponent
	nil,                               // 10: ActorStatePbComponent.StateListEntry
	(*Vector3)(nil),                   // 11: Vector3
}
var file_proto_logic_component_actor_comp_proto_depIdxs = []int32{
	11, // 0: Transform.location:type_name -> Vector3
	1,  // 1: Transform.rotation:type_name -> Rotation
	2,  // 2: Transform.scale:type_name -> Scale
	10, // 3: ActorStatePbComponent.state_list:type_name -> ActorStatePbComponent.StateListEntry
	4,  // [4:4] is the sub-list for method output_type
	4,  // [4:4] is the sub-list for method input_type
	4,  // [4:4] is the sub-list for extension type_name
	4,  // [4:4] is the sub-list for extension extendee
	0,  // [0:4] is the sub-list for field type_name
}

func init() { file_proto_logic_component_actor_comp_proto_init() }
func file_proto_logic_component_actor_comp_proto_init() {
	if File_proto_logic_component_actor_comp_proto != nil {
		return
	}
	file_proto_common_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_actor_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*Location); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*Rotation); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*Scale); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*Transform); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*Velocity); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*Acceleration); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[6].Exporter = func(v any, i int) any {
			switch v := v.(*ViewRadius); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[7].Exporter = func(v any, i int) any {
			switch v := v.(*BaseAttributesPbComponent); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[8].Exporter = func(v any, i int) any {
			switch v := v.(*LevelPbComponent); i {
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
		file_proto_logic_component_actor_comp_proto_msgTypes[9].Exporter = func(v any, i int) any {
			switch v := v.(*ActorStatePbComponent); i {
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
			RawDescriptor: file_proto_logic_component_actor_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   11,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_actor_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_actor_comp_proto_depIdxs,
		MessageInfos:      file_proto_logic_component_actor_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_actor_comp_proto = out.File
	file_proto_logic_component_actor_comp_proto_rawDesc = nil
	file_proto_logic_component_actor_comp_proto_goTypes = nil
	file_proto_logic_component_actor_comp_proto_depIdxs = nil
}
