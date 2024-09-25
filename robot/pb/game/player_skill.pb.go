// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/client_player/player_skill.proto

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

type ReleaseSkillSkillRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SkillTableId uint32    `protobuf:"varint,1,opt,name=skill_table_id,json=skillTableId,proto3" json:"skill_table_id,omitempty"`
	TargetId     uint64    `protobuf:"varint,2,opt,name=target_id,json=targetId,proto3" json:"target_id,omitempty"`
	Position     *Vector3  `protobuf:"bytes,3,opt,name=position,proto3" json:"position,omitempty"`
	Rotation     *Rotation `protobuf:"bytes,4,opt,name=rotation,proto3" json:"rotation,omitempty"`
}

func (x *ReleaseSkillSkillRequest) Reset() {
	*x = ReleaseSkillSkillRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ReleaseSkillSkillRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ReleaseSkillSkillRequest) ProtoMessage() {}

func (x *ReleaseSkillSkillRequest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ReleaseSkillSkillRequest.ProtoReflect.Descriptor instead.
func (*ReleaseSkillSkillRequest) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{0}
}

func (x *ReleaseSkillSkillRequest) GetSkillTableId() uint32 {
	if x != nil {
		return x.SkillTableId
	}
	return 0
}

func (x *ReleaseSkillSkillRequest) GetTargetId() uint64 {
	if x != nil {
		return x.TargetId
	}
	return 0
}

func (x *ReleaseSkillSkillRequest) GetPosition() *Vector3 {
	if x != nil {
		return x.Position
	}
	return nil
}

func (x *ReleaseSkillSkillRequest) GetRotation() *Rotation {
	if x != nil {
		return x.Rotation
	}
	return nil
}

type ReleaseSkillSkillResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
}

func (x *ReleaseSkillSkillResponse) Reset() {
	*x = ReleaseSkillSkillResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ReleaseSkillSkillResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ReleaseSkillSkillResponse) ProtoMessage() {}

func (x *ReleaseSkillSkillResponse) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ReleaseSkillSkillResponse.ProtoReflect.Descriptor instead.
func (*ReleaseSkillSkillResponse) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{1}
}

func (x *ReleaseSkillSkillResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

type SkillInterruptedS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity       uint64 `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
	TargetEntity uint64 `protobuf:"varint,2,opt,name=target_entity,json=targetEntity,proto3" json:"target_entity,omitempty"`
	SkillTableId uint32 `protobuf:"varint,3,opt,name=skill_table_id,json=skillTableId,proto3" json:"skill_table_id,omitempty"`
	ReasonCode   uint32 `protobuf:"varint,4,opt,name=reason_code,json=reasonCode,proto3" json:"reason_code,omitempty"`
}

func (x *SkillInterruptedS2C) Reset() {
	*x = SkillInterruptedS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillInterruptedS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillInterruptedS2C) ProtoMessage() {}

func (x *SkillInterruptedS2C) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillInterruptedS2C.ProtoReflect.Descriptor instead.
func (*SkillInterruptedS2C) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{2}
}

func (x *SkillInterruptedS2C) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

func (x *SkillInterruptedS2C) GetTargetEntity() uint64 {
	if x != nil {
		return x.TargetEntity
	}
	return 0
}

func (x *SkillInterruptedS2C) GetSkillTableId() uint32 {
	if x != nil {
		return x.SkillTableId
	}
	return 0
}

func (x *SkillInterruptedS2C) GetReasonCode() uint32 {
	if x != nil {
		return x.ReasonCode
	}
	return 0
}

type SkillUsedS2C struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Entity       uint64   `protobuf:"varint,1,opt,name=entity,proto3" json:"entity,omitempty"`
	TargetEntity []uint64 `protobuf:"varint,2,rep,packed,name=target_entity,json=targetEntity,proto3" json:"target_entity,omitempty"`
	SkillTableId uint32   `protobuf:"varint,3,opt,name=skill_table_id,json=skillTableId,proto3" json:"skill_table_id,omitempty"`
	Position     *Vector3 `protobuf:"bytes,4,opt,name=position,proto3" json:"position,omitempty"`
	TimeStamp    uint64   `protobuf:"varint,5,opt,name=time_stamp,json=timeStamp,proto3" json:"time_stamp,omitempty"`
}

func (x *SkillUsedS2C) Reset() {
	*x = SkillUsedS2C{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillUsedS2C) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillUsedS2C) ProtoMessage() {}

func (x *SkillUsedS2C) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillUsedS2C.ProtoReflect.Descriptor instead.
func (*SkillUsedS2C) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{3}
}

func (x *SkillUsedS2C) GetEntity() uint64 {
	if x != nil {
		return x.Entity
	}
	return 0
}

func (x *SkillUsedS2C) GetTargetEntity() []uint64 {
	if x != nil {
		return x.TargetEntity
	}
	return nil
}

func (x *SkillUsedS2C) GetSkillTableId() uint32 {
	if x != nil {
		return x.SkillTableId
	}
	return 0
}

func (x *SkillUsedS2C) GetPosition() *Vector3 {
	if x != nil {
		return x.Position
	}
	return nil
}

func (x *SkillUsedS2C) GetTimeStamp() uint64 {
	if x != nil {
		return x.TimeStamp
	}
	return 0
}

// 获取技能列表请求
type GetSkillListRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *GetSkillListRequest) Reset() {
	*x = GetSkillListRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetSkillListRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetSkillListRequest) ProtoMessage() {}

func (x *GetSkillListRequest) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GetSkillListRequest.ProtoReflect.Descriptor instead.
func (*GetSkillListRequest) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{4}
}

// 获取技能列表响应
type GetSkillListResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage             `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
	SkillList    *PlayerSkillListPBComponent `protobuf:"bytes,2,opt,name=skill_list,json=skillList,proto3" json:"skill_list,omitempty"`
}

