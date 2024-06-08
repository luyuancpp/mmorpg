// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.1
// 	protoc        v3.19.4
// source: common_proto/login_service.proto

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

type LoginC2LRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Account     string       `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
	Password    string       `protobuf:"bytes,2,opt,name=password,proto3" json:"password,omitempty"`
	SessionInfo *SessionInfo `protobuf:"bytes,3,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
}

func (x *LoginC2LRequest) Reset() {
	*x = LoginC2LRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginC2LRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginC2LRequest) ProtoMessage() {}

func (x *LoginC2LRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginC2LRequest.ProtoReflect.Descriptor instead.
func (*LoginC2LRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{0}
}

func (x *LoginC2LRequest) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

func (x *LoginC2LRequest) GetPassword() string {
	if x != nil {
		return x.Password
	}
	return ""
}

func (x *LoginC2LRequest) GetSessionInfo() *SessionInfo {
	if x != nil {
		return x.SessionInfo
	}
	return nil
}

type CreatePlayerC2LRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SessionInfo *SessionInfo `protobuf:"bytes,3,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
}

func (x *CreatePlayerC2LRequest) Reset() {
	*x = CreatePlayerC2LRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CreatePlayerC2LRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CreatePlayerC2LRequest) ProtoMessage() {}

func (x *CreatePlayerC2LRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CreatePlayerC2LRequest.ProtoReflect.Descriptor instead.
func (*CreatePlayerC2LRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{1}
}

func (x *CreatePlayerC2LRequest) GetSessionInfo() *SessionInfo {
	if x != nil {
		return x.SessionInfo
	}
	return nil
}

type EnterGameC2LRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId    uint64       `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SessionInfo *SessionInfo `protobuf:"bytes,2,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
}

func (x *EnterGameC2LRequest) Reset() {
	*x = EnterGameC2LRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterGameC2LRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameC2LRequest) ProtoMessage() {}

func (x *EnterGameC2LRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterGameC2LRequest.ProtoReflect.Descriptor instead.
func (*EnterGameC2LRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{2}
}

func (x *EnterGameC2LRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *EnterGameC2LRequest) GetSessionInfo() *SessionInfo {
	if x != nil {
		return x.SessionInfo
	}
	return nil
}

type LoginNodeCreatePlayerResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Error    *Tips            `protobuf:"bytes,1,opt,name=error,proto3" json:"error,omitempty"`
	Players  *AccountDatabase `protobuf:"bytes,2,opt,name=players,proto3" json:"players,omitempty"`
	PlayerId uint64           `protobuf:"varint,3,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *LoginNodeCreatePlayerResponse) Reset() {
	*x = LoginNodeCreatePlayerResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginNodeCreatePlayerResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginNodeCreatePlayerResponse) ProtoMessage() {}

func (x *LoginNodeCreatePlayerResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginNodeCreatePlayerResponse.ProtoReflect.Descriptor instead.
func (*LoginNodeCreatePlayerResponse) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{3}
}

func (x *LoginNodeCreatePlayerResponse) GetError() *Tips {
	if x != nil {
		return x.Error
	}
	return nil
}

func (x *LoginNodeCreatePlayerResponse) GetPlayers() *AccountDatabase {
	if x != nil {
		return x.Players
	}
	return nil
}

func (x *LoginNodeCreatePlayerResponse) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type LoginNodeDisconnectRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SessionId uint64 `protobuf:"varint,1,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
}

func (x *LoginNodeDisconnectRequest) Reset() {
	*x = LoginNodeDisconnectRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginNodeDisconnectRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginNodeDisconnectRequest) ProtoMessage() {}

func (x *LoginNodeDisconnectRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginNodeDisconnectRequest.ProtoReflect.Descriptor instead.
func (*LoginNodeDisconnectRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{4}
}

func (x *LoginNodeDisconnectRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

type LeaveGameC2LRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SessionId uint64 `protobuf:"varint,1,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
}

func (x *LeaveGameC2LRequest) Reset() {
	*x = LeaveGameC2LRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_proto_login_service_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LeaveGameC2LRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LeaveGameC2LRequest) ProtoMessage() {}

