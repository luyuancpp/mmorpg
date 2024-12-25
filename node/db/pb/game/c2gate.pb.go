// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: proto/common/c2gate.proto

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

type ClientMessageType int32

const (
	ClientMessageType_CLIENT_REQUEST   ClientMessageType = 0
	ClientMessageType_CLIENT_RESPONSE  ClientMessageType = 1
	ClientMessageType_CLIENT_RPC_ERROR ClientMessageType = 2 // not used
)

// Enum value maps for ClientMessageType.
var (
	ClientMessageType_name = map[int32]string{
		0: "CLIENT_REQUEST",
		1: "CLIENT_RESPONSE",
		2: "CLIENT_RPC_ERROR",
	}
	ClientMessageType_value = map[string]int32{
		"CLIENT_REQUEST":   0,
		"CLIENT_RESPONSE":  1,
		"CLIENT_RPC_ERROR": 2,
	}
)

func (x ClientMessageType) Enum() *ClientMessageType {
	p := new(ClientMessageType)
	*p = x
	return p
}

func (x ClientMessageType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (ClientMessageType) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_common_c2gate_proto_enumTypes[0].Descriptor()
}

func (ClientMessageType) Type() protoreflect.EnumType {
	return &file_proto_common_c2gate_proto_enumTypes[0]
}

func (x ClientMessageType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use ClientMessageType.Descriptor instead.
func (ClientMessageType) EnumDescriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{0}
}

type TipCode int32

const (
	TipCode_CLIENT_RPC_NO_ERROR     TipCode = 0
	TipCode_CLIENT_WRONG            TipCode = 1
	TipCode_CLIENT_NO_SERVICE       TipCode = 2
	TipCode_CLIENT_NO_METHOD        TipCode = 3
	TipCode_CLIENT_INVALID_REQUEST  TipCode = 4
	TipCode_CLIENT_INVALID_RESPONSE TipCode = 5
	TipCode_CLIENT_TIMEOUT          TipCode = 6
)

// Enum value maps for TipCode.
var (
	TipCode_name = map[int32]string{
		0: "CLIENT_RPC_NO_ERROR",
		1: "CLIENT_WRONG",
		2: "CLIENT_NO_SERVICE",
		3: "CLIENT_NO_METHOD",
		4: "CLIENT_INVALID_REQUEST",
		5: "CLIENT_INVALID_RESPONSE",
		6: "CLIENT_TIMEOUT",
	}
	TipCode_value = map[string]int32{
		"CLIENT_RPC_NO_ERROR":     0,
		"CLIENT_WRONG":            1,
		"CLIENT_NO_SERVICE":       2,
		"CLIENT_NO_METHOD":        3,
		"CLIENT_INVALID_REQUEST":  4,
		"CLIENT_INVALID_RESPONSE": 5,
		"CLIENT_TIMEOUT":          6,
	}
)

func (x TipCode) Enum() *TipCode {
	p := new(TipCode)
	*p = x
	return p
}

func (x TipCode) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (TipCode) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_common_c2gate_proto_enumTypes[1].Descriptor()
}

func (TipCode) Type() protoreflect.EnumType {
	return &file_proto_common_c2gate_proto_enumTypes[1]
}

func (x TipCode) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use TipCode.Descriptor instead.
func (TipCode) EnumDescriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{1}
}

type AccountSimplePlayerWrapper struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Player *AccountSimplePlayer `protobuf:"bytes,1,opt,name=player,proto3" json:"player,omitempty"`
}

func (x *AccountSimplePlayerWrapper) Reset() {
	*x = AccountSimplePlayerWrapper{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AccountSimplePlayerWrapper) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AccountSimplePlayerWrapper) ProtoMessage() {}

func (x *AccountSimplePlayerWrapper) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AccountSimplePlayerWrapper.ProtoReflect.Descriptor instead.
func (*AccountSimplePlayerWrapper) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{0}
}

func (x *AccountSimplePlayerWrapper) GetPlayer() *AccountSimplePlayer {
	if x != nil {
		return x.Player
	}
	return nil
}

type LoginRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account  string `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
	Password string `protobuf:"bytes,2,opt,name=password,proto3" json:"password,omitempty"`
}

func (x *LoginRequest) Reset() {
	*x = LoginRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginRequest) ProtoMessage() {}

func (x *LoginRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginRequest.ProtoReflect.Descriptor instead.
func (*LoginRequest) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{1}
}

func (x *LoginRequest) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

func (x *LoginRequest) GetPassword() string {
	if x != nil {
		return x.Password
	}
	return ""
}

type LoginResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage               `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
	Players      []*AccountSimplePlayerWrapper `protobuf:"bytes,2,rep,name=players,proto3" json:"players,omitempty"`
}

