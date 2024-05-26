// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.1
// 	protoc        v3.19.4
// source: common_proto/db_service.proto

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

type LoadAccountRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account string `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
}

func (x *LoadAccountRequest) Reset() {
	*x = LoadAccountRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoadAccountRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoadAccountRequest) ProtoMessage() {}

func (x *LoadAccountRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoadAccountRequest.ProtoReflect.Descriptor instead.
func (*LoadAccountRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{0}
}

func (x *LoadAccountRequest) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

type LoadAccountResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *LoadAccountResponse) Reset() {
	*x = LoadAccountResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoadAccountResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoadAccountResponse) ProtoMessage() {}

func (x *LoadAccountResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoadAccountResponse.ProtoReflect.Descriptor instead.
func (*LoadAccountResponse) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{1}
}

type SaveAccountRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account string `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
}

func (x *SaveAccountRequest) Reset() {
	*x = SaveAccountRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SaveAccountRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SaveAccountRequest) ProtoMessage() {}

func (x *SaveAccountRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SaveAccountRequest.ProtoReflect.Descriptor instead.
func (*SaveAccountRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{2}
}

func (x *SaveAccountRequest) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

type SaveAccountResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *SaveAccountResponse) Reset() {
	*x = SaveAccountResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SaveAccountResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SaveAccountResponse) ProtoMessage() {}

func (x *SaveAccountResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SaveAccountResponse.ProtoReflect.Descriptor instead.
func (*SaveAccountResponse) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{3}
}

type LoadPlayerRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *LoadPlayerRequest) Reset() {
	*x = LoadPlayerRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoadPlayerRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoadPlayerRequest) ProtoMessage() {}

func (x *LoadPlayerRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoadPlayerRequest.ProtoReflect.Descriptor instead.
func (*LoadPlayerRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{4}
}

func (x *LoadPlayerRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type LoadPlayerResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *LoadPlayerResponse) Reset() {
	*x = LoadPlayerResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoadPlayerResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoadPlayerResponse) ProtoMessage() {}

func (x *LoadPlayerResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoadPlayerResponse.ProtoReflect.Descriptor instead.
func (*LoadPlayerResponse) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{5}
}

func (x *LoadPlayerResponse) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type SavePlayerRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *SavePlayerRequest) Reset() {
	*x = SavePlayerRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[6]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SavePlayerRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SavePlayerRequest) ProtoMessage() {}

