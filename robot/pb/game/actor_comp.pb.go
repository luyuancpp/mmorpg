// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/logic/component/actor_comp.proto

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

type Location struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	X             float64                `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y             float64                `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z             float64                `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Location) Reset() {
	*x = Location{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Location) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Location) ProtoMessage() {}

func (x *Location) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[0]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	X             float64                `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y             float64                `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z             float64                `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Rotation) Reset() {
	*x = Rotation{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[1]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Rotation) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Rotation) ProtoMessage() {}

func (x *Rotation) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[1]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	X             float64                `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y             float64                `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z             float64                `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Scale) Reset() {
	*x = Scale{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[2]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Scale) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Scale) ProtoMessage() {}

func (x *Scale) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[2]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	Location      *Vector3               `protobuf:"bytes,1,opt,name=location,proto3" json:"location,omitempty"`
	Rotation      *Rotation              `protobuf:"bytes,2,opt,name=rotation,proto3" json:"rotation,omitempty"`
	Scale         *Scale                 `protobuf:"bytes,3,opt,name=scale,proto3" json:"scale,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Transform) Reset() {
	*x = Transform{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[3]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Transform) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Transform) ProtoMessage() {}

func (x *Transform) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[3]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	X             float64                `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y             float64                `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z             float64                `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Velocity) Reset() {
	*x = Velocity{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[4]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Velocity) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Velocity) ProtoMessage() {}

func (x *Velocity) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[4]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	X             float64                `protobuf:"fixed64,1,opt,name=x,proto3" json:"x,omitempty"`
	Y             float64                `protobuf:"fixed64,2,opt,name=y,proto3" json:"y,omitempty"`
	Z             float64                `protobuf:"fixed64,3,opt,name=z,proto3" json:"z,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *Acceleration) Reset() {
	*x = Acceleration{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[5]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *Acceleration) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Acceleration) ProtoMessage() {}

func (x *Acceleration) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[5]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	Radius        float64                `protobuf:"fixed64,1,opt,name=radius,proto3" json:"radius,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *ViewRadius) Reset() {
	*x = ViewRadius{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[6]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *ViewRadius) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ViewRadius) ProtoMessage() {}

func (x *ViewRadius) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[6]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	Strength      uint64                 `protobuf:"varint,1,opt,name=strength,proto3" json:"strength,omitempty"`     // 力量，影响物理攻击
	Stamina       uint64                 `protobuf:"varint,2,opt,name=stamina,proto3" json:"stamina,omitempty"`       // 耐力，影响生命值等
	Health        uint64                 `protobuf:"varint,3,opt,name=health,proto3" json:"health,omitempty"`         // 当前生命值
	Mana          uint64                 `protobuf:"varint,4,opt,name=mana,proto3" json:"mana,omitempty"`             // 当前法力值
	Critchance    uint64                 `protobuf:"varint,5,opt,name=critchance,proto3" json:"critchance,omitempty"` // 暴击几率
	Armor         uint64                 `protobuf:"varint,6,opt,name=armor,proto3" json:"armor,omitempty"`           // 护甲
	Resistance    uint64                 `protobuf:"varint,7,opt,name=resistance,proto3" json:"resistance,omitempty"` // 抗性
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *BaseAttributesPbComponent) Reset() {
	*x = BaseAttributesPbComponent{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[7]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *BaseAttributesPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BaseAttributesPbComponent) ProtoMessage() {}

func (x *BaseAttributesPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[7]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	Level         uint32                 `protobuf:"varint,1,opt,name=level,proto3" json:"level,omitempty"` // 玩家等级
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *LevelPbComponent) Reset() {
	*x = LevelPbComponent{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[8]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *LevelPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LevelPbComponent) ProtoMessage() {}

func (x *LevelPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[8]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	StateList     map[uint32]bool        `protobuf:"bytes,1,rep,name=state_list,json=stateList,proto3" json:"state_list,omitempty" protobuf_key:"varint,1,opt,name=key" protobuf_val:"varint,2,opt,name=value"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *ActorStatePbComponent) Reset() {
	*x = ActorStatePbComponent{}
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[9]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *ActorStatePbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ActorStatePbComponent) ProtoMessage() {}