func (x *LeaveGameC2LRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_proto_login_service_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LeaveGameC2LRequest.ProtoReflect.Descriptor instead.
func (*LeaveGameC2LRequest) Descriptor() ([]byte, []int) {
	return file_common_proto_login_service_proto_rawDescGZIP(), []int{5}
}

func (x *LeaveGameC2LRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

var File_common_proto_login_service_proto protoreflect.FileDescriptor

var file_common_proto_login_service_proto_rawDesc = []byte{
	0x0a, 0x20, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c,
	0x6f, 0x67, 0x69, 0x6e, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x2e, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x1a, 0x18, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x16, 0x63, 0x6f,
	0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x74, 0x69, 0x70, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x19, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x2f, 0x63, 0x32, 0x67, 0x61, 0x74, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a,
	0x27, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6d, 0x79,
	0x73, 0x71, 0x6c, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x5f, 0x74, 0x61, 0x62,
	0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x1a, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e,
	0x5f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x22, 0x78, 0x0a, 0x0f, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x43, 0x32, 0x4c,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x18, 0x0a, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75,
	0x6e, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e,
	0x74, 0x12, 0x1a, 0x0a, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x18, 0x02, 0x20,
	0x01, 0x28, 0x09, 0x52, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x12, 0x2f, 0x0a,
	0x0c, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x03, 0x20,
	0x01, 0x28, 0x0b, 0x32, 0x0c, 0x2e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66,
	0x6f, 0x52, 0x0b, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0x22, 0x49,
	0x0a, 0x16, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x43, 0x32,
	0x6c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x2f, 0x0a, 0x0c, 0x73, 0x65, 0x73, 0x73,
	0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0c,
	0x2e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0x52, 0x0b, 0x73, 0x65,
	0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0x22, 0x63, 0x0a, 0x13, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x43, 0x32, 0x4c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74,
	0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x12, 0x2f, 0x0a,
	0x0c, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x02, 0x20,
	0x01, 0x28, 0x0b, 0x32, 0x0c, 0x2e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66,
	0x6f, 0x52, 0x0b, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0x22, 0x86,
	0x01, 0x0a, 0x1d, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x43, 0x72, 0x65, 0x61,
	0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65,
	0x12, 0x1b, 0x0a, 0x05, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32,
	0x05, 0x2e, 0x54, 0x69, 0x70, 0x73, 0x52, 0x05, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x2b, 0x0a,
	0x07, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x11,
	0x2e, 0x61, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73,
	0x65, 0x52, 0x07, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x73, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c,
	0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x22, 0x3b, 0x0a, 0x1a, 0x4c, 0x6f, 0x67, 0x69, 0x6e,
	0x4e, 0x6f, 0x64, 0x65, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52, 0x65,
	0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e,
	0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x73, 0x65, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x49, 0x64, 0x22, 0x34, 0x0a, 0x13, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47, 0x61, 0x6d,
	0x65, 0x43, 0x32, 0x4c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1d, 0x0a, 0x0a, 0x73,
	0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52,
	0x09, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x32, 0x97, 0x02, 0x0a, 0x0c, 0x4c,
	0x6f, 0x67, 0x69, 0x6e, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x29, 0x0a, 0x05, 0x4c,
	0x6f, 0x67, 0x69, 0x6e, 0x12, 0x10, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x43, 0x32, 0x4c, 0x52,
	0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x0e, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x52, 0x65,
	0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x47, 0x0a, 0x0c, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x12, 0x17, 0x2e, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x50,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x43, 0x32, 0x6c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a,
	0x1e, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x43, 0x72, 0x65, 0x61, 0x74,
	0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12,
	0x35, 0x0a, 0x09, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x12, 0x14, 0x2e, 0x45,
	0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x43, 0x32, 0x4c, 0x52, 0x65, 0x71, 0x75, 0x65,
	0x73, 0x74, 0x1a, 0x12, 0x2e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x65,
	0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x29, 0x0a, 0x09, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47,
	0x61, 0x6d, 0x65, 0x12, 0x14, 0x2e, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47, 0x61, 0x6d, 0x65, 0x43,
	0x32, 0x4c, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74,
	0x79, 0x12, 0x31, 0x0a, 0x0a, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x12,
	0x1b, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x44, 0x69, 0x73, 0x63, 0x6f,
	0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45,
	0x6d, 0x70, 0x74, 0x79, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62,
	0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_common_proto_login_service_proto_rawDescOnce sync.Once
	file_common_proto_login_service_proto_rawDescData = file_common_proto_login_service_proto_rawDesc
)

func file_common_proto_login_service_proto_rawDescGZIP() []byte {
	file_common_proto_login_service_proto_rawDescOnce.Do(func() {
		file_common_proto_login_service_proto_rawDescData = protoimpl.X.CompressGZIP(file_common_proto_login_service_proto_rawDescData)
	})
	return file_common_proto_login_service_proto_rawDescData
}

var file_common_proto_login_service_proto_msgTypes = make([]protoimpl.MessageInfo, 6)
var file_common_proto_login_service_proto_goTypes = []interface{}{
	(*LoginC2LRequest)(nil),               // 0: LoginC2LRequest
	(*CreatePlayerC2LRequest)(nil),        // 1: CreatePlayerC2lRequest
	(*EnterGameC2LRequest)(nil),           // 2: EnterGameC2LRequest
	(*LoginNodeCreatePlayerResponse)(nil), // 3: LoginNodeCreatePlayerResponse
	(*LoginNodeDisconnectRequest)(nil),    // 4: LoginNodeDisconnectRequest
	(*LeaveGameC2LRequest)(nil),           // 5: LeaveGameC2LRequest
	(*SessionInfo)(nil),                   // 6: SessionInfo
	(*Tips)(nil),                          // 7: Tips
	(*AccountDatabase)(nil),               // 8: account_database
	(*LoginResponse)(nil),                 // 9: LoginResponse
	(*EnterGameResponse)(nil),             // 10: EnterGameResponse
	(*Empty)(nil),                         // 11: Empty
}
var file_common_proto_login_service_proto_depIdxs = []int32{
	6,  // 0: LoginC2LRequest.session_info:type_name -> SessionInfo
	6,  // 1: CreatePlayerC2lRequest.session_info:type_name -> SessionInfo
	6,  // 2: EnterGameC2LRequest.session_info:type_name -> SessionInfo
	7,  // 3: LoginNodeCreatePlayerResponse.error:type_name -> Tips
	8,  // 4: LoginNodeCreatePlayerResponse.players:type_name -> account_database
	0,  // 5: LoginService.Login:input_type -> LoginC2LRequest
	1,  // 6: LoginService.CreatePlayer:input_type -> CreatePlayerC2lRequest
	2,  // 7: LoginService.EnterGame:input_type -> EnterGameC2LRequest
	5,  // 8: LoginService.LeaveGame:input_type -> LeaveGameC2LRequest
	4,  // 9: LoginService.Disconnect:input_type -> LoginNodeDisconnectRequest
	9,  // 10: LoginService.Login:output_type -> LoginResponse
	3,  // 11: LoginService.CreatePlayer:output_type -> LoginNodeCreatePlayerResponse
	10, // 12: LoginService.EnterGame:output_type -> EnterGameResponse
	11, // 13: LoginService.LeaveGame:output_type -> Empty
	11, // 14: LoginService.Disconnect:output_type -> Empty
	10, // [10:15] is the sub-list for method output_type
	5,  // [5:10] is the sub-list for method input_type
	5,  // [5:5] is the sub-list for extension type_name
	5,  // [5:5] is the sub-list for extension extendee
	0,  // [0:5] is the sub-list for field type_name
}

func init() { file_common_proto_login_service_proto_init() }
func file_common_proto_login_service_proto_init() {
	if File_common_proto_login_service_proto != nil {
		return
	}
	file_common_proto_empty_proto_init()
	file_common_proto_tip_proto_init()
	file_common_proto_c2gate_proto_init()
	file_common_proto_mysql_database_table_proto_init()
	file_common_proto_session_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_common_proto_login_service_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoginC2LRequest); i {
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
		file_common_proto_login_service_proto_msgTypes[1].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*CreatePlayerC2LRequest); i {
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
		file_common_proto_login_service_proto_msgTypes[2].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*EnterGameC2LRequest); i {
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
		file_common_proto_login_service_proto_msgTypes[3].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoginNodeCreatePlayerResponse); i {
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
		file_common_proto_login_service_proto_msgTypes[4].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LoginNodeDisconnectRequest); i {
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
		file_common_proto_login_service_proto_msgTypes[5].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*LeaveGameC2LRequest); i {
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
			RawDescriptor: file_common_proto_login_service_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   6,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_common_proto_login_service_proto_goTypes,
		DependencyIndexes: file_common_proto_login_service_proto_depIdxs,
		MessageInfos:      file_common_proto_login_service_proto_msgTypes,
	}.Build()
	File_common_proto_login_service_proto = out.File
	file_common_proto_login_service_proto_rawDesc = nil
	file_common_proto_login_service_proto_goTypes = nil
	file_common_proto_login_service_proto_depIdxs = nil
}