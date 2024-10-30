// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: logic/component/mission_comp.proto

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

type MissionPBComponentEMissionStatus int32

const (
	MissionPBComponent_E_MISSION_NORMAL      MissionPBComponentEMissionStatus = 0
	MissionPBComponent_E_MISSION_ACHIEVEMENT MissionPBComponentEMissionStatus = 1
	MissionPBComponent_E_MISSION_COMPLETE    MissionPBComponentEMissionStatus = 2
	MissionPBComponent_E_MISSION_TIME_OUT    MissionPBComponentEMissionStatus = 3
	MissionPBComponent_E_MISSION_FAILD       MissionPBComponentEMissionStatus = 4
)

// Enum value maps for MissionPBComponentEMissionStatus.
var (
	MissionPBComponentEMissionStatus_name = map[int32]string{
		0: "E_MISSION_NORMAL",
		1: "E_MISSION_ACHIEVEMENT",
		2: "E_MISSION_COMPLETE",
		3: "E_MISSION_TIME_OUT",
		4: "E_MISSION_FAILD",
	}
	MissionPBComponentEMissionStatus_value = map[string]int32{
		"E_MISSION_NORMAL":      0,
		"E_MISSION_ACHIEVEMENT": 1,
		"E_MISSION_COMPLETE":    2,
		"E_MISSION_TIME_OUT":    3,
		"E_MISSION_FAILD":       4,
	}
)

func (x MissionPBComponentEMissionStatus) Enum() *MissionPBComponentEMissionStatus {
	p := new(MissionPBComponentEMissionStatus)
	*p = x
	return p
}

func (x MissionPBComponentEMissionStatus) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (MissionPBComponentEMissionStatus) Descriptor() protoreflect.EnumDescriptor {
	return file_logic_component_mission_comp_proto_enumTypes[0].Descriptor()
}

func (MissionPBComponentEMissionStatus) Type() protoreflect.EnumType {
	return &file_logic_component_mission_comp_proto_enumTypes[0]
}

func (x MissionPBComponentEMissionStatus) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use MissionPBComponentEMissionStatus.Descriptor instead.
func (MissionPBComponentEMissionStatus) EnumDescriptor() ([]byte, []int) {
	return file_logic_component_mission_comp_proto_rawDescGZIP(), []int{0, 0}
}

type MissionListPBComponentEMissionType int32

const (
	MissionListPBComponent_kPlayerMission       MissionListPBComponentEMissionType = 0 //普通任务
	MissionListPBComponent_kPlayerAchievment    MissionListPBComponentEMissionType = 1 //成就
	MissionListPBComponent_kPlayerDailyActivity MissionListPBComponentEMissionType = 2 //每日活动
	MissionListPBComponent_kPlayerMissionSize   MissionListPBComponentEMissionType = 3 //最大值
)

// Enum value maps for MissionListPBComponentEMissionType.
var (
	MissionListPBComponentEMissionType_name = map[int32]string{
		0: "kPlayerMission",
		1: "kPlayerAchievment",
		2: "kPlayerDailyActivity",
		3: "kPlayerMissionSize",
	}
	MissionListPBComponentEMissionType_value = map[string]int32{
		"kPlayerMission":       0,
		"kPlayerAchievment":    1,
		"kPlayerDailyActivity": 2,
		"kPlayerMissionSize":   3,
	}
)

func (x MissionListPBComponentEMissionType) Enum() *MissionListPBComponentEMissionType {
	p := new(MissionListPBComponentEMissionType)
	*p = x
	return p
}

