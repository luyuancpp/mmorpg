// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.1
// 	protoc        v4.25.1
// source: common_proto/mysql_database_table.proto

package game

import (
	_ "github.com/luyuancpp/dbprotooption"
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

// ///////////         game
type AccountDatabase struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account       string                `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
	Password      string                `protobuf:"bytes,2,opt,name=password,proto3" json:"password,omitempty"`
	SimplePlayers *AccountSimplePlayers `protobuf:"bytes,3,opt,name=simple_players,json=simplePlayers,proto3" json:"simple_players,omitempty"`
}

func (x *AccountDatabase) Reset() {
	*x = AccountDatabase{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_mysql_database_table_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AccountDatabase) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AccountDatabase) ProtoMessage() {}

func (x *AccountDatabase) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_mysql_database_table_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AccountDatabase.ProtoReflect.Descriptor instead.
func (*AccountDatabase) Descriptor() ([]byte, []int) {
	return file_common_proto_mysql_database_table_proto_rawDescGZIP(), []int{0}
}

func (x *AccountDatabase) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

func (x *AccountDatabase) GetPassword() string {
	if x != nil {
		return x.Password
	}
	return ""
}

func (x *AccountDatabase) GetSimplePlayers() *AccountSimplePlayers {
	if x != nil {
		return x.SimplePlayers
	}
	return nil
}

type AccountShareDatabase struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account string `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
}

func (x *AccountShareDatabase) Reset() {
	*x = AccountShareDatabase{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_mysql_database_table_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AccountShareDatabase) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AccountShareDatabase) ProtoMessage() {}

func (x *AccountShareDatabase) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_mysql_database_table_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AccountShareDatabase.ProtoReflect.Descriptor instead.
func (*AccountShareDatabase) Descriptor() ([]byte, []int) {
	return file_common_proto_mysql_database_table_proto_rawDescGZIP(), []int{1}
}

func (x *AccountShareDatabase) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

// 中心服玩家专用数据
type PlayerCentreDatabase struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId  uint64               `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SceneInfo *PlayerSceneInfoComp `protobuf:"bytes,2,opt,name=scene_info,json=sceneInfo,proto3" json:"scene_info,omitempty"`
}

func (x *PlayerCentreDatabase) Reset() {
	*x = PlayerCentreDatabase{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_mysql_database_table_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerCentreDatabase) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerCentreDatabase) ProtoMessage() {}

func (x *PlayerCentreDatabase) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_mysql_database_table_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerCentreDatabase.ProtoReflect.Descriptor instead.
func (*PlayerCentreDatabase) Descriptor() ([]byte, []int) {
	return file_common_proto_mysql_database_table_proto_rawDescGZIP(), []int{2}
}

func (x *PlayerCentreDatabase) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *PlayerCentreDatabase) GetSceneInfo() *PlayerSceneInfoComp {
	if x != nil {
		return x.SceneInfo
	}
	return nil
}

// 玩家数据库表1,重要数据放这里，不重要数据放另外一个表，(重要:比如金钱，跟收入有关)，登录马上加载
type PlayerDatabase struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId     uint64     `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	Transform    *Transform `protobuf:"bytes,2,opt,name=transform,proto3" json:"transform,omitempty"`
	RegisterTime int64      `protobuf:"varint,3,opt,name=register_time,json=registerTime,proto3" json:"register_time,omitempty"`
}

func (x *PlayerDatabase) Reset() {
	*x = PlayerDatabase{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_mysql_database_table_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerDatabase) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerDatabase) ProtoMessage() {}

func (x *PlayerDatabase) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_mysql_database_table_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerDatabase.ProtoReflect.Descriptor instead.
func (*PlayerDatabase) Descriptor() ([]byte, []int) {
	return file_common_proto_mysql_database_table_proto_rawDescGZIP(), []int{3}
}

func (x *PlayerDatabase) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *PlayerDatabase) GetTransform() *Transform {
	if x != nil {
		return x.Transform
	}
	return nil
}