func (x *SavePlayerRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[6]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SavePlayerRequest.ProtoReflect.Descriptor instead.
func (*SavePlayerRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{6}
}

func (x *SavePlayerRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type SavePlayerResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,2,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *SavePlayerResponse) Reset() {
	*x = SavePlayerResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_db_service_proto_msgTypes[7]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SavePlayerResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SavePlayerResponse) ProtoMessage() {}

func (x *SavePlayerResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_db_service_proto_msgTypes[7]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SavePlayerResponse.ProtoReflect.Descriptor instead.
func (*SavePlayerResponse) Descriptor() ([]byte, []int) {
	return file_common_proto_db_service_proto_rawDescGZIP(), []int{7}
}

func (x *SavePlayerResponse) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

var File_common_proto_db_service_proto protoreflect.FileDescriptor

var file_common_proto_db_service_proto_rawDesc = []byte{
	0x0a, 0x1d, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x64,
	0x62, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22,
	0x2e, 0x0a, 0x12, 0x4c, 0x6f, 0x61, 0x64, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65,
	0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x18, 0x0a, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x22,
	0x15, 0x0a, 0x13, 0x4c, 0x6f, 0x61, 0x64, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65,
	0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x22, 0x2e, 0x0a, 0x12, 0x53, 0x61, 0x76, 0x65, 0x41, 0x63,
	0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x18, 0x0a, 0x07,
	0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x61,
	0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x22, 0x15, 0x0a, 0x13, 0x53, 0x61, 0x76, 0x65, 0x41, 0x63,
	0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x22, 0x30, 0x0a,
	0x11, 0x4c, 0x6f, 0x61, 0x64, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x71, 0x75, 0x65,
	0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x22,
	0x31, 0x0a, 0x12, 0x4c, 0x6f, 0x61, 0x64, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x73,
	0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f,
	0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x49, 0x64, 0x22, 0x30, 0x0a, 0x11, 0x53, 0x61, 0x76, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x49, 0x64, 0x22, 0x31, 0x0a, 0x12, 0x53, 0x61, 0x76, 0x65, 0x50, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c,
	0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x32, 0x84, 0x01, 0x0a, 0x10, 0x41, 0x63, 0x63, 0x6f,
	0x75, 0x6e, 0x74, 0x44, 0x42, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x37, 0x0a, 0x0a,
	0x4c, 0x6f, 0x61, 0x64, 0x32, 0x52, 0x65, 0x64, 0x69, 0x73, 0x12, 0x13, 0x2e, 0x4c, 0x6f, 0x61,
	0x64, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a,
	0x14, 0x2e, 0x4c, 0x6f, 0x61, 0x64, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65, 0x73,
	0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x37, 0x0a, 0x0a, 0x53, 0x61, 0x76, 0x65, 0x32, 0x52, 0x65,
	0x64, 0x69, 0x73, 0x12, 0x13, 0x2e, 0x53, 0x61, 0x76, 0x65, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e,
	0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x14, 0x2e, 0x53, 0x61, 0x76, 0x65, 0x41,
	0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x32, 0x7f,
	0x0a, 0x0f, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x44, 0x42, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63,
	0x65, 0x12, 0x35, 0x0a, 0x0a, 0x4c, 0x6f, 0x61, 0x64, 0x32, 0x52, 0x65, 0x64, 0x69, 0x73, 0x12,
	0x12, 0x2e, 0x4c, 0x6f, 0x61, 0x64, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x1a, 0x13, 0x2e, 0x4c, 0x6f, 0x61, 0x64, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x35, 0x0a, 0x0a, 0x53, 0x61, 0x76, 0x65,
	0x32, 0x52, 0x65, 0x64, 0x69, 0x73, 0x12, 0x12, 0x2e, 0x53, 0x61, 0x76, 0x65, 0x50, 0x6c, 0x61,
	0x79, 0x65, 0x72, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x13, 0x2e, 0x53, 0x61, 0x76,
	0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x42,
	0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x33,
}

var (
	file_common_proto_db_service_proto_rawDescOnce sync.Once
	file_common_proto_db_service_proto_rawDescData = file_common_proto_db_service_proto_rawDesc
)

func file_common_proto_db_service_proto_rawDescGZIP() []byte {
	file_common_proto_db_service_proto_rawDescOnce.Do(func() {
		file_common_proto_db_service_proto_rawDescData = protoimpl.X.CompressGZIP(file_common_proto_db_service_proto_rawDescData)
	})
	return file_common_proto_db_service_proto_rawDescData
}

var file_common_proto_db_service_proto_msgTypes = make([]protoimpl.MessageInfo, 8)
var file_common_proto_db_service_proto_goTypes = []interface{}{
	(*LoadAccountRequest)(nil),  // 0: LoadAccountRequest
	(*LoadAccountResponse)(nil), // 1: LoadAccountResponse
	(*SaveAccountRequest)(nil),  // 2: SaveAccountRequest
	(*SaveAccountResponse)(nil), // 3: SaveAccountResponse
	(*LoadPlayerRequest)(nil),   // 4: LoadPlayerRequest
	(*LoadPlayerResponse)(nil),  // 5: LoadPlayerResponse
	(*SavePlayerRequest)(nil),   // 6: SavePlayerRequest
	(*SavePlayerResponse)(nil),  // 7: SavePlayerResponse
}
var file_common_proto_db_service_proto_depIdxs = []int32{
	0, // 0: AccountDBService.Load2Redis:input_type -> LoadAccountRequest
	2, // 1: AccountDBService.Save2Redis:input_type -> SaveAccountRequest
	4, // 2: PlayerDBService.Load2Redis:input_type -> LoadPlayerRequest
	6, // 3: PlayerDBService.Save2Redis:input_type -> SavePlayerRequest
	1, // 4: AccountDBService.Load2Redis:output_type -> LoadAccountResponse
	3, // 5: AccountDBService.Save2Redis:output_type -> SaveAccountResponse
	5, // 6: PlayerDBService.Load2Redis:output_type -> LoadPlayerResponse
	7, // 7: PlayerDBService.Save2Redis:output_type -> SavePlayerResponse
	4, // [4:8] is the sub-list for method output_type
	0, // [0:4] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_common_proto_db_service_proto_init() }
func file_common_proto_db_service_proto_init() {
	if File_common_proto_db_service_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_common_proto_db_service_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoadAccountRequest); i {
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
		file_common_proto_db_service_proto_msgTypes[1].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoadAccountResponse); i {
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
		file_common_proto_db_service_proto_msgTypes[2].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*SaveAccountRequest); i {
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
		file_common_proto_db_service_proto_msgTypes[3].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*SaveAccountResponse); i {
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
		file_common_proto_db_service_proto_msgTypes[4].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoadPlayerRequest); i {
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
		file_common_proto_db_service_proto_msgTypes[5].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoadPlayerResponse); i {
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
		file_common_proto_db_service_proto_msgTypes[6].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*SavePlayerRequest); i {
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
		file_common_proto_db_service_proto_msgTypes[7].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*SavePlayerResponse); i {
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
			RawDescriptor: file_common_proto_db_service_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   8,
			NumExtensions: 0,
			NumServices:   2,
		},
		GoTypes:           file_common_proto_db_service_proto_goTypes,
		DependencyIndexes: file_common_proto_db_service_proto_depIdxs,
		MessageInfos:      file_common_proto_db_service_proto_msgTypes,
	}.Build()
	File_common_proto_db_service_proto = out.File
	file_common_proto_db_service_proto_rawDesc = nil
	file_common_proto_db_service_proto_goTypes = nil
	file_common_proto_db_service_proto_depIdxs = nil
}