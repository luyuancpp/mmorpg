// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: common/game_service.proto

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

type GameNodeEnterGsRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId     uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SessionId    uint64 `protobuf:"varint,2,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	CentreNodeId uint32 `protobuf:"varint,3,opt,name=centre_node_id,json=centreNodeId,proto3" json:"centre_node_id,omitempty"`
}

func (x *GameNodeEnterGsRequest) Reset() {
	*x = GameNodeEnterGsRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameNodeEnterGsRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameNodeEnterGsRequest) ProtoMessage() {}

func (x *GameNodeEnterGsRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameNodeEnterGsRequest.ProtoReflect.Descriptor instead.
func (*GameNodeEnterGsRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{0}
}

func (x *GameNodeEnterGsRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *GameNodeEnterGsRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *GameNodeEnterGsRequest) GetCentreNodeId() uint32 {
	if x != nil {
		return x.CentreNodeId
	}
	return 0
}

type GameNodeRpcClientRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Body      []byte `protobuf:"bytes,1,opt,name=body,proto3" json:"body,omitempty"`
	SessionId uint64 `protobuf:"varint,2,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	MessageId uint32 `protobuf:"varint,3,opt,name=message_id,json=messageId,proto3" json:"message_id,omitempty"`
	Id        uint64 `protobuf:"varint,4,opt,name=id,proto3" json:"id,omitempty"`
}

func (x *GameNodeRpcClientRequest) Reset() {
	*x = GameNodeRpcClientRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameNodeRpcClientRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameNodeRpcClientRequest) ProtoMessage() {}

func (x *GameNodeRpcClientRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameNodeRpcClientRequest.ProtoReflect.Descriptor instead.
func (*GameNodeRpcClientRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{1}
}

func (x *GameNodeRpcClientRequest) GetBody() []byte {
	if x != nil {
		return x.Body
	}
	return nil
}

func (x *GameNodeRpcClientRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *GameNodeRpcClientRequest) GetMessageId() uint32 {
	if x != nil {
		return x.MessageId
	}
	return 0
}

func (x *GameNodeRpcClientRequest) GetId() uint64 {
	if x != nil {
		return x.Id
	}
	return 0
}

type GameNodeRpcClientResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Response  []byte `protobuf:"bytes,1,opt,name=response,proto3" json:"response,omitempty"`
	SessionId uint64 `protobuf:"varint,2,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	MessageId uint32 `protobuf:"varint,3,opt,name=message_id,json=messageId,proto3" json:"message_id,omitempty"`
	Id        uint64 `protobuf:"varint,4,opt,name=id,proto3" json:"id,omitempty"`
}

func (x *GameNodeRpcClientResponse) Reset() {
	*x = GameNodeRpcClientResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameNodeRpcClientResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameNodeRpcClientResponse) ProtoMessage() {}

func (x *GameNodeRpcClientResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameNodeRpcClientResponse.ProtoReflect.Descriptor instead.
func (*GameNodeRpcClientResponse) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{2}
}

func (x *GameNodeRpcClientResponse) GetResponse() []byte {
	if x != nil {
		return x.Response
	}
	return nil
}

func (x *GameNodeRpcClientResponse) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *GameNodeRpcClientResponse) GetMessageId() uint32 {
	if x != nil {
		return x.MessageId
	}
	return 0
}

func (x *GameNodeRpcClientResponse) GetId() uint64 {
	if x != nil {
		return x.Id
	}
	return 0
}

type GameNodeDisconnectRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *GameNodeDisconnectRequest) Reset() {
	*x = GameNodeDisconnectRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameNodeDisconnectRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameNodeDisconnectRequest) ProtoMessage() {}

func (x *GameNodeDisconnectRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameNodeDisconnectRequest.ProtoReflect.Descriptor instead.
func (*GameNodeDisconnectRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{3}
}

func (x *GameNodeDisconnectRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type GameNodeConnectRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	RpcClient  *IpPort `protobuf:"bytes,1,opt,name=rpc_client,json=rpcClient,proto3" json:"rpc_client,omitempty"`
	GateNodeId uint32  `protobuf:"varint,2,opt,name=gate_node_id,json=gateNodeId,proto3" json:"gate_node_id,omitempty"`
}

func (x *GameNodeConnectRequest) Reset() {
	*x = GameNodeConnectRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameNodeConnectRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameNodeConnectRequest) ProtoMessage() {}

func (x *GameNodeConnectRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameNodeConnectRequest.ProtoReflect.Descriptor instead.
func (*GameNodeConnectRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{4}
}

func (x *GameNodeConnectRequest) GetRpcClient() *IpPort {
	if x != nil {
		return x.RpcClient
	}
	return nil
}

func (x *GameNodeConnectRequest) GetGateNodeId() uint32 {
	if x != nil {
		return x.GateNodeId
	}
	return 0
}

type RegisterPlayerSessionRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SessionId uint64 `protobuf:"varint,1,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	PlayerId  uint64 `protobuf:"varint,2,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
}