func (x *PlayerDatabase) GetRegisterTime() int64 {
	if x != nil {
		return x.RegisterTime
	}
	return 0
}

// 玩家数据库表2,不重要数据放这里，不会马上加载
type PlayerUnimportanceDatabase struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *PlayerUnimportanceDatabase) Reset() {
	*x = PlayerUnimportanceDatabase{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_mysql_database_table_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *PlayerUnimportanceDatabase) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*PlayerUnimportanceDatabase) ProtoMessage() {}

func (x *PlayerUnimportanceDatabase) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_mysql_database_table_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use PlayerUnimportanceDatabase.ProtoReflect.Descriptor instead.
func (*PlayerUnimportanceDatabase) Descriptor() ([]byte, []int) {
	return file_common_proto_mysql_database_table_proto_rawDescGZIP(), []int{4}
}

func (x *PlayerUnimportanceDatabase) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

var File_common_proto_mysql_database_table_proto protoreflect.FileDescriptor

var file_common_proto_mysql_database_table_proto_rawDesc = []byte{
	0x0a, 0x27, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6d,
	0x79, 0x73, 0x71, 0x6c, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x5f, 0x74, 0x61,
	0x62, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x1a, 0x63, 0x6f, 0x6d, 0x6d, 0x6f,
	0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x64, 0x62, 0x5f, 0x62, 0x61, 0x73, 0x65, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x23, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x2f, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x64, 0x61, 0x74, 0x61,
	0x62, 0x61, 0x73, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x17, 0x63, 0x6f, 0x6d, 0x6d,
	0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x1a, 0x27, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x5f, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x73, 0x63, 0x65, 0x6e,
	0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x61, 0x63,
	0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x95,
	0x01, 0x0a, 0x10, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62,
	0x61, 0x73, 0x65, 0x12, 0x18, 0x0a, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x12, 0x1a, 0x0a,
	0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52,
	0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x12, 0x3e, 0x0a, 0x0e, 0x73, 0x69, 0x6d,
	0x70, 0x6c, 0x65, 0x5f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x18, 0x03, 0x20, 0x01, 0x28,
	0x0b, 0x32, 0x17, 0x2e, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x73, 0x69, 0x6d, 0x70,
	0x6c, 0x65, 0x5f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x52, 0x0d, 0x73, 0x69, 0x6d, 0x70,
	0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x3a, 0x0b, 0x92, 0xb5, 0x18, 0x07, 0x61,
	0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x22, 0x3f, 0x0a, 0x16, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e,
	0x74, 0x5f, 0x73, 0x68, 0x61, 0x72, 0x65, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65,
	0x12, 0x18, 0x0a, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x3a, 0x0b, 0x92, 0xb5, 0x18, 0x07,
	0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x22, 0x86, 0x01, 0x0a, 0x16, 0x70, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x5f, 0x63, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61,
	0x73, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x12,
	0x33, 0x0a, 0x0a, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x02, 0x20,
	0x01, 0x28, 0x0b, 0x32, 0x14, 0x2e, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x49, 0x6e, 0x66, 0x6f, 0x43, 0x6f, 0x6d, 0x70, 0x52, 0x09, 0x73, 0x63, 0x65, 0x6e, 0x65,
	0x49, 0x6e, 0x66, 0x6f, 0x3a, 0x1a, 0x92, 0xb5, 0x18, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x5f, 0x69, 0x64, 0xb2, 0xb5, 0x18, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64,
	0x22, 0x99, 0x01, 0x0a, 0x0f, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x64, 0x61, 0x74, 0x61,
	0x62, 0x61, 0x73, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69,
	0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49,
	0x64, 0x12, 0x28, 0x0a, 0x09, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x0b, 0x32, 0x0a, 0x2e, 0x54, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d,
	0x52, 0x09, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x66, 0x6f, 0x72, 0x6d, 0x12, 0x23, 0x0a, 0x0d, 0x72,
	0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x5f, 0x74, 0x69, 0x6d, 0x65, 0x18, 0x03, 0x20, 0x01,
	0x28, 0x03, 0x52, 0x0c, 0x72, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x54, 0x69, 0x6d, 0x65,
	0x3a, 0x1a, 0x92, 0xb5, 0x18, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0xb2,
	0xb5, 0x18, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x22, 0x57, 0x0a, 0x1c,
	0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x75, 0x6e, 0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x61,
	0x6e, 0x63, 0x65, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x12, 0x1b, 0x0a, 0x09,
	0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52,
	0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x3a, 0x1a, 0x92, 0xb5, 0x18, 0x09, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0xb2, 0xb5, 0x18, 0x09, 0x70, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x5f, 0x69, 0x64, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_common_proto_mysql_database_table_proto_rawDescOnce sync.Once
	file_common_proto_mysql_database_table_proto_rawDescData = file_common_proto_mysql_database_table_proto_rawDesc
)

