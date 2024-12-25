// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: proto/logic/client_player/player_state_attribute_sync.proto

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

// 消息定义，用于同步实体的基础属性增量
type BaseAttributeSyncDataS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId         uint64                       `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
	Transform        *Transform                   `protobuf:"bytes,2,opt,name=transform,proto3" json:"transform,omitempty"`                // 速度信息
	Velocity         *Velocity                    `protobuf:"bytes,3,opt,name=velocity,proto3" json:"velocity,omitempty"`
	CombatStateFlags *CombatStateFlagsPbComponent `protobuf:"bytes,4,opt,name=combat_state_flags,json=combatStateFlags,proto3" json:"combat_state_flags,omitempty"` //战斗状态
}

func (x *BaseAttributeSyncDataS2C) Reset() {
	*x = BaseAttributeSyncDataS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *BaseAttributeSyncDataS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*BaseAttributeSyncDataS2C) ProtoMessage() {}

func (x *BaseAttributeSyncDataS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use BaseAttributeSyncDataS2C.ProtoReflect.Descriptor instead.
func (*BaseAttributeSyncDataS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{0}
}

func (x *BaseAttributeSyncDataS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

func (x *BaseAttributeSyncDataS2C) GetTransform() *Transform {
	if x != nil {
		return x.Transform
	}
	return nil
}

func (x *BaseAttributeSyncDataS2C) GetVelocity() *Velocity {
	if x != nil {
		return x.Velocity
	}
	return nil
}

func (x *BaseAttributeSyncDataS2C) GetCombatStateFlags() *CombatStateFlagsPbComponent {
	if x != nil {
		return x.CombatStateFlags
	}
	return nil
}

// 各种帧率同步消息
type AttributeDelta2FramesS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId uint64 `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
}

func (x *AttributeDelta2FramesS2C) Reset() {
	*x = AttributeDelta2FramesS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AttributeDelta2FramesS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AttributeDelta2FramesS2C) ProtoMessage() {}