func (x *RegisterPlayerSessionRequest) Reset() {
	*x = RegisterPlayerSessionRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *RegisterPlayerSessionRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*RegisterPlayerSessionRequest) ProtoMessage() {}

func (x *RegisterPlayerSessionRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use RegisterPlayerSessionRequest.ProtoReflect.Descriptor instead.
func (*RegisterPlayerSessionRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{5}
}

func (x *RegisterPlayerSessionRequest) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *RegisterPlayerSessionRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

type Centre2GsEnterSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SceneId  uint64 `protobuf:"varint,2,opt,name=scene_id,json=sceneId,proto3" json:"scene_id,omitempty"`
}

func (x *Centre2GsEnterSceneRequest) Reset() {
	*x = Centre2GsEnterSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[6]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Centre2GsEnterSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Centre2GsEnterSceneRequest) ProtoMessage() {}

func (x *Centre2GsEnterSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[6]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Centre2GsEnterSceneRequest.ProtoReflect.Descriptor instead.
func (*Centre2GsEnterSceneRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{6}
}

func (x *Centre2GsEnterSceneRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *Centre2GsEnterSceneRequest) GetSceneId() uint64 {
	if x != nil {
		return x.SceneId
	}
	return 0
}

type CreateSceneRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ConfigId uint32 `protobuf:"varint,1,opt,name=config_id,json=configId,proto3" json:"config_id,omitempty"`
}

func (x *CreateSceneRequest) Reset() {
	*x = CreateSceneRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[7]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CreateSceneRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CreateSceneRequest) ProtoMessage() {}

func (x *CreateSceneRequest) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[7]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CreateSceneRequest.ProtoReflect.Descriptor instead.
func (*CreateSceneRequest) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{7}
}

func (x *CreateSceneRequest) GetConfigId() uint32 {
	if x != nil {
		return x.ConfigId
	}
	return 0
}

type CreateSceneResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	SceneInfo *SceneInfo `protobuf:"bytes,1,opt,name=scene_info,json=sceneInfo,proto3" json:"scene_info,omitempty"`
}

func (x *CreateSceneResponse) Reset() {
	*x = CreateSceneResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_common_game_service_proto_msgTypes[8]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CreateSceneResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CreateSceneResponse) ProtoMessage() {}

