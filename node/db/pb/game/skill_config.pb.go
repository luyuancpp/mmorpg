// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: skill_config.proto

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

type Requestresource struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	RequestresourceId   uint32 `protobuf:"varint,1,opt,name=requestresource_id,json=requestresourceId,proto3" json:"requestresource_id,omitempty"`
	RequestresourceCost uint32 `protobuf:"varint,2,opt,name=requestresource_cost,json=requestresourceCost,proto3" json:"requestresource_cost,omitempty"`
}

func (x *Requestresource) Reset() {
	*x = Requestresource{}
	if protoimpl.UnsafeEnabled {
		mi := &file_skill_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Requestresource) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Requestresource) ProtoMessage() {}

func (x *Requestresource) ProtoReflect() protoreflect.Message {
	mi := &file_skill_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Requestresource.ProtoReflect.Descriptor instead.
func (*Requestresource) Descriptor() ([]byte, []int) {
	return file_skill_config_proto_rawDescGZIP(), []int{0}
}

func (x *Requestresource) GetRequestresourceId() uint32 {
	if x != nil {
		return x.RequestresourceId
	}
	return 0
}

func (x *Requestresource) GetRequestresourceCost() uint32 {
	if x != nil {
		return x.RequestresourceCost
	}
	return 0
}

type SkillTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id                   uint32             `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	SkillType            []uint32           `protobuf:"varint,2,rep,packed,name=skill_type,json=skillType,proto3" json:"skill_type,omitempty"`
	TargetType           []uint32           `protobuf:"varint,3,rep,packed,name=target_type,json=targetType,proto3" json:"target_type,omitempty"`
	RquestTarget         uint32             `protobuf:"varint,4,opt,name=rquest_target,json=rquestTarget,proto3" json:"rquest_target,omitempty"`
	TargetStatus         uint32             `protobuf:"varint,5,opt,name=target_status,json=targetStatus,proto3" json:"target_status,omitempty"`
	Castpoint            uint32             `protobuf:"varint,6,opt,name=castpoint,proto3" json:"castpoint,omitempty"`
	Recoverytime         uint32             `protobuf:"varint,7,opt,name=recoverytime,proto3" json:"recoverytime,omitempty"`
	Immediately          uint32             `protobuf:"varint,8,opt,name=immediately,proto3" json:"immediately,omitempty"`
	Effect               []uint32           `protobuf:"varint,9,rep,packed,name=effect,proto3" json:"effect,omitempty"`
	Postcastrecoverytime uint32             `protobuf:"varint,10,opt,name=postcastrecoverytime,proto3" json:"postcastrecoverytime,omitempty"`
	Channelthink         uint32             `protobuf:"varint,11,opt,name=channelthink,proto3" json:"channelthink,omitempty"`
	Channelfinish        uint32             `protobuf:"varint,12,opt,name=channelfinish,proto3" json:"channelfinish,omitempty"`
	Thinkinterval        uint32             `protobuf:"varint,13,opt,name=thinkinterval,proto3" json:"thinkinterval,omitempty"`
	Channeltime          uint32             `protobuf:"varint,14,opt,name=channeltime,proto3" json:"channeltime,omitempty"`
	Requestresource      []*Requestresource `protobuf:"bytes,15,rep,name=requestresource,proto3" json:"requestresource,omitempty"`
	Range                float64            `protobuf:"fixed64,16,opt,name=range,proto3" json:"range,omitempty"`
	MaxRange             float64            `protobuf:"fixed64,17,opt,name=max_range,json=maxRange,proto3" json:"max_range,omitempty"`
	MminRange            float64            `protobuf:"fixed64,18,opt,name=mmin_range,json=mminRange,proto3" json:"mmin_range,omitempty"`
	SelfStatus           uint32             `protobuf:"varint,19,opt,name=self_status,json=selfStatus,proto3" json:"self_status,omitempty"`
	RequiredStatus       uint32             `protobuf:"varint,20,opt,name=required_status,json=requiredStatus,proto3" json:"required_status,omitempty"`
	CooldownId           uint32             `protobuf:"varint,21,opt,name=cooldown_id,json=cooldownId,proto3" json:"cooldown_id,omitempty"`
}

func (x *SkillTable) Reset() {
	*x = SkillTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_skill_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillTable) ProtoMessage() {}

func (x *SkillTable) ProtoReflect() protoreflect.Message {
	mi := &file_skill_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillTable.ProtoReflect.Descriptor instead.
func (*SkillTable) Descriptor() ([]byte, []int) {
	return file_skill_config_proto_rawDescGZIP(), []int{1}
}

func (x *SkillTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *SkillTable) GetSkillType() []uint32 {
	if x != nil {
		return x.SkillType
	}
	return nil
}

func (x *SkillTable) GetTargetType() []uint32 {
	if x != nil {
		return x.TargetType
	}
	return nil
}

func (x *SkillTable) GetRquestTarget() uint32 {
	if x != nil {
		return x.RquestTarget
	}
	return 0
}

func (x *SkillTable) GetTargetStatus() uint32 {
	if x != nil {
		return x.TargetStatus
	}
	return 0
}

func (x *SkillTable) GetCastpoint() uint32 {
	if x != nil {
		return x.Castpoint
	}
	return 0
}

func (x *SkillTable) GetRecoverytime() uint32 {
	if x != nil {
		return x.Recoverytime
	}
	return 0
}

func (x *SkillTable) GetImmediately() uint32 {
	if x != nil {
		return x.Immediately
	}
	return 0
}

func (x *SkillTable) GetEffect() []uint32 {
	if x != nil {
		return x.Effect
	}
	return nil
}

func (x *SkillTable) GetPostcastrecoverytime() uint32 {
	if x != nil {
		return x.Postcastrecoverytime
	}
	return 0
}

func (x *SkillTable) GetChannelthink() uint32 {
	if x != nil {
		return x.Channelthink
	}
	return 0
}

func (x *SkillTable) GetChannelfinish() uint32 {
	if x != nil {
		return x.Channelfinish
	}
	return 0
}

func (x *SkillTable) GetThinkinterval() uint32 {
	if x != nil {
		return x.Thinkinterval
	}
	return 0
}

func (x *SkillTable) GetChanneltime() uint32 {
	if x != nil {
		return x.Channeltime
	}
	return 0
}

func (x *SkillTable) GetRequestresource() []*Requestresource {
	if x != nil {
		return x.Requestresource
	}
	return nil
}

func (x *SkillTable) GetRange() float64 {
	if x != nil {
		return x.Range
	}
	return 0
}

func (x *SkillTable) GetMaxRange() float64 {
	if x != nil {
		return x.MaxRange
	}
	return 0
}

func (x *SkillTable) GetMminRange() float64 {
	if x != nil {
		return x.MminRange
	}
	return 0
}

func (x *SkillTable) GetSelfStatus() uint32 {
	if x != nil {
		return x.SelfStatus
	}
	return 0
}

func (x *SkillTable) GetRequiredStatus() uint32 {
	if x != nil {
		return x.RequiredStatus
	}
	return 0
}

func (x *SkillTable) GetCooldownId() uint32 {
	if x != nil {
		return x.CooldownId
	}
	return 0
}

type SkillTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*SkillTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *SkillTabledData) Reset() {
	*x = SkillTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_skill_config_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SkillTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SkillTabledData) ProtoMessage() {}

func (x *SkillTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_skill_config_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SkillTabledData.ProtoReflect.Descriptor instead.
func (*SkillTabledData) Descriptor() ([]byte, []int) {
	return file_skill_config_proto_rawDescGZIP(), []int{2}
}

func (x *SkillTabledData) GetData() []*SkillTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_skill_config_proto protoreflect.FileDescriptor

var file_skill_config_proto_rawDesc = []byte{
	0x0a, 0x12, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x22, 0x73, 0x0a, 0x0f, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72,
	0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x12, 0x2d, 0x0a, 0x12, 0x72, 0x65, 0x71, 0x75, 0x65,
	0x73, 0x74, 0x72, 0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x11, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72, 0x65, 0x73, 0x6f,
	0x75, 0x72, 0x63, 0x65, 0x49, 0x64, 0x12, 0x31, 0x0a, 0x14, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73,
	0x74, 0x72, 0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x5f, 0x63, 0x6f, 0x73, 0x74, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x13, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72, 0x65, 0x73,
	0x6f, 0x75, 0x72, 0x63, 0x65, 0x43, 0x6f, 0x73, 0x74, 0x22, 0xe1, 0x05, 0x0a, 0x0a, 0x53, 0x6b,
	0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x6b, 0x69, 0x6c,
	0x6c, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0d, 0x52, 0x09, 0x73, 0x6b,
	0x69, 0x6c, 0x6c, 0x54, 0x79, 0x70, 0x65, 0x12, 0x1f, 0x0a, 0x0b, 0x74, 0x61, 0x72, 0x67, 0x65,
	0x74, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x18, 0x03, 0x20, 0x03, 0x28, 0x0d, 0x52, 0x0a, 0x74, 0x61,
	0x72, 0x67, 0x65, 0x74, 0x54, 0x79, 0x70, 0x65, 0x12, 0x23, 0x0a, 0x0d, 0x72, 0x71, 0x75, 0x65,
	0x73, 0x74, 0x5f, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x0c, 0x72, 0x71, 0x75, 0x65, 0x73, 0x74, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x12, 0x23, 0x0a,
	0x0d, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x18, 0x05,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x53, 0x74, 0x61, 0x74,
	0x75, 0x73, 0x12, 0x1c, 0x0a, 0x09, 0x63, 0x61, 0x73, 0x74, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x18,
	0x06, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x63, 0x61, 0x73, 0x74, 0x70, 0x6f, 0x69, 0x6e, 0x74,
	0x12, 0x22, 0x0a, 0x0c, 0x72, 0x65, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79, 0x74, 0x69, 0x6d, 0x65,
	0x18, 0x07, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x72, 0x65, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79,
	0x74, 0x69, 0x6d, 0x65, 0x12, 0x20, 0x0a, 0x0b, 0x69, 0x6d, 0x6d, 0x65, 0x64, 0x69, 0x61, 0x74,
	0x65, 0x6c, 0x79, 0x18, 0x08, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x69, 0x6d, 0x6d, 0x65, 0x64,
	0x69, 0x61, 0x74, 0x65, 0x6c, 0x79, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x66, 0x66, 0x65, 0x63, 0x74,
	0x18, 0x09, 0x20, 0x03, 0x28, 0x0d, 0x52, 0x06, 0x65, 0x66, 0x66, 0x65, 0x63, 0x74, 0x12, 0x32,
	0x0a, 0x14, 0x70, 0x6f, 0x73, 0x74, 0x63, 0x61, 0x73, 0x74, 0x72, 0x65, 0x63, 0x6f, 0x76, 0x65,
	0x72, 0x79, 0x74, 0x69, 0x6d, 0x65, 0x18, 0x0a, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x14, 0x70, 0x6f,
	0x73, 0x74, 0x63, 0x61, 0x73, 0x74, 0x72, 0x65, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79, 0x74, 0x69,
	0x6d, 0x65, 0x12, 0x22, 0x0a, 0x0c, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x74, 0x68, 0x69,
	0x6e, 0x6b, 0x18, 0x0b, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65,
	0x6c, 0x74, 0x68, 0x69, 0x6e, 0x6b, 0x12, 0x24, 0x0a, 0x0d, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65,
	0x6c, 0x66, 0x69, 0x6e, 0x69, 0x73, 0x68, 0x18, 0x0c, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0d, 0x63,
	0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x66, 0x69, 0x6e, 0x69, 0x73, 0x68, 0x12, 0x24, 0x0a, 0x0d,
	0x74, 0x68, 0x69, 0x6e, 0x6b, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x76, 0x61, 0x6c, 0x18, 0x0d, 0x20,
	0x01, 0x28, 0x0d, 0x52, 0x0d, 0x74, 0x68, 0x69, 0x6e, 0x6b, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x76,
	0x61, 0x6c, 0x12, 0x20, 0x0a, 0x0b, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x74, 0x69, 0x6d,
	0x65, 0x18, 0x0e, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c,
	0x74, 0x69, 0x6d, 0x65, 0x12, 0x3a, 0x0a, 0x0f, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72,
	0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x18, 0x0f, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x10, 0x2e,
	0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72, 0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x52,
	0x0f, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x72, 0x65, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65,
	0x12, 0x14, 0x0a, 0x05, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x18, 0x10, 0x20, 0x01, 0x28, 0x01, 0x52,
	0x05, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x6d, 0x61, 0x78, 0x5f, 0x72, 0x61,
	0x6e, 0x67, 0x65, 0x18, 0x11, 0x20, 0x01, 0x28, 0x01, 0x52, 0x08, 0x6d, 0x61, 0x78, 0x52, 0x61,
	0x6e, 0x67, 0x65, 0x12, 0x1d, 0x0a, 0x0a, 0x6d, 0x6d, 0x69, 0x6e, 0x5f, 0x72, 0x61, 0x6e, 0x67,
	0x65, 0x18, 0x12, 0x20, 0x01, 0x28, 0x01, 0x52, 0x09, 0x6d, 0x6d, 0x69, 0x6e, 0x52, 0x61, 0x6e,
	0x67, 0x65, 0x12, 0x1f, 0x0a, 0x0b, 0x73, 0x65, 0x6c, 0x66, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x75,
	0x73, 0x18, 0x13, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x73, 0x65, 0x6c, 0x66, 0x53, 0x74, 0x61,
	0x74, 0x75, 0x73, 0x12, 0x27, 0x0a, 0x0f, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x5f,
	0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x18, 0x14, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0e, 0x72, 0x65,
	0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x12, 0x1f, 0x0a, 0x0b,
	0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x15, 0x20, 0x01, 0x28,
	0x0d, 0x52, 0x0a, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x49, 0x64, 0x22, 0x32, 0x0a,
	0x0f, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44, 0x61, 0x74, 0x61,
	0x12, 0x1f, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x0b,
	0x2e, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x52, 0x04, 0x64, 0x61, 0x74,
	0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_skill_config_proto_rawDescOnce sync.Once
	file_skill_config_proto_rawDescData = file_skill_config_proto_rawDesc
)

func file_skill_config_proto_rawDescGZIP() []byte {
	file_skill_config_proto_rawDescOnce.Do(func() {
		file_skill_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_skill_config_proto_rawDescData)
	})
	return file_skill_config_proto_rawDescData
}

var file_skill_config_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_skill_config_proto_goTypes = []any{
	(*Requestresource)(nil), // 0: requestresource
	(*SkillTable)(nil),      // 1: SkillTable
	(*SkillTabledData)(nil), // 2: SkillTabledData
}
var file_skill_config_proto_depIdxs = []int32{
	0, // 0: SkillTable.requestresource:type_name -> requestresource
	1, // 1: SkillTabledData.data:type_name -> SkillTable
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_skill_config_proto_init() }
func file_skill_config_proto_init() {
	if File_skill_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_skill_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*Requestresource); i {
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
		file_skill_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*SkillTable); i {
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
		file_skill_config_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*SkillTabledData); i {
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
			RawDescriptor: file_skill_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_skill_config_proto_goTypes,
		DependencyIndexes: file_skill_config_proto_depIdxs,
		MessageInfos:      file_skill_config_proto_msgTypes,
	}.Build()
	File_skill_config_proto = out.File
	file_skill_config_proto_rawDesc = nil
	file_skill_config_proto_goTypes = nil
	file_skill_config_proto_depIdxs = nil
}