func (x MissionListPBComponentEMissionType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (MissionListPBComponentEMissionType) Descriptor() protoreflect.EnumDescriptor {
	return file_logic_component_mission_comp_proto_enumTypes[1].Descriptor()
}

func (MissionListPBComponentEMissionType) Type() protoreflect.EnumType {
	return &file_logic_component_mission_comp_proto_enumTypes[1]
}

func (x MissionListPBComponentEMissionType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use MissionListPBComponentEMissionType.Descriptor instead.
func (MissionListPBComponentEMissionType) EnumDescriptor() ([]byte, []int) {
	return file_logic_component_mission_comp_proto_rawDescGZIP(), []int{1, 0}
}

type MissionPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id       uint32   `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Status   uint32   `protobuf:"varint,2,opt,name=status,proto3" json:"status,omitempty"`
	Progress []uint32 `protobuf:"varint,3,rep,packed,name=progress,proto3" json:"progress,omitempty"`
	Data     []byte   `protobuf:"bytes,4,opt,name=data,proto3" json:"data,omitempty"` // 各种类型buff的各种数据
}

func (x *MissionPBComponent) Reset() {
	*x = MissionPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_mission_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *MissionPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*MissionPBComponent) ProtoMessage() {}

func (x *MissionPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_mission_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use MissionPBComponent.ProtoReflect.Descriptor instead.
func (*MissionPBComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_mission_comp_proto_rawDescGZIP(), []int{0}
}

func (x *MissionPBComponent) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *MissionPBComponent) GetStatus() uint32 {
	if x != nil {
		return x.Status
	}
	return 0
}

func (x *MissionPBComponent) GetProgress() []uint32 {
	if x != nil {
		return x.Progress
	}
	return nil
}

func (x *MissionPBComponent) GetData() []byte {
	if x != nil {
		return x.Data
	}
	return nil
}

type MissionListPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Missions         map[uint32]*MissionPBComponent `protobuf:"bytes,1,rep,name=missions,proto3" json:"missions,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	CompleteMissions map[uint32]bool                `protobuf:"bytes,2,rep,name=complete_missions,json=completeMissions,proto3" json:"complete_missions,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
	MissionBeginTime map[uint32]uint64              `protobuf:"bytes,3,rep,name=mission_begin_time,json=missionBeginTime,proto3" json:"mission_begin_time,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
	Type             uint32                         `protobuf:"varint,4,opt,name=type,proto3" json:"type,omitempty"`
}

func (x *MissionListPBComponent) Reset() {
	*x = MissionListPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_mission_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *MissionListPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*MissionListPBComponent) ProtoMessage() {}

func (x *MissionListPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_mission_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use MissionListPBComponent.ProtoReflect.Descriptor instead.
func (*MissionListPBComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_mission_comp_proto_rawDescGZIP(), []int{1}
}

func (x *MissionListPBComponent) GetMissions() map[uint32]*MissionPBComponent {
	if x != nil {
		return x.Missions
	}
	return nil
}

func (x *MissionListPBComponent) GetCompleteMissions() map[uint32]bool {
	if x != nil {
		return x.CompleteMissions
	}
	return nil
}

func (x *MissionListPBComponent) GetMissionBeginTime() map[uint32]uint64 {
	if x != nil {
		return x.MissionBeginTime
	}
	return nil
}

func (x *MissionListPBComponent) GetType() uint32 {
	if x != nil {
		return x.Type
	}
	return 0
}

type RewardListPBComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	CanRewardMissionId map[uint32]bool `protobuf:"bytes,2,rep,name=can_reward_mission_id,json=canRewardMissionId,proto3" json:"can_reward_mission_id,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
}

func (x *RewardListPBComponent) Reset() {
	*x = RewardListPBComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_logic_component_mission_comp_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RewardListPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RewardListPBComponent) ProtoMessage() {}

func (x *RewardListPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_logic_component_mission_comp_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RewardListPBComponent.ProtoReflect.Descriptor instead.
func (*RewardListPBComponent) Descriptor() ([]byte, []int) {
	return file_logic_component_mission_comp_proto_rawDescGZIP(), []int{2}
}

func (x *RewardListPBComponent) GetCanRewardMissionId() map[uint32]bool {
	if x != nil {
		return x.CanRewardMissionId
	}
	return nil
}

var File_logic_component_mission_comp_proto protoreflect.FileDescriptor

var file_logic_component_mission_comp_proto_rawDesc = []byte{
	0x0a, 0x22, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x2f, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x22, 0xf5, 0x01, 0x0a, 0x12, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e,
	0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x0e, 0x0a, 0x02, 0x69,
	0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x16, 0x0a, 0x06, 0x73,
	0x74, 0x61, 0x74, 0x75, 0x73, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x06, 0x73, 0x74, 0x61,
	0x74, 0x75, 0x73, 0x12, 0x1a, 0x0a, 0x08, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x18,
	0x03, 0x20, 0x03, 0x28, 0x0d, 0x52, 0x08, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x12,
	0x12, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x04, 0x64,
	0x61, 0x74, 0x61, 0x22, 0x86, 0x01, 0x0a, 0x0e, 0x65, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e,
	0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x12, 0x14, 0x0a, 0x10, 0x45, 0x5f, 0x4d, 0x49, 0x53, 0x53,
	0x49, 0x4f, 0x4e, 0x5f, 0x4e, 0x4f, 0x52, 0x4d, 0x41, 0x4c, 0x10, 0x00, 0x12, 0x19, 0x0a, 0x15,
	0x45, 0x5f, 0x4d, 0x49, 0x53, 0x53, 0x49, 0x4f, 0x4e, 0x5f, 0x41, 0x43, 0x48, 0x49, 0x45, 0x56,
	0x45, 0x4d, 0x45, 0x4e, 0x54, 0x10, 0x01, 0x12, 0x16, 0x0a, 0x12, 0x45, 0x5f, 0x4d, 0x49, 0x53,
	0x53, 0x49, 0x4f, 0x4e, 0x5f, 0x43, 0x4f, 0x4d, 0x50, 0x4c, 0x45, 0x54, 0x45, 0x10, 0x02, 0x12,
	0x16, 0x0a, 0x12, 0x45, 0x5f, 0x4d, 0x49, 0x53, 0x53, 0x49, 0x4f, 0x4e, 0x5f, 0x54, 0x49, 0x4d,
	0x45, 0x5f, 0x4f, 0x55, 0x54, 0x10, 0x03, 0x12, 0x13, 0x0a, 0x0f, 0x45, 0x5f, 0x4d, 0x49, 0x53,
	0x53, 0x49, 0x4f, 0x4e, 0x5f, 0x46, 0x41, 0x49, 0x4c, 0x44, 0x10, 0x04, 0x22, 0xf1, 0x04, 0x0a,
	0x16, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x4c, 0x69, 0x73, 0x74, 0x50, 0x42, 0x43, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x41, 0x0a, 0x08, 0x6d, 0x69, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x73, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x25, 0x2e, 0x4d, 0x69, 0x73, 0x73,
	0x69, 0x6f, 0x6e, 0x4c, 0x69, 0x73, 0x74, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65,
	0x6e, 0x74, 0x2e, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79,
	0x52, 0x08, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x12, 0x5a, 0x0a, 0x11, 0x63, 0x6f,
	0x6d, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x5f, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x18,
	0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x2d, 0x2e, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x4c,
	0x69, 0x73, 0x74, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x43,
	0x6f, 0x6d, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x45,
	0x6e, 0x74, 0x72, 0x79, 0x52, 0x10, 0x63, 0x6f, 0x6d, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x4d, 0x69,
	0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x12, 0x5b, 0x0a, 0x12, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f,
	0x6e, 0x5f, 0x62, 0x65, 0x67, 0x69, 0x6e, 0x5f, 0x74, 0x69, 0x6d, 0x65, 0x18, 0x03, 0x20, 0x03,
	0x28, 0x0b, 0x32, 0x2d, 0x2e, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x4c, 0x69, 0x73, 0x74,
	0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x4d, 0x69, 0x73, 0x73,
	0x69, 0x6f, 0x6e, 0x42, 0x65, 0x67, 0x69, 0x6e, 0x54, 0x69, 0x6d, 0x65, 0x45, 0x6e, 0x74, 0x72,
	0x79, 0x52, 0x10, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x42, 0x65, 0x67, 0x69, 0x6e, 0x54,
	0x69, 0x6d, 0x65, 0x12, 0x12, 0x0a, 0x04, 0x74, 0x79, 0x70, 0x65, 0x18, 0x04, 0x20, 0x01, 0x28,
	0x0d, 0x52, 0x04, 0x74, 0x79, 0x70, 0x65, 0x1a, 0x50, 0x0a, 0x0d, 0x4d, 0x69, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x29, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x13, 0x2e, 0x4d, 0x69, 0x73, 0x73,
	0x69, 0x6f, 0x6e, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x52, 0x05,
	0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x1a, 0x43, 0x0a, 0x15, 0x43, 0x6f, 0x6d,
	0x70, 0x6c, 0x65, 0x74, 0x65, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x73, 0x45, 0x6e, 0x74,
	0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20,
	0x01, 0x28, 0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x1a, 0x43,
	0x0a, 0x15, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x42, 0x65, 0x67, 0x69, 0x6e, 0x54, 0x69,
	0x6d, 0x65, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c,
	0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a,
	0x02, 0x38, 0x01, 0x22, 0x6b, 0x0a, 0x0c, 0x65, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x54,
	0x79, 0x70, 0x65, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d, 0x69,
	0x73, 0x73, 0x69, 0x6f, 0x6e, 0x10, 0x00, 0x12, 0x15, 0x0a, 0x11, 0x6b, 0x50, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x41, 0x63, 0x68, 0x69, 0x65, 0x76, 0x6d, 0x65, 0x6e, 0x74, 0x10, 0x01, 0x12, 0x18,
	0x0a, 0x14, 0x6b, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x44, 0x61, 0x69, 0x6c, 0x79, 0x41, 0x63,
	0x74, 0x69, 0x76, 0x69, 0x74, 0x79, 0x10, 0x02, 0x12, 0x16, 0x0a, 0x12, 0x6b, 0x50, 0x6c, 0x61,
	0x79, 0x65, 0x72, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x03,
	0x22, 0xc1, 0x01, 0x0a, 0x15, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x4c, 0x69, 0x73, 0x74, 0x50,
	0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x61, 0x0a, 0x15, 0x63, 0x61,
	0x6e, 0x5f, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64, 0x5f, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e,
	0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x2e, 0x2e, 0x52, 0x65, 0x77, 0x61,
	0x72, 0x64, 0x4c, 0x69, 0x73, 0x74, 0x50, 0x42, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x2e, 0x43, 0x61, 0x6e, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x4d, 0x69, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x49, 0x64, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x12, 0x63, 0x61, 0x6e, 0x52, 0x65,
	0x77, 0x61, 0x72, 0x64, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x1a, 0x45, 0x0a,
	0x17, 0x43, 0x61, 0x6e, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f,
	0x6e, 0x49, 0x64, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x3a, 0x02, 0x38, 0x01, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62,
	0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_logic_component_mission_comp_proto_rawDescOnce sync.Once
	file_logic_component_mission_comp_proto_rawDescData = file_logic_component_mission_comp_proto_rawDesc
)

func file_logic_component_mission_comp_proto_rawDescGZIP() []byte {
	file_logic_component_mission_comp_proto_rawDescOnce.Do(func() {
		file_logic_component_mission_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_logic_component_mission_comp_proto_rawDescData)
	})
	return file_logic_component_mission_comp_proto_rawDescData
}

var file_logic_component_mission_comp_proto_enumTypes = make([]protoimpl.EnumInfo, 2)
var file_logic_component_mission_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 7)
var file_logic_component_mission_comp_proto_goTypes = []any{
	(MissionPBComponentEMissionStatus)(0),   // 0: MissionPBComponent.eMissionStatus
	(MissionListPBComponentEMissionType)(0), // 1: MissionListPBComponent.eMissionType
	(*MissionPBComponent)(nil),              // 2: MissionPBComponent
	(*MissionListPBComponent)(nil),          // 3: MissionListPBComponent
	(*RewardListPBComponent)(nil),           // 4: RewardListPBComponent
	nil,                                     // 5: MissionListPBComponent.MissionsEntry
	nil,                                     // 6: MissionListPBComponent.CompleteMissionsEntry
	nil,                                     // 7: MissionListPBComponent.MissionBeginTimeEntry
	nil,                                     // 8: RewardListPBComponent.CanRewardMissionIdEntry
}
var file_logic_component_mission_comp_proto_depIdxs = []int32{
	5, // 0: MissionListPBComponent.missions:type_name -> MissionListPBComponent.MissionsEntry
	6, // 1: MissionListPBComponent.complete_missions:type_name -> MissionListPBComponent.CompleteMissionsEntry
	7, // 2: MissionListPBComponent.mission_begin_time:type_name -> MissionListPBComponent.MissionBeginTimeEntry
	8, // 3: RewardListPBComponent.can_reward_mission_id:type_name -> RewardListPBComponent.CanRewardMissionIdEntry
	2, // 4: MissionListPBComponent.MissionsEntry.value:type_name -> MissionPBComponent
	5, // [5:5] is the sub-list for method output_type
	5, // [5:5] is the sub-list for method input_type
	5, // [5:5] is the sub-list for extension type_name
	5, // [5:5] is the sub-list for extension extendee
	0, // [0:5] is the sub-list for field type_name
}

func init() { file_logic_component_mission_comp_proto_init() }
func file_logic_component_mission_comp_proto_init() {
	if File_logic_component_mission_comp_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_logic_component_mission_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*MissionPBComponent); i {
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
		file_logic_component_mission_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*MissionListPBComponent); i {
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
		file_logic_component_mission_comp_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*RewardListPBComponent); i {
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
			RawDescriptor: file_logic_component_mission_comp_proto_rawDesc,
			NumEnums:      2,
			NumMessages:   7,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_logic_component_mission_comp_proto_goTypes,
		DependencyIndexes: file_logic_component_mission_comp_proto_depIdxs,
		EnumInfos:         file_logic_component_mission_comp_proto_enumTypes,
		MessageInfos:      file_logic_component_mission_comp_proto_msgTypes,
	}.Build()
	File_logic_component_mission_comp_proto = out.File
	file_logic_component_mission_comp_proto_rawDesc = nil
	file_logic_component_mission_comp_proto_goTypes = nil
	file_logic_component_mission_comp_proto_depIdxs = nil
}