func (x *CreateSceneResponse) ProtoReflect() protoreflect.Message {
	mi := &file_common_game_service_proto_msgTypes[8]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CreateSceneResponse.ProtoReflect.Descriptor instead.
func (*CreateSceneResponse) Descriptor() ([]byte, []int) {
	return file_common_game_service_proto_rawDescGZIP(), []int{8}
}

func (x *CreateSceneResponse) GetSceneInfo() *SceneInfo {
	if x != nil {
		return x.SceneInfo
	}
	return nil
}

var File_common_game_service_proto protoreflect.FileDescriptor

var file_common_game_service_proto_rawDesc = []byte{
	0x0a, 0x19, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x5f, 0x73, 0x65,
	0x72, 0x76, 0x69, 0x63, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x12, 0x63, 0x6f, 0x6d,
	0x6d, 0x6f, 0x6e, 0x2f, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a,
	0x13, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x18, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x6e, 0x6f, 0x64,
	0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x20,
	0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f,
	0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x22, 0x7a, 0x0a, 0x16, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x45, 0x6e, 0x74, 0x65,
	0x72, 0x47, 0x73, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c,
	0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x65, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x73, 0x65, 0x73,
	0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x12, 0x24, 0x0a, 0x0e, 0x63, 0x65, 0x6e, 0x74, 0x72, 0x65,
	0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0c,
	0x63, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x22, 0x7c, 0x0a, 0x18,
	0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x70, 0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e,
	0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x12, 0x0a, 0x04, 0x62, 0x6f, 0x64, 0x79,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x04, 0x62, 0x6f, 0x64, 0x79, 0x12, 0x1d, 0x0a, 0x0a,
	0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04,
	0x52, 0x09, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x12, 0x1d, 0x0a, 0x0a, 0x6d,
	0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x09, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x49, 0x64, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64,
	0x18, 0x04, 0x20, 0x01, 0x28, 0x04, 0x52, 0x02, 0x69, 0x64, 0x22, 0x85, 0x01, 0x0a, 0x19, 0x47,
	0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x70, 0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74,
	0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x1a, 0x0a, 0x08, 0x72, 0x65, 0x73, 0x70,
	0x6f, 0x6e, 0x73, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x08, 0x72, 0x65, 0x73, 0x70,
	0x6f, 0x6e, 0x73, 0x65, 0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f,
	0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f,
	0x6e, 0x49, 0x64, 0x12, 0x1d, 0x0a, 0x0a, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x5f, 0x69,
	0x64, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x49, 0x64, 0x12, 0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x04, 0x20, 0x01, 0x28, 0x04, 0x52, 0x02,
	0x69, 0x64, 0x22, 0x38, 0x0a, 0x19, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x44, 0x69,
	0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12,
	0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x22, 0x62, 0x0a, 0x16,
	0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52,
	0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x26, 0x0a, 0x0a, 0x72, 0x70, 0x63, 0x5f, 0x63, 0x6c,
	0x69, 0x65, 0x6e, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x07, 0x2e, 0x49, 0x70, 0x50,
	0x6f, 0x72, 0x74, 0x52, 0x09, 0x72, 0x70, 0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x12, 0x20,
	0x0a, 0x0c, 0x67, 0x61, 0x74, 0x65, 0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x67, 0x61, 0x74, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x49, 0x64,
	0x22, 0x5a, 0x0a, 0x1c, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x50, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74,
	0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x69, 0x64, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x04, 0x52, 0x09, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x12,
	0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x22, 0x54, 0x0a, 0x1a,
	0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x32, 0x47, 0x73, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63,
	0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c,
	0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64, 0x12, 0x19, 0x0a, 0x08, 0x73, 0x63, 0x65, 0x6e, 0x65,
	0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04, 0x52, 0x07, 0x73, 0x63, 0x65, 0x6e, 0x65,
	0x49, 0x64, 0x22, 0x31, 0x0a, 0x12, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x53, 0x63, 0x65, 0x6e,
	0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x63, 0x6f, 0x6e, 0x66,
	0x69, 0x67, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08, 0x63, 0x6f, 0x6e,
	0x66, 0x69, 0x67, 0x49, 0x64, 0x22, 0x40, 0x0a, 0x13, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x53,
	0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x29, 0x0a, 0x0a,
	0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x69, 0x6e, 0x66, 0x6f, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b,
	0x32, 0x0a, 0x2e, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x49, 0x6e, 0x66, 0x6f, 0x52, 0x09, 0x73, 0x63,
	0x65, 0x6e, 0x65, 0x49, 0x6e, 0x66, 0x6f, 0x32, 0xe9, 0x05, 0x0a, 0x0b, 0x47, 0x61, 0x6d, 0x65,
	0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x2a, 0x0a, 0x07, 0x45, 0x6e, 0x74, 0x65, 0x72,
	0x47, 0x73, 0x12, 0x17, 0x2e, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x47, 0x73, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d,
	0x70, 0x74, 0x79, 0x12, 0x42, 0x0a, 0x0b, 0x53, 0x65, 0x6e, 0x64, 0x32, 0x50, 0x6c, 0x61, 0x79,
	0x65, 0x72, 0x12, 0x18, 0x2e, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65,
	0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x19, 0x2e, 0x4e,
	0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52,
	0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x4a, 0x0a, 0x11, 0x43, 0x6c, 0x69, 0x65, 0x6e,
	0x74, 0x53, 0x65, 0x6e, 0x64, 0x32, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x12, 0x19, 0x2e, 0x47,
	0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x70, 0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x1a, 0x2e, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f,
	0x64, 0x65, 0x52, 0x70, 0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x52, 0x65, 0x73, 0x70, 0x6f,
	0x6e, 0x73, 0x65, 0x12, 0x30, 0x0a, 0x0a, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63,
	0x74, 0x12, 0x1a, 0x2e, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x44, 0x69, 0x73, 0x63,
	0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x2c, 0x0a, 0x0c, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65,
	0x72, 0x47, 0x61, 0x74, 0x65, 0x12, 0x14, 0x2e, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72,
	0x47, 0x61, 0x74, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d,
	0x70, 0x74, 0x79, 0x12, 0x3a, 0x0a, 0x16, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x53, 0x65, 0x6e,
	0x64, 0x32, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x56, 0x69, 0x61, 0x47, 0x73, 0x12, 0x18, 0x2e,
	0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12,
	0x41, 0x0a, 0x0a, 0x43, 0x61, 0x6c, 0x6c, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x12, 0x18, 0x2e,
	0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x19, 0x2e, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f,
	0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e,
	0x73, 0x65, 0x12, 0x45, 0x0a, 0x12, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x53,
	0x74, 0x72, 0x69, 0x6e, 0x67, 0x4d, 0x73, 0x67, 0x12, 0x16, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65,
	0x4d, 0x73, 0x67, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74,
	0x1a, 0x17, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x73, 0x67, 0x53, 0x74, 0x72, 0x69, 0x6e,
	0x67, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x53, 0x0a, 0x14, 0x52, 0x6f, 0x75,
	0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x4d, 0x73,
	0x67, 0x12, 0x1c, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d,
	0x73, 0x67, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a,
	0x1d, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d, 0x73, 0x67,
	0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x36,
	0x0a, 0x0d, 0x55, 0x70, 0x64, 0x61, 0x74, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x12,
	0x1d, 0x2e, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06,
	0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x31, 0x0a, 0x0a, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x53,
	0x63, 0x65, 0x6e, 0x65, 0x12, 0x1b, 0x2e, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x32, 0x47, 0x73,
	0x45, 0x6e, 0x74, 0x65, 0x72, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73,
	0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x38, 0x0a, 0x0b, 0x43, 0x72, 0x65,
	0x61, 0x74, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x12, 0x13, 0x2e, 0x43, 0x72, 0x65, 0x61, 0x74,
	0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x14, 0x2e,
	0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x53, 0x63, 0x65, 0x6e, 0x65, 0x52, 0x65, 0x73, 0x70, 0x6f,
	0x6e, 0x73, 0x65, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x80, 0x01,
	0x01, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_common_game_service_proto_rawDescOnce sync.Once
	file_common_game_service_proto_rawDescData = file_common_game_service_proto_rawDesc
)

func file_common_game_service_proto_rawDescGZIP() []byte {
	file_common_game_service_proto_rawDescOnce.Do(func() {
		file_common_game_service_proto_rawDescData = protoimpl.X.CompressGZIP(file_common_game_service_proto_rawDescData)
	})
	return file_common_game_service_proto_rawDescData
}

var file_common_game_service_proto_msgTypes = make([]protoimpl.MessageInfo, 9)
var file_common_game_service_proto_goTypes = []any{
	(*GameNodeEnterGsRequest)(nil),       // 0: GameNodeEnterGsRequest
	(*GameNodeRpcClientRequest)(nil),     // 1: GameNodeRpcClientRequest
	(*GameNodeRpcClientResponse)(nil),    // 2: GameNodeRpcClientResponse
	(*GameNodeDisconnectRequest)(nil),    // 3: GameNodeDisconnectRequest
	(*GameNodeConnectRequest)(nil),       // 4: GameNodeConnectRequest
	(*RegisterPlayerSessionRequest)(nil), // 5: RegisterPlayerSessionRequest
	(*Centre2GsEnterSceneRequest)(nil),   // 6: Centre2GsEnterSceneRequest
	(*CreateSceneRequest)(nil),           // 7: CreateSceneRequest
	(*CreateSceneResponse)(nil),          // 8: CreateSceneResponse
	(*IpPort)(nil),                       // 9: IpPort
	(*SceneInfo)(nil),                    // 10: SceneInfo
	(*NodeRouteMessageRequest)(nil),      // 11: NodeRouteMessageRequest
	(*RegisterGateRequest)(nil),          // 12: RegisterGateRequest
	(*RouteMsgStringRequest)(nil),        // 13: RouteMsgStringRequest
	(*RoutePlayerMsgStringRequest)(nil),  // 14: RoutePlayerMsgStringRequest
	(*Empty)(nil),                        // 15: Empty
	(*NodeRouteMessageResponse)(nil),     // 16: NodeRouteMessageResponse
	(*RouteMsgStringResponse)(nil),       // 17: RouteMsgStringResponse
	(*RoutePlayerMsgStringResponse)(nil), // 18: RoutePlayerMsgStringResponse
}
var file_common_game_service_proto_depIdxs = []int32{
	9,  // 0: GameNodeConnectRequest.rpc_client:type_name -> IpPort
	10, // 1: CreateSceneResponse.scene_info:type_name -> SceneInfo
	0,  // 2: GameService.EnterGs:input_type -> GameNodeEnterGsRequest
	11, // 3: GameService.Send2Player:input_type -> NodeRouteMessageRequest
	1,  // 4: GameService.ClientSend2Player:input_type -> GameNodeRpcClientRequest
	3,  // 5: GameService.Disconnect:input_type -> GameNodeDisconnectRequest
	12, // 6: GameService.RegisterGate:input_type -> RegisterGateRequest
	11, // 7: GameService.CentreSend2PlayerViaGs:input_type -> NodeRouteMessageRequest
	11, // 8: GameService.CallPlayer:input_type -> NodeRouteMessageRequest
	13, // 9: GameService.RouteNodeStringMsg:input_type -> RouteMsgStringRequest
	14, // 10: GameService.RoutePlayerStringMsg:input_type -> RoutePlayerMsgStringRequest
	5,  // 11: GameService.UpdateSession:input_type -> RegisterPlayerSessionRequest
	6,  // 12: GameService.EnterScene:input_type -> Centre2GsEnterSceneRequest
	7,  // 13: GameService.CreateScene:input_type -> CreateSceneRequest
	15, // 14: GameService.EnterGs:output_type -> Empty
	16, // 15: GameService.Send2Player:output_type -> NodeRouteMessageResponse
	2,  // 16: GameService.ClientSend2Player:output_type -> GameNodeRpcClientResponse
	15, // 17: GameService.Disconnect:output_type -> Empty
	15, // 18: GameService.RegisterGate:output_type -> Empty
	15, // 19: GameService.CentreSend2PlayerViaGs:output_type -> Empty
	16, // 20: GameService.CallPlayer:output_type -> NodeRouteMessageResponse
	17, // 21: GameService.RouteNodeStringMsg:output_type -> RouteMsgStringResponse
	18, // 22: GameService.RoutePlayerStringMsg:output_type -> RoutePlayerMsgStringResponse
	15, // 23: GameService.UpdateSession:output_type -> Empty
	15, // 24: GameService.EnterScene:output_type -> Empty
	8,  // 25: GameService.CreateScene:output_type -> CreateSceneResponse
	14, // [14:26] is the sub-list for method output_type
	2,  // [2:14] is the sub-list for method input_type
	2,  // [2:2] is the sub-list for extension type_name
	2,  // [2:2] is the sub-list for extension extendee
	0,  // [0:2] is the sub-list for field type_name
}

func init() { file_common_game_service_proto_init() }
func file_common_game_service_proto_init() {
	if File_common_game_service_proto != nil {
		return
	}
	file_common_empty_proto_init()
	file_common_common_proto_init()
	file_common_node_common_proto_init()
	file_logic_component_scene_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_common_game_service_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*GameNodeEnterGsRequest); i {
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
		file_common_game_service_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*GameNodeRpcClientRequest); i {
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
		file_common_game_service_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*GameNodeRpcClientResponse); i {
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
		file_common_game_service_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*GameNodeDisconnectRequest); i {
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
		file_common_game_service_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*GameNodeConnectRequest); i {
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
		file_common_game_service_proto_msgTypes[5].Exporter = func(v any, i int) any {
			switch v := v.(*RegisterPlayerSessionRequest); i {
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
		file_common_game_service_proto_msgTypes[6].Exporter = func(v any, i int) any {
			switch v := v.(*Centre2GsEnterSceneRequest); i {
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
		file_common_game_service_proto_msgTypes[7].Exporter = func(v any, i int) any {
			switch v := v.(*CreateSceneRequest); i {
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
		file_common_game_service_proto_msgTypes[8].Exporter = func(v any, i int) any {
			switch v := v.(*CreateSceneResponse); i {
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
			RawDescriptor: file_common_game_service_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   9,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_common_game_service_proto_goTypes,
		DependencyIndexes: file_common_game_service_proto_depIdxs,
		MessageInfos:      file_common_game_service_proto_msgTypes,
	}.Build()
	File_common_game_service_proto = out.File
	file_common_game_service_proto_rawDesc = nil
	file_common_game_service_proto_goTypes = nil
	file_common_game_service_proto_depIdxs = nil
}