func file_common_proto_mysql_database_table_proto_rawDescGZIP() []byte {
	file_common_proto_mysql_database_table_proto_rawDescOnce.Do(func() {
		file_common_proto_mysql_database_table_proto_rawDescData = protoimpl.X.CompressGZIP(file_common_proto_mysql_database_table_proto_rawDescData)
	})
	return file_common_proto_mysql_database_table_proto_rawDescData
}

var file_common_proto_mysql_database_table_proto_msgTypes = make([]protoimpl.MessageInfo, 5)
var file_common_proto_mysql_database_table_proto_goTypes = []interface{}{
	(*AccountDatabase)(nil),            // 0: account_database
	(*AccountShareDatabase)(nil),       // 1: account_share_database
	(*PlayerCentreDatabase)(nil),       // 2: player_centre_database
	(*PlayerDatabase)(nil),             // 3: player_database
	(*PlayerUnimportanceDatabase)(nil), // 4: player_unimportance_database
	(*AccountSimplePlayers)(nil),       // 5: account_simple_players
	(*PlayerSceneInfoComp)(nil),        // 6: PlayerSceneInfoComp
	(*Transform)(nil),                  // 7: Transform
}
var file_common_proto_mysql_database_table_proto_depIdxs = []int32{
	5, // 0: account_database.simple_players:type_name -> account_simple_players
	6, // 1: player_centre_database.scene_info:type_name -> PlayerSceneInfoComp
	7, // 2: player_database.transform:type_name -> Transform
	3, // [3:3] is the sub-list for method output_type
	3, // [3:3] is the sub-list for method input_type
	3, // [3:3] is the sub-list for extension type_name
	3, // [3:3] is the sub-list for extension extendee
	0, // [0:3] is the sub-list for field type_name
}

func init() { file_common_proto_mysql_database_table_proto_init() }
func file_common_proto_mysql_database_table_proto_init() {
	if File_common_proto_mysql_database_table_proto != nil {
		return
	}
	file_common_proto_account_database_proto_init()
	file_common_proto_comp_proto_init()
	file_component_proto_player_scene_comp_proto_init()
	file_component_proto_actor_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_common_proto_mysql_database_table_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*AccountDatabase); i {
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
		file_common_proto_mysql_database_table_proto_msgTypes[1].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*AccountShareDatabase); i {
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
		file_common_proto_mysql_database_table_proto_msgTypes[2].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*PlayerCentreDatabase); i {
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
		file_common_proto_mysql_database_table_proto_msgTypes[3].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*PlayerDatabase); i {
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
		file_common_proto_mysql_database_table_proto_msgTypes[4].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*PlayerUnimportanceDatabase); i {
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
			RawDescriptor: file_common_proto_mysql_database_table_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   5,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_common_proto_mysql_database_table_proto_goTypes,
		DependencyIndexes: file_common_proto_mysql_database_table_proto_depIdxs,
		MessageInfos:      file_common_proto_mysql_database_table_proto_msgTypes,
	}.Build()
	File_common_proto_mysql_database_table_proto = out.File
	file_common_proto_mysql_database_table_proto_rawDesc = nil
	file_common_proto_mysql_database_table_proto_goTypes = nil
	file_common_proto_mysql_database_table_proto_depIdxs = nil
}