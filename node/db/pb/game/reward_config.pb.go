// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: reward_config.proto

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

type Rewardreward struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	RewardItem  uint32 `protobuf:"varint,1,opt,name=reward_item,json=rewardItem,proto3" json:"reward_item,omitempty"`
	RewardCount uint32 `protobuf:"varint,2,opt,name=reward_count,json=rewardCount,proto3" json:"reward_count,omitempty"`
}

func (x *Rewardreward) Reset() {
	*x = Rewardreward{}
	if protoimpl.UnsafeEnabled {
		mi := &file_reward_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Rewardreward) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Rewardreward) ProtoMessage() {}

func (x *Rewardreward) ProtoReflect() protoreflect.Message {
	mi := &file_reward_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Rewardreward.ProtoReflect.Descriptor instead.
func (*Rewardreward) Descriptor() ([]byte, []int) {
	return file_reward_config_proto_rawDescGZIP(), []int{0}
}

func (x *Rewardreward) GetRewardItem() uint32 {
	if x != nil {
		return x.RewardItem
	}
	return 0
}

func (x *Rewardreward) GetRewardCount() uint32 {
	if x != nil {
		return x.RewardCount
	}
	return 0
}

type RewardTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id     uint32          `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Reward []*Rewardreward `protobuf:"bytes,2,rep,name=reward,proto3" json:"reward,omitempty"`
}

func (x *RewardTable) Reset() {
	*x = RewardTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_reward_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RewardTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RewardTable) ProtoMessage() {}

func (x *RewardTable) ProtoReflect() protoreflect.Message {
	mi := &file_reward_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RewardTable.ProtoReflect.Descriptor instead.
func (*RewardTable) Descriptor() ([]byte, []int) {
	return file_reward_config_proto_rawDescGZIP(), []int{1}
}

func (x *RewardTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *RewardTable) GetReward() []*Rewardreward {
	if x != nil {
		return x.Reward
	}
	return nil
}

type RewardTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*RewardTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *RewardTabledData) Reset() {
	*x = RewardTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_reward_config_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RewardTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RewardTabledData) ProtoMessage() {}

func (x *RewardTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_reward_config_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RewardTabledData.ProtoReflect.Descriptor instead.
func (*RewardTabledData) Descriptor() ([]byte, []int) {
	return file_reward_config_proto_rawDescGZIP(), []int{2}
}

func (x *RewardTabledData) GetData() []*RewardTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_reward_config_proto protoreflect.FileDescriptor

var file_reward_config_proto_rawDesc = []byte{
	0x0a, 0x13, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x52, 0x0a, 0x0c, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x72,
	0x65, 0x77, 0x61, 0x72, 0x64, 0x12, 0x1f, 0x0a, 0x0b, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64, 0x5f,
	0x69, 0x74, 0x65, 0x6d, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x72, 0x65, 0x77, 0x61,
	0x72, 0x64, 0x49, 0x74, 0x65, 0x6d, 0x12, 0x21, 0x0a, 0x0c, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64,
	0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x72, 0x65,
	0x77, 0x61, 0x72, 0x64, 0x43, 0x6f, 0x75, 0x6e, 0x74, 0x22, 0x44, 0x0a, 0x0b, 0x52, 0x65, 0x77,
	0x61, 0x72, 0x64, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x25, 0x0a, 0x06, 0x72, 0x65, 0x77, 0x61,
	0x72, 0x64, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x0d, 0x2e, 0x52, 0x65, 0x77, 0x61, 0x72,
	0x64, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64, 0x52, 0x06, 0x72, 0x65, 0x77, 0x61, 0x72, 0x64, 0x22,
	0x34, 0x0a, 0x10, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44,
	0x61, 0x74, 0x61, 0x12, 0x20, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28,
	0x0b, 0x32, 0x0c, 0x2e, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x52,
	0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_reward_config_proto_rawDescOnce sync.Once
	file_reward_config_proto_rawDescData = file_reward_config_proto_rawDesc
)

func file_reward_config_proto_rawDescGZIP() []byte {
	file_reward_config_proto_rawDescOnce.Do(func() {
		file_reward_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_reward_config_proto_rawDescData)
	})
	return file_reward_config_proto_rawDescData
}

var file_reward_config_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_reward_config_proto_goTypes = []any{
	(*Rewardreward)(nil),     // 0: Rewardreward
	(*RewardTable)(nil),      // 1: RewardTable
	(*RewardTabledData)(nil), // 2: RewardTabledData
}
var file_reward_config_proto_depIdxs = []int32{
	0, // 0: RewardTable.reward:type_name -> Rewardreward
	1, // 1: RewardTabledData.data:type_name -> RewardTable
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_reward_config_proto_init() }
func file_reward_config_proto_init() {
	if File_reward_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_reward_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*Rewardreward); i {
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
		file_reward_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*RewardTable); i {
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
		file_reward_config_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*RewardTabledData); i {
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
			RawDescriptor: file_reward_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_reward_config_proto_goTypes,
		DependencyIndexes: file_reward_config_proto_depIdxs,
		MessageInfos:      file_reward_config_proto_msgTypes,
	}.Build()
	File_reward_config_proto = out.File
	file_reward_config_proto_rawDesc = nil
	file_reward_config_proto_goTypes = nil
	file_reward_config_proto_depIdxs = nil
}