func (x *AttributeDelta2FramesS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AttributeDelta2FramesS2C.ProtoReflect.Descriptor instead.
func (*AttributeDelta2FramesS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{1}
}

func (x *AttributeDelta2FramesS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

type AttributeDelta5FramesS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId uint64 `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
}

func (x *AttributeDelta5FramesS2C) Reset() {
	*x = AttributeDelta5FramesS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AttributeDelta5FramesS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AttributeDelta5FramesS2C) ProtoMessage() {}

func (x *AttributeDelta5FramesS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AttributeDelta5FramesS2C.ProtoReflect.Descriptor instead.
func (*AttributeDelta5FramesS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{2}
}

func (x *AttributeDelta5FramesS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

type AttributeDelta10FramesS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId uint64 `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
}

func (x *AttributeDelta10FramesS2C) Reset() {
	*x = AttributeDelta10FramesS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AttributeDelta10FramesS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AttributeDelta10FramesS2C) ProtoMessage() {}

func (x *AttributeDelta10FramesS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AttributeDelta10FramesS2C.ProtoReflect.Descriptor instead.
func (*AttributeDelta10FramesS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{3}
}

func (x *AttributeDelta10FramesS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

type AttributeDelta30FramesS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId uint64 `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
}

func (x *AttributeDelta30FramesS2C) Reset() {
	*x = AttributeDelta30FramesS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AttributeDelta30FramesS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AttributeDelta30FramesS2C) ProtoMessage() {}

func (x *AttributeDelta30FramesS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AttributeDelta30FramesS2C.ProtoReflect.Descriptor instead.
func (*AttributeDelta30FramesS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{4}
}

func (x *AttributeDelta30FramesS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

type AttributeDelta60FramesS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EntityId uint64 `protobuf:"varint,1,opt,name=entity_id,json=entityId,proto3" json:"entity_id,omitempty"` // 实体的唯一ID
}

func (x *AttributeDelta60FramesS2C) Reset() {
	*x = AttributeDelta60FramesS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AttributeDelta60FramesS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AttributeDelta60FramesS2C) ProtoMessage() {}

func (x *AttributeDelta60FramesS2C) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AttributeDelta60FramesS2C.ProtoReflect.Descriptor instead.
func (*AttributeDelta60FramesS2C) Descriptor() ([]byte, []int) {
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP(), []int{5}
}

func (x *AttributeDelta60FramesS2C) GetEntityId() uint64 {
	if x != nil {
		return x.EntityId
	}
	return 0
}

var File_proto_logic_client_player_player_state_attribute_sync_proto protoreflect.FileDescriptor

var file_proto_logic_client_player_player_state_attribute_sync_proto_rawDesc = []byte{
	0x0a, 0x3b, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6c,
	0x69, 0x65, 0x6e, 0x74, 0x5f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x2f, 0x70, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x61, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75,
	0x74, 0x65, 0x5f, 0x73, 0x79, 0x6e, 0x63, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x16, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x74, 0x69, 0x70, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x17, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d,
	0x6d, 0x6f, 0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x18,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x65, 0x6d, 0x70,
	0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x26, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f,
	0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x1a, 0x26, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f,
	0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x36, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f,
	0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x61, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x5f,
	0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x22, 0xd4, 0x01, 0x0a, 0x18, 0x42, 0x61, 0x73, 0x65, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75,
	0x74, 0x65, 0x53, 0x79, 0x6e, 0x63, 0x44, 0x61, 0x74, 0x61, 0x53, 0x32, 0x43, 0x12, 0x1b, 0x0a,
	0x09, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04,
	0x52, 0x08, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x49, 0x64, 0x12, 0x28, 0x0a, 0x09, 0x74, 0x72,
	0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0a, 0x2e,
	0x54, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x52, 0x09, 0x74, 0x72, 0x61, 0x6e, 0x73,
	0x66, 0x6f, 0x72, 0x6d, 0x12, 0x25, 0x0a, 0x08, 0x76, 0x65, 0x6c, 0x6f, 0x63, 0x69, 0x74, 0x79,
	0x18, 0x03, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x09, 0x2e, 0x56, 0x65, 0x6c, 0x6f, 0x63, 0x69, 0x74,
	0x79, 0x52, 0x08, 0x76, 0x65, 0x6c, 0x6f, 0x63, 0x69, 0x74, 0x79, 0x12, 0x4a, 0x0a, 0x12, 0x63,
	0x6f, 0x6d, 0x62, 0x61, 0x74, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x66, 0x6c, 0x61, 0x67,
	0x73, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x1c, 0x2e, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74,
	0x53, 0x74, 0x61, 0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70,
	0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x52, 0x10, 0x63, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53, 0x74, 0x61,
	0x74, 0x65, 0x46, 0x6c, 0x61, 0x67, 0x73, 0x22, 0x37, 0x0a, 0x18, 0x41, 0x74, 0x74, 0x72, 0x69,
	0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x32, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73,
	0x53, 0x32, 0x43, 0x12, 0x1b, 0x0a, 0x09, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x49, 0x64,
	0x22, 0x37, 0x0a, 0x18, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c,
	0x74, 0x61, 0x35, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x12, 0x1b, 0x0a, 0x09,
	0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52,
	0x08, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x49, 0x64, 0x22, 0x38, 0x0a, 0x19, 0x41, 0x74, 0x74,
	0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x31, 0x30, 0x46, 0x72, 0x61,
	0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x12, 0x1b, 0x0a, 0x09, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79,
	0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x65, 0x6e, 0x74, 0x69, 0x74,
	0x79, 0x49, 0x64, 0x22, 0x38, 0x0a, 0x19, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65,
	0x44, 0x65, 0x6c, 0x74, 0x61, 0x33, 0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43,
	0x12, 0x1b, 0x0a, 0x09, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x08, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x49, 0x64, 0x22, 0x38, 0x0a,
	0x19, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x36,
	0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x12, 0x1b, 0x0a, 0x09, 0x65, 0x6e,
	0x74, 0x69, 0x74, 0x79, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x65,
	0x6e, 0x74, 0x69, 0x74, 0x79, 0x49, 0x64, 0x32, 0xf8, 0x02, 0x0a, 0x11, 0x45, 0x6e, 0x74, 0x69,
	0x74, 0x79, 0x53, 0x79, 0x6e, 0x63, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x36, 0x0a,
	0x11, 0x53, 0x79, 0x6e, 0x63, 0x42, 0x61, 0x73, 0x65, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75,
	0x74, 0x65, 0x12, 0x19, 0x2e, 0x42, 0x61, 0x73, 0x65, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75,
	0x74, 0x65, 0x53, 0x79, 0x6e, 0x63, 0x44, 0x61, 0x74, 0x61, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x39, 0x0a, 0x14, 0x53, 0x79, 0x6e, 0x63, 0x41, 0x74, 0x74,
	0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x32, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x12, 0x19, 0x2e,
	0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x32, 0x46,
	0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79,
	0x12, 0x39, 0x0a, 0x14, 0x53, 0x79, 0x6e, 0x63, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74,
	0x65, 0x35, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x12, 0x19, 0x2e, 0x41, 0x74, 0x74, 0x72, 0x69,
	0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x35, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73,
	0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3b, 0x0a, 0x15, 0x53,
	0x79, 0x6e, 0x63, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x31, 0x30, 0x46, 0x72,
	0x61, 0x6d, 0x65, 0x73, 0x12, 0x1a, 0x2e, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65,
	0x44, 0x65, 0x6c, 0x74, 0x61, 0x31, 0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43,
	0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3b, 0x0a, 0x15, 0x53, 0x79, 0x6e, 0x63,
	0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x33, 0x30, 0x46, 0x72, 0x61, 0x6d, 0x65,
	0x73, 0x12, 0x1a, 0x2e, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c,
	0x74, 0x61, 0x33, 0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3b, 0x0a, 0x15, 0x53, 0x79, 0x6e, 0x63, 0x41, 0x74, 0x74,
	0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x36, 0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x12, 0x1a,
	0x2e, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x36,
	0x30, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70,
	0x74, 0x79, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x80, 0x01, 0x01,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescOnce sync.Once
	file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescData = file_proto_logic_client_player_player_state_attribute_sync_proto_rawDesc
)

func file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescGZIP() []byte {
	file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescOnce.Do(func() {
		file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescData)
	})
	return file_proto_logic_client_player_player_state_attribute_sync_proto_rawDescData
}

var file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes = make([]protoimpl.MessageInfo, 6)
var file_proto_logic_client_player_player_state_attribute_sync_proto_goTypes = []any{
	(*BaseAttributeSyncDataS2C)(nil),    // 0: BaseAttributeSyncDataS2C
	(*AttributeDelta2FramesS2C)(nil),    // 1: AttributeDelta2FramesS2C
	(*AttributeDelta5FramesS2C)(nil),    // 2: AttributeDelta5FramesS2C
	(*AttributeDelta10FramesS2C)(nil),   // 3: AttributeDelta10FramesS2C
	(*AttributeDelta30FramesS2C)(nil),   // 4: AttributeDelta30FramesS2C
	(*AttributeDelta60FramesS2C)(nil),   // 5: AttributeDelta60FramesS2C
	(*Transform)(nil),                   // 6: Transform
	(*Velocity)(nil),                    // 7: Velocity
	(*CombatStateFlagsPbComponent)(nil), // 8: CombatStateFlagsPbComponent
	(*Empty)(nil),                       // 9: Empty
}
var file_proto_logic_client_player_player_state_attribute_sync_proto_depIdxs = []int32{
	6, // 0: BaseAttributeSyncDataS2C.transform:type_name -> Transform
	7, // 1: BaseAttributeSyncDataS2C.velocity:type_name -> Velocity
	8, // 2: BaseAttributeSyncDataS2C.combat_state_flags:type_name -> CombatStateFlagsPbComponent
	0, // 3: EntitySyncService.SyncBaseAttribute:input_type -> BaseAttributeSyncDataS2C
	1, // 4: EntitySyncService.SyncAttribute2Frames:input_type -> AttributeDelta2FramesS2C
	2, // 5: EntitySyncService.SyncAttribute5Frames:input_type -> AttributeDelta5FramesS2C
	3, // 6: EntitySyncService.SyncAttribute10Frames:input_type -> AttributeDelta10FramesS2C
	4, // 7: EntitySyncService.SyncAttribute30Frames:input_type -> AttributeDelta30FramesS2C
	5, // 8: EntitySyncService.SyncAttribute60Frames:input_type -> AttributeDelta60FramesS2C
	9, // 9: EntitySyncService.SyncBaseAttribute:output_type -> Empty
	9, // 10: EntitySyncService.SyncAttribute2Frames:output_type -> Empty
	9, // 11: EntitySyncService.SyncAttribute5Frames:output_type -> Empty
	9, // 12: EntitySyncService.SyncAttribute10Frames:output_type -> Empty
	9, // 13: EntitySyncService.SyncAttribute30Frames:output_type -> Empty
	9, // 14: EntitySyncService.SyncAttribute60Frames:output_type -> Empty
	9, // [9:15] is the sub-list for method output_type
	3, // [3:9] is the sub-list for method input_type
	3, // [3:3] is the sub-list for extension type_name
	3, // [3:3] is the sub-list for extension extendee
	0, // [0:3] is the sub-list for field type_name
}

func init() { file_proto_logic_client_player_player_state_attribute_sync_proto_init() }
func file_proto_logic_client_player_player_state_attribute_sync_proto_init() {
	if File_proto_logic_client_player_player_state_attribute_sync_proto != nil {
		return
	}
	file_proto_common_tip_proto_init()
	file_proto_common_comp_proto_init()
	file_proto_common_empty_proto_init()
	file_proto_logic_component_skill_comp_proto_init()
	file_proto_logic_component_actor_comp_proto_init()
	file_proto_logic_component_actor_attribute_state_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*BaseAttributeSyncDataS2C); i {
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
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*AttributeDelta2FramesS2C); i {
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
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*AttributeDelta5FramesS2C); i {
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
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*AttributeDelta10FramesS2C); i {
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
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*AttributeDelta30FramesS2C); i {
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
		file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*AttributeDelta60FramesS2C); i {
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
			RawDescriptor: file_proto_logic_client_player_player_state_attribute_sync_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   6,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_proto_logic_client_player_player_state_attribute_sync_proto_goTypes,
		DependencyIndexes: file_proto_logic_client_player_player_state_attribute_sync_proto_depIdxs,
		MessageInfos:      file_proto_logic_client_player_player_state_attribute_sync_proto_msgTypes,
	}.Build()
	File_proto_logic_client_player_player_state_attribute_sync_proto = out.File
	file_proto_logic_client_player_player_state_attribute_sync_proto_rawDesc = nil
	file_proto_logic_client_player_player_state_attribute_sync_proto_goTypes = nil
	file_proto_logic_client_player_player_state_attribute_sync_proto_depIdxs = nil
}