func (x *LoginResponse) Reset() {
	*x = LoginResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginResponse) ProtoMessage() {}

func (x *LoginResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginResponse.ProtoReflect.Descriptor instead.
func (*LoginResponse) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{2}
}

func (x *LoginResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

func (x *LoginResponse) GetPlayers() []*AccountSimplePlayerWrapper {
	if x != nil {
		return x.Players
	}
	return nil
}

type TestResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage               `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
	Players      []*AccountSimplePlayerWrapper `protobuf:"bytes,2,rep,name=players,proto3" json:"players,omitempty"`
	Teststring   []string                      `protobuf:"bytes,3,rep,name=teststring,proto3" json:"teststring,omitempty"`
	Testint      []int32                       `protobuf:"varint,4,rep,packed,name=testint,proto3" json:"testint,omitempty"`
}

func (x *TestResponse) Reset() {
	*x = TestResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *TestResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*TestResponse) ProtoMessage() {}

func (x *TestResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use TestResponse.ProtoReflect.Descriptor instead.
func (*TestResponse) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{3}
}

func (x *TestResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

func (x *TestResponse) GetPlayers() []*AccountSimplePlayerWrapper {
	if x != nil {
		return x.Players
	}
	return nil
}

func (x *TestResponse) GetTeststring() []string {
	if x != nil {
		return x.Teststring
	}
	return nil
}

func (x *TestResponse) GetTestint() []int32 {
	if x != nil {
		return x.Testint
	}
	return nil
}

type CreatePlayerRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *CreatePlayerRequest) Reset() {
	*x = CreatePlayerRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CreatePlayerRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CreatePlayerRequest) ProtoMessage() {}

func (x *CreatePlayerRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CreatePlayerRequest.ProtoReflect.Descriptor instead.
func (*CreatePlayerRequest) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{4}
}

type CreatePlayerResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage               `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
	Players      []*AccountSimplePlayerWrapper `protobuf:"bytes,2,rep,name=players,proto3" json:"players,omitempty"`
}

func (x *CreatePlayerResponse) Reset() {
	*x = CreatePlayerResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CreatePlayerResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CreatePlayerResponse) ProtoMessage() {}

func (x *CreatePlayerResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CreatePlayerResponse.ProtoReflect.Descriptor instead.
func (*CreatePlayerResponse) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{5}
}

func (x *CreatePlayerResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

func (x *CreatePlayerResponse) GetPlayers() []*AccountSimplePlayerWrapper {
	if x != nil {
		return x.Players
	}
	return nil
}

type EnterGameRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *EnterGameRequest) Reset() {
	*x = EnterGameRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[6]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterGameRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameRequest) ProtoMessage() {}

func (x *EnterGameRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[6]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterGameRequest.ProtoReflect.Descriptor instead.
func (*EnterGameRequest) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{6}
}

func (x *EnterGameRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type EnterGameResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ErrorMessage *TipInfoMessage `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
}

func (x *EnterGameResponse) Reset() {
	*x = EnterGameResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[7]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterGameResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameResponse) ProtoMessage() {}

func (x *EnterGameResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[7]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterGameResponse.ProtoReflect.Descriptor instead.
func (*EnterGameResponse) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{7}
}

func (x *EnterGameResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

type LeaveGameRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *LeaveGameRequest) Reset() {
	*x = LeaveGameRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[8]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LeaveGameRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LeaveGameRequest) ProtoMessage() {}

func (x *LeaveGameRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[8]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LeaveGameRequest.ProtoReflect.Descriptor instead.
func (*LeaveGameRequest) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{8}
}

type ClientRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id        uint64 `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Service   string `protobuf:"bytes,2,opt,name=service,proto3" json:"service,omitempty"`
	Method    string `protobuf:"bytes,3,opt,name=method,proto3" json:"method,omitempty"`
	Body      []byte `protobuf:"bytes,4,opt,name=body,proto3" json:"body,omitempty"`
	MessageId uint32 `protobuf:"varint,5,opt,name=message_id,json=messageId,proto3" json:"message_id,omitempty"`
}