func (x *ActorStatePbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_comp_proto_msgTypes[9]
	if x != nil {
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

const file_proto_logic_component_actor_comp_proto_rawDesc = "" +
	"\n" +
	"&proto/logic/component/actor_comp.proto\x1a proto/logic/component/comp.proto\"4\n" +
	"\bLocation\x12\f\n" +
	"\x01x\x18\x01 \x01(\x01R\x01x\x12\f\n" +
	"\x01y\x18\x02 \x01(\x01R\x01y\x12\f\n" +
	"\x01z\x18\x03 \x01(\x01R\x01z\"4\n" +
	"\bRotation\x12\f\n" +
	"\x01x\x18\x01 \x01(\x01R\x01x\x12\f\n" +
	"\x01y\x18\x02 \x01(\x01R\x01y\x12\f\n" +
	"\x01z\x18\x03 \x01(\x01R\x01z\"1\n" +
	"\x05Scale\x12\f\n" +
	"\x01x\x18\x01 \x01(\x01R\x01x\x12\f\n" +
	"\x01y\x18\x02 \x01(\x01R\x01y\x12\f\n" +
	"\x01z\x18\x03 \x01(\x01R\x01z\"v\n" +
	"\tTransform\x12$\n" +
	"\blocation\x18\x01 \x01(\v2\b.Vector3R\blocation\x12%\n" +
	"\brotation\x18\x02 \x01(\v2\t.RotationR\brotation\x12\x1c\n" +
	"\x05scale\x18\x03 \x01(\v2\x06.ScaleR\x05scale\"4\n" +
	"\bVelocity\x12\f\n" +
	"\x01x\x18\x01 \x01(\x01R\x01x\x12\f\n" +
	"\x01y\x18\x02 \x01(\x01R\x01y\x12\f\n" +
	"\x01z\x18\x03 \x01(\x01R\x01z\"8\n" +
	"\fAcceleration\x12\f\n" +
	"\x01x\x18\x01 \x01(\x01R\x01x\x12\f\n" +
	"\x01y\x18\x02 \x01(\x01R\x01y\x12\f\n" +
	"\x01z\x18\x03 \x01(\x01R\x01z\"$\n" +
	"\n" +
	"ViewRadius\x12\x16\n" +
	"\x06radius\x18\x01 \x01(\x01R\x06radius\"\xd3\x01\n" +
	"\x19BaseAttributesPbComponent\x12\x1a\n" +
	"\bstrength\x18\x01 \x01(\x04R\bstrength\x12\x18\n" +
	"\astamina\x18\x02 \x01(\x04R\astamina\x12\x16\n" +
	"\x06health\x18\x03 \x01(\x04R\x06health\x12\x12\n" +
	"\x04mana\x18\x04 \x01(\x04R\x04mana\x12\x1e\n" +
	"\n" +
	"critchance\x18\x05 \x01(\x04R\n" +
	"critchance\x12\x14\n" +
	"\x05armor\x18\x06 \x01(\x04R\x05armor\x12\x1e\n" +
	"\n" +
	"resistance\x18\a \x01(\x04R\n" +
	"resistance\"(\n" +
	"\x10LevelPbComponent\x12\x14\n" +
	"\x05level\x18\x01 \x01(\rR\x05level\"\x9b\x01\n" +
	"\x15ActorStatePbComponent\x12D\n" +
	"\n" +
	"state_list\x18\x01 \x03(\v2%.ActorStatePbComponent.StateListEntryR\tstateList\x1a<\n" +
	"\x0eStateListEntry\x12\x10\n" +
	"\x03key\x18\x01 \x01(\rR\x03key\x12\x14\n" +
	"\x05value\x18\x02 \x01(\bR\x05value:\x028\x01B\tZ\apb/gameb\x06proto3"

var (
	file_proto_logic_component_actor_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_actor_comp_proto_rawDescData []byte
)

func file_proto_logic_component_actor_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_actor_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_actor_comp_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_logic_component_actor_comp_proto_rawDesc), len(file_proto_logic_component_actor_comp_proto_rawDesc)))
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
	file_proto_logic_component_comp_proto_init()
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_logic_component_actor_comp_proto_rawDesc), len(file_proto_logic_component_actor_comp_proto_rawDesc)),
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
	file_proto_logic_component_actor_comp_proto_goTypes = nil
	file_proto_logic_component_actor_comp_proto_depIdxs = nil
}
