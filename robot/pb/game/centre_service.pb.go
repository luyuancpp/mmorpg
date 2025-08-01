// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/centre/centre_service.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
	unsafe "unsafe"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type GateClientMessageRequest struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	RpcClient     *NetworkAddress        `protobuf:"bytes,1,opt,name=rpc_client,json=rpcClient,proto3" json:"rpc_client,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *GateClientMessageRequest) Reset() {
	*x = GateClientMessageRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *GateClientMessageRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GateClientMessageRequest) ProtoMessage() {}

func (x *GateClientMessageRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[0]
	if x != nil {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	SessionInfo   *SessionDetails        `protobuf:"bytes,1,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *LoginNodeLeaveGameRequest) Reset() {
	*x = LoginNodeLeaveGameRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[1]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *LoginNodeLeaveGameRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*LoginNodeLeaveGameRequest) ProtoMessage() {}

func (x *LoginNodeLeaveGameRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[1]
	if x != nil {
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

func (x *LoginNodeLeaveGameRequest) GetSessionInfo() *SessionDetails {
	if x != nil {
		return x.SessionInfo
	}
	return nil
}

type EnterGameNodeSuccessRequest struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	PlayerId      uint64                 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	SceneNodeId   uint32                 `protobuf:"varint,2,opt,name=scene_node_id,json=sceneNodeId,proto3" json:"scene_node_id,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *EnterGameNodeSuccessRequest) Reset() {
	*x = EnterGameNodeSuccessRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[2]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *EnterGameNodeSuccessRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EnterGameNodeSuccessRequest) ProtoMessage() {}

func (x *EnterGameNodeSuccessRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[2]
	if x != nil {
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

type CentreEnterGameRequest struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	PlayerId      uint64                 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	LoginToken    string                 `protobuf:"bytes,2,opt,name=login_token,json=loginToken,proto3" json:"login_token,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *CentreEnterGameRequest) Reset() {
	*x = CentreEnterGameRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[3]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *CentreEnterGameRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CentreEnterGameRequest) ProtoMessage() {}

func (x *CentreEnterGameRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[3]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CentreEnterGameRequest.ProtoReflect.Descriptor instead.
func (*CentreEnterGameRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{3}
}

func (x *CentreEnterGameRequest) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

func (x *CentreEnterGameRequest) GetLoginToken() string {
	if x != nil {
		return x.LoginToken
	}
	return ""
}

type CentrePlayerGameNodeEntryRequest struct {
	state         protoimpl.MessageState  `protogen:"open.v1"`
	ClientMsgBody *CentreEnterGameRequest `protobuf:"bytes,1,opt,name=client_msg_body,json=clientMsgBody,proto3" json:"client_msg_body,omitempty"`
	SessionInfo   *SessionDetails         `protobuf:"bytes,2,opt,name=session_info,json=sessionInfo,proto3" json:"session_info,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *CentrePlayerGameNodeEntryRequest) Reset() {
	*x = CentrePlayerGameNodeEntryRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[4]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *CentrePlayerGameNodeEntryRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CentrePlayerGameNodeEntryRequest) ProtoMessage() {}

func (x *CentrePlayerGameNodeEntryRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[4]
	if x != nil {
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
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{4}
}

func (x *CentrePlayerGameNodeEntryRequest) GetClientMsgBody() *CentreEnterGameRequest {
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
	state         protoimpl.MessageState `protogen:"open.v1"`
	NodeId        uint32                 `protobuf:"varint,1,opt,name=node_id,json=nodeId,proto3" json:"node_id,omitempty"`
	SceneNodeType uint32                 `protobuf:"varint,2,opt,name=scene_node_type,json=sceneNodeType,proto3" json:"scene_node_type,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *InitSceneNodeRequest) Reset() {
	*x = InitSceneNodeRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[5]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *InitSceneNodeRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*InitSceneNodeRequest) ProtoMessage() {}

func (x *InitSceneNodeRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[5]
	if x != nil {
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
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{5}
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

type CentreLoginRequest struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	Account       string                 `protobuf:"bytes,1,opt,name=account,proto3" json:"account,omitempty"`
	Password      string                 `protobuf:"bytes,2,opt,name=password,proto3" json:"password,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *CentreLoginRequest) Reset() {
	*x = CentreLoginRequest{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[6]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *CentreLoginRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CentreLoginRequest) ProtoMessage() {}

func (x *CentreLoginRequest) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[6]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CentreLoginRequest.ProtoReflect.Descriptor instead.
func (*CentreLoginRequest) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{6}
}

func (x *CentreLoginRequest) GetAccount() string {
	if x != nil {
		return x.Account
	}
	return ""
}

func (x *CentreLoginRequest) GetPassword() string {
	if x != nil {
		return x.Password
	}
	return ""
}

type CentreLoginResponse struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	ErrorMessage  *TipInfoMessage        `protobuf:"bytes,1,opt,name=error_message,json=errorMessage,proto3" json:"error_message,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *CentreLoginResponse) Reset() {
	*x = CentreLoginResponse{}
	mi := &file_proto_centre_centre_service_proto_msgTypes[7]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *CentreLoginResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CentreLoginResponse) ProtoMessage() {}

func (x *CentreLoginResponse) ProtoReflect() protoreflect.Message {
	mi := &file_proto_centre_centre_service_proto_msgTypes[7]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CentreLoginResponse.ProtoReflect.Descriptor instead.
func (*CentreLoginResponse) Descriptor() ([]byte, []int) {
	return file_proto_centre_centre_service_proto_rawDescGZIP(), []int{7}
}

func (x *CentreLoginResponse) GetErrorMessage() *TipInfoMessage {
	if x != nil {
		return x.ErrorMessage
	}
	return nil
}

var File_proto_centre_centre_service_proto protoreflect.FileDescriptor

const file_proto_centre_centre_service_proto_rawDesc = "" +
	"\n" +
	"!proto/centre/centre_service.proto\x1a\x19proto/common/common.proto\x1a\x18proto/common/empty.proto\x1a\x1aproto/common/session.proto\x1a\x1aproto/common/message.proto\x1a\x16proto/common/tip.proto\"J\n" +
	"\x18GateClientMessageRequest\x12.\n" +
	"\n" +
	"rpc_client\x18\x01 \x01(\v2\x0f.NetworkAddressR\trpcClient\"O\n" +
	"\x19LoginNodeLeaveGameRequest\x122\n" +
	"\fsession_info\x18\x01 \x01(\v2\x0f.SessionDetailsR\vsessionInfo\"^\n" +
	"\x1bEnterGameNodeSuccessRequest\x12\x1b\n" +
	"\tplayer_id\x18\x01 \x01(\x04R\bplayerId\x12\"\n" +
	"\rscene_node_id\x18\x02 \x01(\rR\vsceneNodeId\"V\n" +
	"\x16CentreEnterGameRequest\x12\x1b\n" +
	"\tplayer_id\x18\x01 \x01(\x04R\bplayerId\x12\x1f\n" +
	"\vlogin_token\x18\x02 \x01(\tR\n" +
	"loginToken\"\x97\x01\n" +
	" CentrePlayerGameNodeEntryRequest\x12?\n" +
	"\x0fclient_msg_body\x18\x01 \x01(\v2\x17.CentreEnterGameRequestR\rclientMsgBody\x122\n" +
	"\fsession_info\x18\x02 \x01(\v2\x0f.SessionDetailsR\vsessionInfo\"W\n" +
	"\x14InitSceneNodeRequest\x12\x17\n" +
	"\anode_id\x18\x01 \x01(\rR\x06nodeId\x12&\n" +
	"\x0fscene_node_type\x18\x02 \x01(\rR\rsceneNodeType\"J\n" +
	"\x12CentreLoginRequest\x12\x18\n" +
	"\aaccount\x18\x01 \x01(\tR\aaccount\x12\x1a\n" +
	"\bpassword\x18\x02 \x01(\tR\bpassword\"K\n" +
	"\x13CentreLoginResponse\x124\n" +
	"\rerror_message\x18\x01 \x01(\v2\x0f.TipInfoMessageR\ferrorMessage2\x98\x06\n" +
	"\x06Centre\x126\n" +
	"\x11GatePlayerService\x12\x19.GateClientMessageRequest\x1a\x06.Empty\x12>\n" +
	"\x15GateSessionDisconnect\x12\x1d.GateSessionDisconnectRequest\x1a\x06.Empty\x12B\n" +
	"\x15LoginNodeAccountLogin\x12\x13.CentreLoginRequest\x1a\x14.CentreLoginResponse\x12?\n" +
	"\x12LoginNodeEnterGame\x12!.CentrePlayerGameNodeEntryRequest\x1a\x06.Empty\x128\n" +
	"\x12LoginNodeLeaveGame\x12\x1a.LoginNodeLeaveGameRequest\x1a\x06.Empty\x12C\n" +
	"\x1aLoginNodeSessionDisconnect\x12\x1d.GateSessionDisconnectRequest\x1a\x06.Empty\x12D\n" +
	"\rPlayerService\x12\x18.NodeRouteMessageRequest\x1a\x19.NodeRouteMessageResponse\x126\n" +
	"\x0eEnterGsSucceed\x12\x1c.EnterGameNodeSuccessRequest\x1a\x06.Empty\x12A\n" +
	"\x12RouteNodeStringMsg\x12\x14.RouteMessageRequest\x1a\x15.RouteMessageResponse\x12O\n" +
	"\x14RoutePlayerStringMsg\x12\x1a.RoutePlayerMessageRequest\x1a\x1b.RoutePlayerMessageResponse\x12.\n" +
	"\rInitSceneNode\x12\x15.InitSceneNodeRequest\x1a\x06.Empty\x12P\n" +
	"\x13RegisterNodeSession\x12\x1b.RegisterNodeSessionRequest\x1a\x1c.RegisterNodeSessionResponseB\fZ\apb/game\x80\x01\x01b\x06proto3"

var (
	file_proto_centre_centre_service_proto_rawDescOnce sync.Once
	file_proto_centre_centre_service_proto_rawDescData []byte
)

func file_proto_centre_centre_service_proto_rawDescGZIP() []byte {
	file_proto_centre_centre_service_proto_rawDescOnce.Do(func() {
		file_proto_centre_centre_service_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_centre_centre_service_proto_rawDesc), len(file_proto_centre_centre_service_proto_rawDesc)))
	})
	return file_proto_centre_centre_service_proto_rawDescData
}

var file_proto_centre_centre_service_proto_msgTypes = make([]protoimpl.MessageInfo, 8)
var file_proto_centre_centre_service_proto_goTypes = []any{
	(*GateClientMessageRequest)(nil),         // 0: GateClientMessageRequest
	(*LoginNodeLeaveGameRequest)(nil),        // 1: LoginNodeLeaveGameRequest
	(*EnterGameNodeSuccessRequest)(nil),      // 2: EnterGameNodeSuccessRequest
	(*CentreEnterGameRequest)(nil),           // 3: CentreEnterGameRequest
	(*CentrePlayerGameNodeEntryRequest)(nil), // 4: CentrePlayerGameNodeEntryRequest
	(*InitSceneNodeRequest)(nil),             // 5: InitSceneNodeRequest
	(*CentreLoginRequest)(nil),               // 6: CentreLoginRequest
	(*CentreLoginResponse)(nil),              // 7: CentreLoginResponse
	(*NetworkAddress)(nil),                   // 8: NetworkAddress
	(*SessionDetails)(nil),                   // 9: SessionDetails
	(*TipInfoMessage)(nil),                   // 10: TipInfoMessage
	(*GateSessionDisconnectRequest)(nil),     // 11: GateSessionDisconnectRequest
	(*NodeRouteMessageRequest)(nil),          // 12: NodeRouteMessageRequest
	(*RouteMessageRequest)(nil),              // 13: RouteMessageRequest
	(*RoutePlayerMessageRequest)(nil),        // 14: RoutePlayerMessageRequest
	(*RegisterNodeSessionRequest)(nil),       // 15: RegisterNodeSessionRequest
	(*Empty)(nil),                            // 16: Empty
	(*NodeRouteMessageResponse)(nil),         // 17: NodeRouteMessageResponse
	(*RouteMessageResponse)(nil),             // 18: RouteMessageResponse
	(*RoutePlayerMessageResponse)(nil),       // 19: RoutePlayerMessageResponse
	(*RegisterNodeSessionResponse)(nil),      // 20: RegisterNodeSessionResponse
}
var file_proto_centre_centre_service_proto_depIdxs = []int32{
	8,  // 0: GateClientMessageRequest.rpc_client:type_name -> NetworkAddress
	9,  // 1: LoginNodeLeaveGameRequest.session_info:type_name -> SessionDetails
	3,  // 2: CentrePlayerGameNodeEntryRequest.client_msg_body:type_name -> CentreEnterGameRequest
	9,  // 3: CentrePlayerGameNodeEntryRequest.session_info:type_name -> SessionDetails
	10, // 4: CentreLoginResponse.error_message:type_name -> TipInfoMessage
	0,  // 5: Centre.GatePlayerService:input_type -> GateClientMessageRequest
	11, // 6: Centre.GateSessionDisconnect:input_type -> GateSessionDisconnectRequest
	6,  // 7: Centre.LoginNodeAccountLogin:input_type -> CentreLoginRequest
	4,  // 8: Centre.LoginNodeEnterGame:input_type -> CentrePlayerGameNodeEntryRequest
	1,  // 9: Centre.LoginNodeLeaveGame:input_type -> LoginNodeLeaveGameRequest
	11, // 10: Centre.LoginNodeSessionDisconnect:input_type -> GateSessionDisconnectRequest
	12, // 11: Centre.PlayerService:input_type -> NodeRouteMessageRequest
	2,  // 12: Centre.EnterGsSucceed:input_type -> EnterGameNodeSuccessRequest
	13, // 13: Centre.RouteNodeStringMsg:input_type -> RouteMessageRequest
	14, // 14: Centre.RoutePlayerStringMsg:input_type -> RoutePlayerMessageRequest
	5,  // 15: Centre.InitSceneNode:input_type -> InitSceneNodeRequest
	15, // 16: Centre.RegisterNodeSession:input_type -> RegisterNodeSessionRequest
	16, // 17: Centre.GatePlayerService:output_type -> Empty
	16, // 18: Centre.GateSessionDisconnect:output_type -> Empty
	7,  // 19: Centre.LoginNodeAccountLogin:output_type -> CentreLoginResponse
	16, // 20: Centre.LoginNodeEnterGame:output_type -> Empty
	16, // 21: Centre.LoginNodeLeaveGame:output_type -> Empty
	16, // 22: Centre.LoginNodeSessionDisconnect:output_type -> Empty
	17, // 23: Centre.PlayerService:output_type -> NodeRouteMessageResponse
	16, // 24: Centre.EnterGsSucceed:output_type -> Empty
	18, // 25: Centre.RouteNodeStringMsg:output_type -> RouteMessageResponse
	19, // 26: Centre.RoutePlayerStringMsg:output_type -> RoutePlayerMessageResponse
	16, // 27: Centre.InitSceneNode:output_type -> Empty
	20, // 28: Centre.RegisterNodeSession:output_type -> RegisterNodeSessionResponse
	17, // [17:29] is the sub-list for method output_type
	5,  // [5:17] is the sub-list for method input_type
	5,  // [5:5] is the sub-list for extension type_name
	5,  // [5:5] is the sub-list for extension extendee
	0,  // [0:5] is the sub-list for field type_name
}

func init() { file_proto_centre_centre_service_proto_init() }
func file_proto_centre_centre_service_proto_init() {
	if File_proto_centre_centre_service_proto != nil {
		return
	}
	file_proto_common_common_proto_init()
	file_proto_common_empty_proto_init()
	file_proto_common_session_proto_init()
	file_proto_common_message_proto_init()
	file_proto_common_tip_proto_init()
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_centre_centre_service_proto_rawDesc), len(file_proto_centre_centre_service_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   8,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_proto_centre_centre_service_proto_goTypes,
		DependencyIndexes: file_proto_centre_centre_service_proto_depIdxs,
		MessageInfos:      file_proto_centre_centre_service_proto_msgTypes,
	}.Build()
	File_proto_centre_centre_service_proto = out.File
	file_proto_centre_centre_service_proto_goTypes = nil
	file_proto_centre_centre_service_proto_depIdxs = nil
}