func (x *ClientRequest) Reset() {
	*x = ClientRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_common_c2gate_proto_msgTypes[9]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ClientRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ClientRequest) ProtoMessage() {}

func (x *ClientRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_c2gate_proto_msgTypes[9]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ClientRequest.ProtoReflect.Descriptor instead.
func (*ClientRequest) Descriptor() ([]byte, []int) {
	return file_proto_common_c2gate_proto_rawDescGZIP(), []int{9}
}

func (x *ClientRequest) GetId() uint64 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *ClientRequest) GetService() string {
	if x != nil {
		return x.Service
	}
	return ""
}

func (x *ClientRequest) GetMethod() string {
	if x != nil {
		return x.Method
	}
	return ""
}

func (x *ClientRequest) GetBody() []byte {
	if x != nil {
		return x.Body
	}
	return nil
}

func (x *ClientRequest) GetMessageId() uint32 {
	if x != nil {
		return x.MessageId
	}
	return 0
}

var File_proto_common_c2gate_proto protoreflect.FileDescriptor

var file_proto_common_c2gate_proto_rawDesc = []byte{
	0x0a, 0x19, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x63,
	0x32, 0x67, 0x61, 0x74, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x16, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x74, 0x69, 0x70, 0x2e, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f,
	0x6e, 0x2f, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x73, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x4a, 0x0a, 0x1a, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74,
	0x53, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x57, 0x72, 0x61, 0x70,
	0x70, 0x65, 0x72, 0x12, 0x2c, 0x0a, 0x06, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x0b, 0x32, 0x14, 0x2e, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x53, 0x69, 0x6d,
	0x70, 0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x06, 0x70, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x22, 0x44, 0x0a, 0x0c, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73,
	0x74, 0x12, 0x18, 0x0a, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x09, 0x52, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x12, 0x1a, 0x0a, 0x08, 0x70,
	0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x08, 0x70,
	0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x22, 0x7c, 0x0a, 0x0d, 0x4c, 0x6f, 0x67, 0x69, 0x6e,
	0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a, 0x0d, 0x65, 0x72, 0x72, 0x6f,
	0x72, 0x5f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32,
	0x0f, 0x2e, 0x54, 0x69, 0x70, 0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x52, 0x0c, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x12, 0x35,
	0x0a, 0x07, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32,
	0x1b, 0x2e, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x53, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x50,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x57, 0x72, 0x61, 0x70, 0x70, 0x65, 0x72, 0x52, 0x07, 0x70, 0x6c,
	0x61, 0x79, 0x65, 0x72, 0x73, 0x22, 0xb5, 0x01, 0x0a, 0x0c, 0x54, 0x65, 0x73, 0x74, 0x52, 0x65,
	0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a, 0x0d, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f,
	0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e,
	0x54, 0x69, 0x70, 0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x0c,
	0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x12, 0x35, 0x0a, 0x07,
	0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x1b, 0x2e,
	0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x53, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x50, 0x6c, 0x61,
	0x79, 0x65, 0x72, 0x57, 0x72, 0x61, 0x70, 0x70, 0x65, 0x72, 0x52, 0x07, 0x70, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x73, 0x12, 0x1e, 0x0a, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x73, 0x74, 0x72, 0x69, 0x6e,
	0x67, 0x18, 0x03, 0x20, 0x03, 0x28, 0x09, 0x52, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x73, 0x74, 0x72,
	0x69, 0x6e, 0x67, 0x12, 0x18, 0x0a, 0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x74, 0x18, 0x04,
	0x20, 0x03, 0x28, 0x05, 0x52, 0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x74, 0x22, 0x15, 0x0a,
	0x13, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x71,
	0x75, 0x65, 0x73, 0x74, 0x22, 0x83, 0x01, 0x0a, 0x14, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x50,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x34, 0x0a,
	0x0d, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x54, 0x69, 0x70, 0x49, 0x6e, 0x66, 0x6f, 0x4d, 0x65,
	0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x0c, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d, 0x65, 0x73, 0x73,
	0x61, 0x67, 0x65, 0x12, 0x35, 0x0a, 0x07, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x18, 0x02,
	0x20, 0x03, 0x28, 0x0b, 0x32, 0x1b, 0x2e, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x53, 0x69,
	0x6d, 0x70, 0x6c, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x57, 0x72, 0x61, 0x70, 0x70, 0x65,
	0x72, 0x52, 0x07, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x22, 0x2f, 0x0a, 0x10, 0x45, 0x6e,
	0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b,
	0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x22, 0x49, 0x0a, 0x11, 0x45,
	0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65,
	0x12, 0x34, 0x0a, 0x0d, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67,
	0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x54, 0x69, 0x70, 0x49, 0x6e, 0x66,
	0x6f, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x0c, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4d,
	0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x22, 0x12, 0x0a, 0x10, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47,
	0x61, 0x6d, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x22, 0x84, 0x01, 0x0a, 0x0d, 0x43,
	0x6c, 0x69, 0x65, 0x6e, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x0e, 0x0a, 0x02,
	0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x02, 0x69, 0x64, 0x12, 0x18, 0x0a, 0x07,
	0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x73,
	0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x16, 0x0a, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64,
	0x18, 0x03, 0x20, 0x01, 0x28, 0x09, 0x52, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, 0x12, 0x12,
	0x0a, 0x04, 0x62, 0x6f, 0x64, 0x79, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x04, 0x62, 0x6f,
	0x64, 0x79, 0x12, 0x1d, 0x0a, 0x0a, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x5f, 0x69, 0x64,
	0x18, 0x05, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x49,
	0x64, 0x2a, 0x52, 0x0a, 0x11, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x4d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x54, 0x79, 0x70, 0x65, 0x12, 0x12, 0x0a, 0x0e, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54,
	0x5f, 0x52, 0x45, 0x51, 0x55, 0x45, 0x53, 0x54, 0x10, 0x00, 0x12, 0x13, 0x0a, 0x0f, 0x43, 0x4c,
	0x49, 0x45, 0x4e, 0x54, 0x5f, 0x52, 0x45, 0x53, 0x50, 0x4f, 0x4e, 0x53, 0x45, 0x10, 0x01, 0x12,
	0x14, 0x0a, 0x10, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f, 0x52, 0x50, 0x43, 0x5f, 0x45, 0x52,
	0x52, 0x4f, 0x52, 0x10, 0x02, 0x2a, 0xae, 0x01, 0x0a, 0x07, 0x54, 0x69, 0x70, 0x43, 0x6f, 0x64,
	0x65, 0x12, 0x17, 0x0a, 0x13, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f, 0x52, 0x50, 0x43, 0x5f,
	0x4e, 0x4f, 0x5f, 0x45, 0x52, 0x52, 0x4f, 0x52, 0x10, 0x00, 0x12, 0x10, 0x0a, 0x0c, 0x43, 0x4c,
	0x49, 0x45, 0x4e, 0x54, 0x5f, 0x57, 0x52, 0x4f, 0x4e, 0x47, 0x10, 0x01, 0x12, 0x15, 0x0a, 0x11,
	0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f, 0x4e, 0x4f, 0x5f, 0x53, 0x45, 0x52, 0x56, 0x49, 0x43,
	0x45, 0x10, 0x02, 0x12, 0x14, 0x0a, 0x10, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f, 0x4e, 0x4f,
	0x5f, 0x4d, 0x45, 0x54, 0x48, 0x4f, 0x44, 0x10, 0x03, 0x12, 0x1a, 0x0a, 0x16, 0x43, 0x4c, 0x49,
	0x45, 0x4e, 0x54, 0x5f, 0x49, 0x4e, 0x56, 0x41, 0x4c, 0x49, 0x44, 0x5f, 0x52, 0x45, 0x51, 0x55,
	0x45, 0x53, 0x54, 0x10, 0x04, 0x12, 0x1b, 0x0a, 0x17, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f,
	0x49, 0x4e, 0x56, 0x41, 0x4c, 0x49, 0x44, 0x5f, 0x52, 0x45, 0x53, 0x50, 0x4f, 0x4e, 0x53, 0x45,
	0x10, 0x05, 0x12, 0x12, 0x0a, 0x0e, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54, 0x5f, 0x54, 0x49, 0x4d,
	0x45, 0x4f, 0x55, 0x54, 0x10, 0x06, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d,
	0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_common_c2gate_proto_rawDescOnce sync.Once
	file_proto_common_c2gate_proto_rawDescData = file_proto_common_c2gate_proto_rawDesc
)

