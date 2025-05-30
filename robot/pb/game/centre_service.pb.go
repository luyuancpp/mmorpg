// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/centre/centre_service.proto

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

type GateClientMessageRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	RpcClient *NetworkAddress `protobuf:"bytes,1,opt,name=rpc_client,json=rpcClient,proto3" json:"rpc_client,omitempty"`
}

func (x *GateClientMessageRequest) Reset() {
	*x = GateClientMessageRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_centre_centre_service_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GateClientMessageRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GateClientMessageRequest) ProtoMessage() {}

func (x *GateClientMessageRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GateClientMessageRequest.ProtoReflect.Descriptor instead.
func (*GateClientMessageRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{0}
}

func (x *GateClientMessageRequest) GetRpcClient() *NetworkAddress {
	if x != nil {
		return x.RpcClient
	}
	return nil
}

type LoginNodeLeaveGameRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields
}

func (x *LoginNodeLeaveGameRequest) Reset() {
	*x = LoginNodeLeaveGameRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_centre_centre_service_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *LoginNodeLeaveGameRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginNodeLeaveGameRequest) ProtoMessage() {}

func (x *LoginNodeLeaveGameRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use LoginNodeLeaveGameRequest.ProtoReflect.Descriptor instead.
func (*LoginNodeLeaveGameRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{1}
}

type EnterGameNodeSuccessRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	PlayerId    uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SceneNodeId uint32 `protobuf:"varint,2,opt,name=scene_node_id,json=sceneNodeId,proto3" json:"scene_node_id,omitempty"`
}

func (x *EnterGameNodeSuccessRequest) Reset() {
	*x = EnterGameNodeSuccessRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_centre_centre_service_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *EnterGameNodeSuccessRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameNodeSuccessRequest) ProtoMessage() {}

func (x *EnterGameNodeSuccessRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EnterGameNodeSuccessRequest.ProtoReflect.Descriptor instead.
func (*EnterGameNodeSuccessRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{2}
}

func (x *EnterGameNodeSuccessRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *EnterGameNodeSuccessRequest) GetSceneNodeId() uint32 {
	if x != nil {
		return x.SceneNodeId
	}
	return 0
}

type CentrePlayerGameNodeEntryRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ClientMsgBody *EnterGameRequest `protobuf:"bytes,1,opt,name=client_msg_body,json=clientMsgBody,proto3" json:"client_msg_body,omitempty"`
	SessionInfo   *SessionDetails   `protobuf:"bytes,2,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
}

func (x *CentrePlayerGameNodeEntryRequest) Reset() {
	*x = CentrePlayerGameNodeEntryRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_centre_centre_service_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CentrePlayerGameNodeEntryRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CentrePlayerGameNodeEntryRequest) ProtoMessage() {}

func (x *CentrePlayerGameNodeEntryRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CentrePlayerGameNodeEntryRequest.ProtoReflect.Descriptor instead.
func (*CentrePlayerGameNodeEntryRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{3}
}

func (x *CentrePlayerGameNodeEntryRequest) GetClientMsgBody() *EnterGameRequest {
	if x != nil {
		return x.ClientMsgBody
	}
	return nil
}

func (x *CentrePlayerGameNodeEntryRequest) GetSessionInfo() *SessionDetails {
	if x != nil {
		return x.SessionInfo
	}
	return nil
}

// 新增：SceneNode 初始化请求
type InitSceneNodeRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	NodeId        uint32 `protobuf:"varint,1,opt,name=node_id,json=nodeId,proto3" json:"node_id,omitempty"`
	SceneNodeType uint32 `protobuf:"varint,2,opt,name=scene_node_type,json=sceneNodeType,proto3" json:"scene_node_type,omitempty"`
}

func (x *InitSceneNodeRequest) Reset() {
	*x = InitSceneNodeRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_centre_centre_service_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *InitSceneNodeRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*InitSceneNodeRequest) ProtoMessage() {}

func (x *InitSceneNodeRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use InitSceneNodeRequest.ProtoReflect.Descriptor instead.
func (*InitSceneNodeRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{4}
}

func (x *InitSceneNodeRequest) GetNodeId() uint32 {
	if x != nil {
		return x.NodeId
	}
	return 0
}

func (x *InitSceneNodeRequest) GetSceneNodeType() uint32 {
	if x != nil {
		return x.SceneNodeType
	}
	return 0
}

var File_proto_centre_centre_service_proto protoreflect.FileDescriptor

var file_proto_centre_centre_service_proto_rawDesc = []byte{
	0x0a, 0x21, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x2f, 0x63,
	0x65, 0x6e, 0x74, 0x72, 0x65, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x2e, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x1a, 0x19, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f,
	0x6e, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x18,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x65, 0x6d, 0x70,
	0x74, 0x79, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x1a, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f,
	0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x2e, 0x70,
	0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x19, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d,
	0x6f, 0x6e, 0x2f, 0x63, 0x32, 0x67, 0x61, 0x74, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a,
	0x1a, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x2f, 0x6d, 0x65,
	0x73, 0x73, 0x61, 0x67, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x4a, 0x0a, 0x18, 0x47,
	0x61, 0x74, 0x65, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x2e, 0x0a, 0x0a, 0x72, 0x70, 0x63, 0x5f, 0x63,
	0x6c, 0x69, 0x65, 0x6e, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x4e, 0x65,
	0x74, 0x77, 0x6f, 0x72, 0x6b, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x52, 0x09, 0x72, 0x70,
	0x63, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x22, 0x1b, 0x0a, 0x19, 0x4c, 0x6f, 0x67, 0x69, 0x6e,
	0x4e, 0x6f, 0x64, 0x65, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x65, 0x71,
	0x75, 0x65, 0x73, 0x74, 0x22, 0x5e, 0x0a, 0x1b, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d,
	0x65, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x75, 0x63, 0x63, 0x65, 0x73, 0x73, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x12, 0x1b, 0x0a, 0x09, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x5f, 0x69, 0x64,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x08, 0x70, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x49, 0x64,
	0x12, 0x22, 0x0a, 0x0d, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x69,
	0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0b, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x4e, 0x6f,
	0x64, 0x65, 0x49, 0x64, 0x22, 0x91, 0x01, 0x0a, 0x20, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x50,
	0x6c, 0x61, 0x79, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x45, 0x6e, 0x74,
	0x72, 0x79, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x39, 0x0a, 0x0f, 0x63, 0x6c, 0x69,
	0x65, 0x6e, 0x74, 0x5f, 0x6d, 0x73, 0x67, 0x5f, 0x62, 0x6f, 0x64, 0x79, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x0b, 0x32, 0x11, 0x2e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x65,
	0x71, 0x75, 0x65, 0x73, 0x74, 0x52, 0x0d, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x4d, 0x73, 0x67,
	0x42, 0x6f, 0x64, 0x79, 0x12, 0x32, 0x0a, 0x0c, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f,
	0x69, 0x6e, 0x66, 0x6f, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0f, 0x2e, 0x53, 0x65, 0x73,
	0x73, 0x69, 0x6f, 0x6e, 0x44, 0x65, 0x74, 0x61, 0x69, 0x6c, 0x73, 0x52, 0x0b, 0x73, 0x65, 0x73,
	0x73, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0x22, 0x57, 0x0a, 0x14, 0x49, 0x6e, 0x69, 0x74,
	0x53, 0x63, 0x65, 0x6e, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74,
	0x12, 0x17, 0x0a, 0x07, 0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x0d, 0x52, 0x06, 0x6e, 0x6f, 0x64, 0x65, 0x49, 0x64, 0x12, 0x26, 0x0a, 0x0f, 0x73, 0x63, 0x65,
	0x6e, 0x65, 0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x0d, 0x52, 0x0d, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x54, 0x79, 0x70,
	0x65, 0x32, 0x93, 0x06, 0x0a, 0x0d, 0x43, 0x65, 0x6e, 0x74, 0x72, 0x65, 0x53, 0x65, 0x72, 0x76,
	0x69, 0x63, 0x65, 0x12, 0x36, 0x0a, 0x11, 0x47, 0x61, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x19, 0x2e, 0x47, 0x61, 0x74, 0x65, 0x43,
	0x6c, 0x69, 0x65, 0x6e, 0x74, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x3e, 0x0a, 0x15, 0x47,
	0x61, 0x74, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e,
	0x6e, 0x65, 0x63, 0x74, 0x12, 0x1d, 0x2e, 0x47, 0x61, 0x74, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x36, 0x0a, 0x15, 0x4c,
	0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x4c,
	0x6f, 0x67, 0x69, 0x6e, 0x12, 0x0d, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x52, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x1a, 0x0e, 0x2e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x52, 0x65, 0x73, 0x70, 0x6f,
	0x6e, 0x73, 0x65, 0x12, 0x3f, 0x0a, 0x12, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65,
	0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x12, 0x21, 0x2e, 0x43, 0x65, 0x6e, 0x74,
	0x72, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65,
	0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45,
	0x6d, 0x70, 0x74, 0x79, 0x12, 0x38, 0x0a, 0x12, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64,
	0x65, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47, 0x61, 0x6d, 0x65, 0x12, 0x1a, 0x2e, 0x4c, 0x6f, 0x67,
	0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x4c, 0x65, 0x61, 0x76, 0x65, 0x47, 0x61, 0x6d, 0x65, 0x52,
	0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x43,
	0x0a, 0x1a, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x12, 0x1d, 0x2e, 0x47,
	0x61, 0x74, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x44, 0x69, 0x73, 0x63, 0x6f, 0x6e,
	0x6e, 0x65, 0x63, 0x74, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d,
	0x70, 0x74, 0x79, 0x12, 0x44, 0x0a, 0x0d, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x53, 0x65, 0x72,
	0x76, 0x69, 0x63, 0x65, 0x12, 0x18, 0x2e, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65,
	0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x19,
	0x2e, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67,
	0x65, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x36, 0x0a, 0x0e, 0x45, 0x6e, 0x74,
	0x65, 0x72, 0x47, 0x73, 0x53, 0x75, 0x63, 0x63, 0x65, 0x65, 0x64, 0x12, 0x1c, 0x2e, 0x45, 0x6e,
	0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x75, 0x63, 0x63, 0x65,
	0x73, 0x73, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e, 0x45, 0x6d, 0x70, 0x74,
	0x79, 0x12, 0x41, 0x0a, 0x12, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x74,
	0x72, 0x69, 0x6e, 0x67, 0x4d, 0x73, 0x67, 0x12, 0x14, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d,
	0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x15, 0x2e,
	0x52, 0x6f, 0x75, 0x74, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x73, 0x70,
	0x6f, 0x6e, 0x73, 0x65, 0x12, 0x4f, 0x0a, 0x14, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x50, 0x6c, 0x61,
	0x79, 0x65, 0x72, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x4d, 0x73, 0x67, 0x12, 0x1a, 0x2e, 0x52,
	0x6f, 0x75, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67,
	0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x1b, 0x2e, 0x52, 0x6f, 0x75, 0x74, 0x65,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x52, 0x65, 0x73,
	0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x2e, 0x0a, 0x0d, 0x49, 0x6e, 0x69, 0x74, 0x53, 0x63, 0x65,
	0x6e, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x12, 0x15, 0x2e, 0x49, 0x6e, 0x69, 0x74, 0x53, 0x63, 0x65,
	0x6e, 0x65, 0x4e, 0x6f, 0x64, 0x65, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x06, 0x2e,
	0x45, 0x6d, 0x70, 0x74, 0x79, 0x12, 0x50, 0x0a, 0x13, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65,
	0x72, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x12, 0x1b, 0x2e, 0x52,
	0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x1c, 0x2e, 0x52, 0x65, 0x67, 0x69,
	0x73, 0x74, 0x65, 0x72, 0x4e, 0x6f, 0x64, 0x65, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x52,
	0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x42, 0x0c, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61,
	0x6d, 0x65, 0x80, 0x01, 0x01, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_centre_centre_service_proto_rawDescOnce sync.Once
	file_proto_centre_centre_service_proto_rawDescData = file_proto_centre_centre_service_proto_rawDesc
)

func file_proto_centre_centre_service_proto_rawDescGZIP() []byte {
	file_proto_centre_centre_service_proto_rawDescOnce.Do(func() {
		file_proto_centre_centre_service_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_centre_centre_service_proto_rawDescData)
	})
	return file_proto_centre_centre_service_proto_rawDescData
}

var file_proto_centre_centre_service_proto_msgTypes = make([]protoimpl.MessageInfo, 5)
var file_proto_centre_centre_service_proto_goTypes = []any{
	(*GateClientMessageRequest)(nil),         // 0: GateClientMessageRequest
	(*LoginNodeLeaveGameRequest)(nil),        // 1: LoginNodeLeaveGameRequest
	(*EnterGameNodeSuccessRequest)(nil),      // 2: EnterGameNodeSuccessRequest
	(*CentrePlayerGameNodeEntryRequest)(nil), // 3: CentrePlayerGameNodeEntryRequest
	(*InitSceneNodeRequest)(nil),             // 4: InitSceneNodeRequest
	(*NetworkAddress)(nil),                   // 5: NetworkAddress
	(*EnterGameRequest)(nil),                 // 6: EnterGameRequest
	(*SessionDetails)(nil),                   // 7: SessionDetails
	(*GateSessionDisconnectRequest)(nil),     // 8: GateSessionDisconnectRequest
	(*LoginRequest)(nil),                     // 9: LoginRequest
	(*NodeRouteMessageRequest)(nil),          // 10: NodeRouteMessageRequest
	(*RouteMessageRequest)(nil),              // 11: RouteMessageRequest
	(*RoutePlayerMessageRequest)(nil),        // 12: RoutePlayerMessageRequest
	(*RegisterNodeSessionRequest)(nil),       // 13: RegisterNodeSessionRequest
	(*Empty)(nil),                            // 14: Empty
	(*LoginResponse)(nil),                    // 15: LoginResponse
	(*NodeRouteMessageResponse)(nil),         // 16: NodeRouteMessageResponse
	(*RouteMessageResponse)(nil),             // 17: RouteMessageResponse
	(*RoutePlayerMessageResponse)(nil),       // 18: RoutePlayerMessageResponse
	(*RegisterNodeSessionResponse)(nil),      // 19: RegisterNodeSessionResponse
}
var file_proto_centre_centre_service_proto_depIdxs = []int32{
	5,  // 0: GateClientMessageRequest.rpc_client:type_name -> NetworkAddress
	6,  // 1: CentrePlayerGameNodeEntryRequest.client_msg_body:type_name -> EnterGameRequest
	7,  // 2: CentrePlayerGameNodeEntryRequest.session_info:type_name -> SessionDetails
	0,  // 3: CentreService.GatePlayerService:input_type -> GateClientMessageRequest
	8,  // 4: CentreService.GateSessionDisconnect:input_type -> GateSessionDisconnectRequest
	9,  // 5: CentreService.LoginNodeAccountLogin:input_type -> LoginRequest
	3,  // 6: CentreService.LoginNodeEnterGame:input_type -> CentrePlayerGameNodeEntryRequest
	1,  // 7: CentreService.LoginNodeLeaveGame:input_type -> LoginNodeLeaveGameRequest
	8,  // 8: CentreService.LoginNodeSessionDisconnect:input_type -> GateSessionDisconnectRequest
	10, // 9: CentreService.PlayerService:input_type -> NodeRouteMessageRequest
	2,  // 10: CentreService.EnterGsSucceed:input_type -> EnterGameNodeSuccessRequest
	11, // 11: CentreService.RouteNodeStringMsg:input_type -> RouteMessageRequest
	12, // 12: CentreService.RoutePlayerStringMsg:input_type -> RoutePlayerMessageRequest
	4,  // 13: CentreService.InitSceneNode:input_type -> InitSceneNodeRequest
	13, // 14: CentreService.RegisterNodeSession:input_type -> RegisterNodeSessionRequest
	14, // 15: CentreService.GatePlayerService:output_type -> Empty
	14, // 16: CentreService.GateSessionDisconnect:output_type -> Empty
	15, // 17: CentreService.LoginNodeAccountLogin:output_type -> LoginResponse
	14, // 18: CentreService.LoginNodeEnterGame:output_type -> Empty
	14, // 19: CentreService.LoginNodeLeaveGame:output_type -> Empty
	14, // 20: CentreService.LoginNodeSessionDisconnect:output_type -> Empty
	16, // 21: CentreService.PlayerService:output_type -> NodeRouteMessageResponse
	14, // 22: CentreService.EnterGsSucceed:output_type -> Empty
	17, // 23: CentreService.RouteNodeStringMsg:output_type -> RouteMessageResponse
	18, // 24: CentreService.RoutePlayerStringMsg:output_type -> RoutePlayerMessageResponse
	14, // 25: CentreService.InitSceneNode:output_type -> Empty
	19, // 26: CentreService.RegisterNodeSession:output_type -> RegisterNodeSessionResponse
	15, // [15:27] is the sub-list for method output_type
	3,  // [3:15] is the sub-list for method input_type
	3,  // [3:3] is the sub-list for extension type_name
	3,  // [3:3] is the sub-list for extension extendee
	0,  // [0:3] is the sub-list for field type_name
}

func init() { file_proto_centre_centre_service_proto_init() }
func file_proto_centre_centre_service_proto_init() {
	if File_proto_centre_centre_service_proto != nil {
		return
	}
	file_proto_common_common_proto_init()
	file_proto_common_empty_proto_init()
	file_proto_common_session_proto_init()
	file_proto_common_c2gate_proto_init()
	file_proto_common_message_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_centre_centre_service_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*GateClientMessageRequest); i {
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
		file_proto_centre_centre_service_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*LoginNodeLeaveGameRequest); i {
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
		file_proto_centre_centre_service_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*EnterGameNodeSuccessRequest); i {
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
		file_proto_centre_centre_service_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*CentrePlayerGameNodeEntryRequest); i {
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
		file_proto_centre_centre_service_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*InitSceneNodeRequest); i {
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
			RawDescriptor: file_proto_centre_centre_service_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   5,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_proto_centre_centre_service_proto_goTypes,
		DependencyIndexes: file_proto_centre_centre_service_proto_depIdxs,
		MessageInfos:      file_proto_centre_centre_service_proto_msgTypes,
	}.Build()
	File_proto_centre_centre_service_proto = out.File
	file_proto_centre_centre_service_proto_rawDesc = nil
	file_proto_centre_centre_service_proto_goTypes = nil
	file_proto_centre_centre_service_proto_depIdxs = nil
}