func (x *GetSkillListResponse) Reset() {
	*x = GetSkillListResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_client_player_player_skill_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetSkillListResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetSkillListResponse) ProtoMessage() {}

func (x *GetSkillListResponse) ProtoReflect() protoreflect.Message {
	mi := &file_logic_client_player_player_skill_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GetSkillListResponse.ProtoReflect.Descriptor instead.
func (*GetSkillListResponse) Descriptor() ([]byte, []int) {
	return file_logic_client_player_player_skill_proto_rawDescGZIP(), []int{5}
}

func (x *GetSkillListResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

func (x *GetSkillListResponse) GetSkillList() *PlayerSkillListPBComponent {
	if x != nil {
		return x.SkillList
	}
	return nil
}

var File_logic_client_player_player_skill_proto protoreflect.FileDescriptor

var file_logic_client_player_player_skill_proto_rawDesc = []byte{
	0x0a, 0x26, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x5f, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x73, 0x6b, 0x69,
	0x6c, 0x6c, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x10, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e,
	0x2f, 0x74, 0x69, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x11, 0x63, 0x6f, 0x6d, 0x6d,
	0x6f, 0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x12, 0x63,
	0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x1a, 0x27, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65,
	0x6e, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f,
	0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x6c, 0x6f, 0x67, 0x69,
	0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x73, 0x6b, 0x69, 0x6c,
	0x6c, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x6c, 0x6f,
	0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x61, 0x63,
	0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0xaa,
	0x01, 0x0a, 0x18, 0x52, 0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x53,
	0x6b, 0x69, 0x6c, 0x6c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x24, 0x0a, 0x0e, 0x73,
	0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x0c, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49,
	0x64, 0x12, 0x1b, 0x0a, 0x09, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x5f, 0x69, 0x64, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x49, 0x64, 0x12, 0x24,
	0x0a, 0x08, 0x70, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0b,
	0x32, 0x08, 0x2e, 0x56, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x33, 0x52, 0x08, 0x70, 0x6f, 0x73, 0x69,
	0x74, 0x69, 0x6f, 0x6e, 0x12, 0x25, 0x0a, 0x08, 0x72, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e,
	0x18, 0x04, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x09, 0x2e, 0x52, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f,
	0x6e, 0x52, 0x08, 0x72, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x22, 0x51, 0x0a, 0x19, 0x52,
	0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x53, 0x6b, 0x69, 0x6c, 0x6c,
	0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a, 0x0d, 0x65, 0x72, 0x72, 0x6f,
	0x72, 0x5f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32,
	0x0f, 0x2e, 0x54, 0x69, 0x70, 0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x52, 0x0c, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x22, 0x99,
	0x01, 0x0a, 0x13, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70,
	0x74, 0x65, 0x64, 0x53, 0x32, 0x43, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x12, 0x23,
	0x0a, 0x0d, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x5f, 0x65, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x18,
	0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x0c, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x45, 0x6e, 0x74,
	0x69, 0x74, 0x79, 0x12, 0x24, 0x0a, 0x0e, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x74, 0x61, 0x62,
	0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x73, 0x6b, 0x69,
	0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64, 0x12, 0x1f, 0x0a, 0x0b, 0x72, 0x65, 0x61,
	0x73, 0x6f, 0x6e, 0x5f, 0x63, 0x6f, 0x64, 0x65, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a,
	0x72, 0x65, 0x61, 0x73, 0x6f, 0x6e, 0x43, 0x6f, 0x64, 0x65, 0x22, 0xb6, 0x01, 0x0a, 0x0c, 0x53,
	0x6b, 0x69, 0x6c, 0x6c, 0x55, 0x73, 0x65, 0x64, 0x53, 0x32, 0x43, 0x12, 0x16, 0x0a, 0x06, 0x65,
	0x6e, 0x74, 0x69, 0x74, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x06, 0x65, 0x6e, 0x74,
	0x69, 0x74, 0x79, 0x12, 0x23, 0x0a, 0x0d, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x5f, 0x65, 0x6e,
	0x74, 0x69, 0x74, 0x79, 0x18, 0x02, 0x20, 0x03, 0x28, 0x04, 0x52, 0x0c, 0x74, 0x61, 0x72, 0x67,
	0x65, 0x74, 0x45, 0x6e, 0x74, 0x69, 0x74, 0x79, 0x12, 0x24, 0x0a, 0x0e, 0x73, 0x6b, 0x69, 0x6c,
	0x6c, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d,
	0x52, 0x0c, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64, 0x12, 0x24,
	0x0a, 0x08, 0x70, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0b,
	0x32, 0x08, 0x2e, 0x56, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x33, 0x52, 0x08, 0x70, 0x6f, 0x73, 0x69,
	0x74, 0x69, 0x6f, 0x6e, 0x12, 0x1d, 0x0a, 0x0a, 0x74, 0x69, 0x6d, 0x65, 0x5f, 0x73, 0x74, 0x61,
	0x6d, 0x70, 0x18, 0x05, 0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x74, 0x69, 0x6d, 0x65, 0x53, 0x74,
	0x61, 0x6d, 0x70, 0x22, 0x15, 0x0a, 0x13, 0x47, 0x65, 0x74, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x4c,
	0x69, 0x73, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x22, 0x88, 0x01, 0x0a, 0x14, 0x47,
	0x65, 0x74, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x4c, 0x69, 0x73, 0x74, 0x52, 0x65, 0x73, 0x70, 0x6f,
	0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a, 0x0d, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x6d, 0x65, 0x73,
	0x73, 0x61, 0x67, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x54, 0x69, 0x70,
	0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x0c, 0x65, 0x72, 0x72,
	0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x12, 0x3a, 0x0a, 0x0a, 0x73, 0x6b, 0x69,
	0x6c, 0x6c, 0x5f, 0x6c, 0x69, 0x73, 0x74, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x1b, 0x2e,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x4c, 0x69, 0x73, 0x74, 0x50,
	0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x52, 0x09, 0x73, 0x6b, 0x69, 0x6c,
	0x6c, 0x4c, 0x69, 0x73, 0x74, 0x32, 0xfa, 0x01, 0x0a, 0x12, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x45, 0x0a, 0x0c,
	0x52, 0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x12, 0x19, 0x2e, 0x52,
	0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x53, 0x6b, 0x69, 0x6c, 0x6c,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x1a, 0x2e, 0x52, 0x65, 0x6c, 0x65, 0x61, 0x73,
	0x65, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x52, 0x65, 0x73, 0x70, 0x6f,
	0x6e, 0x73, 0x65, 0x12, 0x28, 0x0a, 0x0f, 0x4e, 0x6f, 0x74, 0x69, 0x66, 0x79, 0x53, 0x6b, 0x69,
	0x6c, 0x6c, 0x55, 0x73, 0x65, 0x64, 0x12, 0x0d, 0x2e, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x55, 0x73,
	0x65, 0x64, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x36, 0x0a,
	0x16, 0x4e, 0x6f, 0x74, 0x69, 0x66, 0x79, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49, 0x6e, 0x74, 0x65,
	0x72, 0x72, 0x75, 0x70, 0x74, 0x65, 0x64, 0x12, 0x14, 0x2e, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49,
	0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x65, 0x64, 0x53, 0x32, 0x43, 0x1a, 0x06, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3b, 0x0a, 0x0c, 0x47, 0x65, 0x74, 0x53, 0x6b, 0x69, 0x6c,
	0x6c, 0x4c, 0x69, 0x73, 0x74, 0x12, 0x14, 0x2e, 0x47, 0x65, 0x74, 0x53, 0x6b, 0x69, 0x6c, 0x6c,
	0x4c, 0x69, 0x73, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x15, 0x2e, 0x47, 0x65,
	0x74, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x4c, 0x69, 0x73, 0x74, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e,
	0x73, 0x65, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x80, 0x01, 0x01,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_client_player_player_skill_proto_rawDescOnce sync.Once
	file_logic_client_player_player_skill_proto_rawDescData = file_logic_client_player_player_skill_proto_rawDesc
)

func file_logic_client_player_player_skill_proto_rawDescGZIP() []byte {
	file_logic_client_player_player_skill_proto_rawDescOnce.Do(func() {
		file_logic_client_player_player_skill_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_client_player_player_skill_proto_rawDescData)
	})
	return file_logic_client_player_player_skill_proto_rawDescData
}

var file_logic_client_player_player_skill_proto_msgTypes = make([]protoimpl.MessageInfo, 6)
var file_logic_client_player_player_skill_proto_goTypes = []any{
	(*ReleaseSkillSkillRequest)(nil),   // 0: ReleaseSkillSkillRequest
	(*ReleaseSkillSkillResponse)(nil),  // 1: ReleaseSkillSkillResponse
	(*SkillInterruptedS2C)(nil),        // 2: SkillInterruptedS2C
	(*SkillUsedS2C)(nil),               // 3: SkillUsedS2C
	(*GetSkillListRequest)(nil),        // 4: GetSkillListRequest
	(*GetSkillListResponse)(nil),       // 5: GetSkillListResponse
	(*Vector3)(nil),                    // 6: Vector3
	(*Rotation)(nil),                   // 7: Rotation
	(*TipInfoMessage)(nil),             // 8: TipInfoMessage
	(*PlayerSkillListPBComponent)(nil), // 9: PlayerSkillListPBComponent
	(*Empty)(nil),                      // 10: Empty
}
var file_logic_client_player_player_skill_proto_depIdxs = []int32{
	6,  // 0: ReleaseSkillSkillRequest.position:type_name -> Vector3
	7,  // 1: ReleaseSkillSkillRequest.rotation:type_name -> Rotation
	8,  // 2: ReleaseSkillSkillResponse.error_message:type_name -> TipInfoMessage
	6,  // 3: SkillUsedS2C.position:type_name -> Vector3
	8,  // 4: GetSkillListResponse.error_message:type_name -> TipInfoMessage
	9,  // 5: GetSkillListResponse.skill_list:type_name -> PlayerSkillListPBComponent
	0,  // 6: PlayerSkillService.ReleaseSkill:input_type -> ReleaseSkillSkillRequest
	3,  // 7: PlayerSkillService.NotifySkillUsed:input_type -> SkillUsedS2C
	2,  // 8: PlayerSkillService.NotifySkillInterrupted:input_type -> SkillInterruptedS2C
	4,  // 9: PlayerSkillService.GetSkillList:input_type -> GetSkillListRequest
	1,  // 10: PlayerSkillService.ReleaseSkill:output_type -> ReleaseSkillSkillResponse
	10, // 11: PlayerSkillService.NotifySkillUsed:output_type -> Empty
	10, // 12: PlayerSkillService.NotifySkillInterrupted:output_type -> Empty
	5,  // 13: PlayerSkillService.GetSkillList:output_type -> GetSkillListResponse
	10, // [10:14] is the sub-list for method output_type
	6,  // [6:10] is the sub-list for method input_type
	6,  // [6:6] is the sub-list for extension type_name
	6,  // [6:6] is the sub-list for extension extendee
	0,  // [0:6] is the sub-list for field type_name
}

func init() { file_logic_client_player_player_skill_proto_init() }
func file_logic_client_player_player_skill_proto_init() {
	if File_logic_client_player_player_skill_proto != nil {
		return
	}
	file_common_tip_proto_init()
	file_common_comp_proto_init()
	file_common_empty_proto_init()
	file_logic_component_player_skill_comp_proto_init()
	file_logic_component_skill_comp_proto_init()
	file_logic_component_actor_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_logic_client_player_player_skill_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*ReleaseSkillSkillRequest); i {
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
		file_logic_client_player_player_skill_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*ReleaseSkillSkillResponse); i {
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
		file_logic_client_player_player_skill_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*SkillInterruptedS2C); i {
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
		file_logic_client_player_player_skill_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*SkillUsedS2C); i {
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
		file_logic_client_player_player_skill_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*GetSkillListRequest); i {
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
		file_logic_client_player_player_skill_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*GetSkillListResponse); i {
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
			RawDescriptor: file_logic_client_player_player_skill_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   6,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_logic_client_player_player_skill_proto_goTypes,
		DependencyIndexes: file_logic_client_player_player_skill_proto_depIdxs,
		MessageInfos:      file_logic_client_player_player_skill_proto_msgTypes,
	}.Build()
	File_logic_client_player_player_skill_proto = out.File
	file_logic_client_player_player_skill_proto_rawDesc = nil
	file_logic_client_player_player_skill_proto_goTypes = nil
	file_logic_client_player_player_skill_proto_depIdxs = nil
}