func file_proto_common_c2gate_proto_rawDescGZIP() []byte {
	file_proto_common_c2gate_proto_rawDescOnce.Do(func() {
		file_proto_common_c2gate_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_common_c2gate_proto_rawDescData)
	})
	return file_proto_common_c2gate_proto_rawDescData
}

var file_proto_common_c2gate_proto_enumTypes = make([]protoimpl.EnumInfo, 2)
var file_proto_common_c2gate_proto_msgTypes = make([]protoimpl.MessageInfo, 10)
var file_proto_common_c2gate_proto_goTypes = []any{
	(ClientMessageType)(0),             // 0: ClientMessageType
	(TipCode)(0),                       // 1: TipCode
	(*AccountSimplePlayerWrapper)(nil), // 2: AccountSimplePlayerWrapper
	(*LoginRequest)(nil),               // 3: LoginRequest
	(*LoginResponse)(nil),              // 4: LoginResponse
	(*TestResponse)(nil),               // 5: TestResponse
	(*CreatePlayerRequest)(nil),        // 6: CreatePlayerRequest
	(*CreatePlayerResponse)(nil),       // 7: CreatePlayerResponse
	(*EnterGameRequest)(nil),           // 8: EnterGameRequest
	(*EnterGameResponse)(nil),          // 9: EnterGameResponse
	(*LeaveGameRequest)(nil),           // 10: LeaveGameRequest
	(*ClientRequest)(nil),              // 11: ClientRequest
	(*AccountSimplePlayer)(nil),        // 12: AccountSimplePlayer
	(*TipInfoMessage)(nil),             // 13: TipInfoMessage
}
var file_proto_common_c2gate_proto_depIdxs = []int32{
	12, // 0: AccountSimplePlayerWrapper.player:type_name -> AccountSimplePlayer
	13, // 1: LoginResponse.error_message:type_name -> TipInfoMessage
	2,  // 2: LoginResponse.players:type_name -> AccountSimplePlayerWrapper
	13, // 3: TestResponse.error_message:type_name -> TipInfoMessage
	2,  // 4: TestResponse.players:type_name -> AccountSimplePlayerWrapper
	13, // 5: CreatePlayerResponse.error_message:type_name -> TipInfoMessage
	2,  // 6: CreatePlayerResponse.players:type_name -> AccountSimplePlayerWrapper
	13, // 7: EnterGameResponse.error_message:type_name -> TipInfoMessage
	8,  // [8:8] is the sub-list for method output_type
	8,  // [8:8] is the sub-list for method input_type
	8,  // [8:8] is the sub-list for extension type_name
	8,  // [8:8] is the sub-list for extension extendee
	0,  // [0:8] is the sub-list for field type_name
}

func init() { file_proto_common_c2gate_proto_init() }
func file_proto_common_c2gate_proto_init() {
	if File_proto_common_c2gate_proto != nil {
		return
	}
	file_proto_common_tip_proto_init()
	file_proto_common_user_accounts_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_common_c2gate_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*AccountSimplePlayerWrapper); i {
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
		file_proto_common_c2gate_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*LoginRequest); i {
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
		file_proto_common_c2gate_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*LoginResponse); i {
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
		file_proto_common_c2gate_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*TestResponse); i {
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
		file_proto_common_c2gate_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*CreatePlayerRequest); i {
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
		file_proto_common_c2gate_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*CreatePlayerResponse); i {
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
		file_proto_common_c2gate_proto_msgTypes[6].Exporter = func(v any, i int) any {
			switch v := v.(*EnterGameRequest); i {
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
		file_proto_common_c2gate_proto_msgTypes[7].Exporter = func(v any, i int) any {
			switch v := v.(*EnterGameResponse); i {
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
		file_proto_common_c2gate_proto_msgTypes[8].Exporter = func(v any, i int) any {
			switch v := v.(*LeaveGameRequest); i {
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
		file_proto_common_c2gate_proto_msgTypes[9].Exporter = func(v any, i int) any {
			switch v := v.(*ClientRequest); i {
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
			RawDescriptor: file_proto_common_c2gate_proto_rawDesc,
			NumEnums:      2,
			NumMessages:   10,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_common_c2gate_proto_goTypes,
		DependencyIndexes: file_proto_common_c2gate_proto_depIdxs,
		EnumInfos:         file_proto_common_c2gate_proto_enumTypes,
		MessageInfos:      file_proto_common_c2gate_proto_msgTypes,
	}.Build()
	File_proto_common_c2gate_proto = out.File
	file_proto_common_c2gate_proto_rawDesc = nil
	file_proto_common_c2gate_proto_goTypes = nil
	file_proto_common_c2gate_proto_depIdxs = nil
}
