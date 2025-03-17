// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v6.31.0--dev
// source: game_rpc.proto

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

type GameMessageType int32

const (
	GameMessageType_REQUEST            GameMessageType = 0
	GameMessageType_RESPONSE           GameMessageType = 1
	GameMessageType_RPC_CLIENT_REQUEST GameMessageType = 2 //服务器到客户端
	GameMessageType_NODE_ROUTE         GameMessageType = 3 //服务器间传递
	GameMessageType_RPC_ERROR          GameMessageType = 4 // not used
)

// Enum value maps for GameMessageType.
var (
	GameMessageType_name = map[int32]string{
		0: "REQUEST",
		1: "RESPONSE",
		2: "RPC_CLIENT_REQUEST",
		3: "NODE_ROUTE",
		4: "RPC_ERROR",
	}
	GameMessageType_value = map[string]int32{
		"REQUEST":            0,
		"RESPONSE":           1,
		"RPC_CLIENT_REQUEST": 2,
		"NODE_ROUTE":         3,
		"RPC_ERROR":          4,
	}
)

func (x GameMessageType) Enum() *GameMessageType {
	p := new(GameMessageType)
	*p = x
	return p
}

func (x GameMessageType) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (GameMessageType) Descriptor() protoreflect.EnumDescriptor {
	return file_game_rpc_proto_enumTypes[0].Descriptor()
}

func (GameMessageType) Type() protoreflect.EnumType {
	return &file_game_rpc_proto_enumTypes[0]
}

func (x GameMessageType) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use GameMessageType.Descriptor instead.
func (GameMessageType) EnumDescriptor() ([]byte, []int) {
	return file_game_rpc_proto_rawDescGZIP(), []int{0}
}

type GameErrorCode int32

const (
	GameErrorCode_RPC_NO_ERROR     GameErrorCode = 0
	GameErrorCode_WRONG_PROTO      GameErrorCode = 1
	GameErrorCode_NO_SERVICE       GameErrorCode = 2
	GameErrorCode_NO_METHOD        GameErrorCode = 3
	GameErrorCode_INVALID_REQUEST  GameErrorCode = 4
	GameErrorCode_INVALID_RESPONSE GameErrorCode = 5
	GameErrorCode_TIMEOUT          GameErrorCode = 6
)

// Enum value maps for GameErrorCode.
var (
	GameErrorCode_name = map[int32]string{
		0: "RPC_NO_ERROR",
		1: "WRONG_PROTO",
		2: "NO_SERVICE",
		3: "NO_METHOD",
		4: "INVALID_REQUEST",
		5: "INVALID_RESPONSE",
		6: "TIMEOUT",
	}
	GameErrorCode_value = map[string]int32{
		"RPC_NO_ERROR":     0,
		"WRONG_PROTO":      1,
		"NO_SERVICE":       2,
		"NO_METHOD":        3,
		"INVALID_REQUEST":  4,
		"INVALID_RESPONSE": 5,
		"TIMEOUT":          6,
	}
)

func (x GameErrorCode) Enum() *GameErrorCode {
	p := new(GameErrorCode)
	*p = x
	return p
}

func (x GameErrorCode) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (GameErrorCode) Descriptor() protoreflect.EnumDescriptor {
	return file_game_rpc_proto_enumTypes[1].Descriptor()
}

func (GameErrorCode) Type() protoreflect.EnumType {
	return &file_game_rpc_proto_enumTypes[1]
}

func (x GameErrorCode) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use GameErrorCode.Descriptor instead.
func (GameErrorCode) EnumDescriptor() ([]byte, []int) {
	return file_game_rpc_proto_rawDescGZIP(), []int{1}
}

type GameRpcMessage struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Type      GameMessageType `protobuf:"varint,1,opt,name=type,proto3,enum=GameMessageType" json:"type,omitempty"`
	Request   []byte          `protobuf:"bytes,2,opt,name=request,proto3" json:"request,omitempty"`
	Response  []byte          `protobuf:"bytes,3,opt,name=response,proto3" json:"response,omitempty"`
	Error     GameErrorCode   `protobuf:"varint,4,opt,name=error,proto3,enum=GameErrorCode" json:"error,omitempty"`
	MessageId uint32          `protobuf:"varint,5,opt,name=message_id,json=messageId,proto3" json:"message_id,omitempty"`
}

func (x *GameRpcMessage) Reset() {
	*x = GameRpcMessage{}
	if protoimpl.UnsafeEnabled {
		mi := &file_game_rpc_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GameRpcMessage) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GameRpcMessage) ProtoMessage() {}

func (x *GameRpcMessage) ProtoReflect() protoreflect.Message {
	mi := &file_game_rpc_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use GameRpcMessage.ProtoReflect.Descriptor instead.
func (*GameRpcMessage) Descriptor() ([]byte, []int) {
	return file_game_rpc_proto_rawDescGZIP(), []int{0}
}

func (x *GameRpcMessage) GetType() GameMessageType {
	if x != nil {
		return x.Type
	}
	return GameMessageType_REQUEST
}

func (x *GameRpcMessage) GetRequest() []byte {
	if x != nil {
		return x.Request
	}
	return nil
}

func (x *GameRpcMessage) GetResponse() []byte {
	if x != nil {
		return x.Response
	}
	return nil
}

func (x *GameRpcMessage) GetError() GameErrorCode {
	if x != nil {
		return x.Error
	}
	return GameErrorCode_RPC_NO_ERROR
}

func (x *GameRpcMessage) GetMessageId() uint32 {
	if x != nil {
		return x.MessageId
	}
	return 0
}

var File_game_rpc_proto protoreflect.FileDescriptor

var file_game_rpc_proto_rawDesc = []byte{
	0x0a, 0x0e, 0x67, 0x61, 0x6d, 0x65, 0x5f, 0x72, 0x70, 0x63, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x22, 0xb1, 0x01, 0x0a, 0x0e, 0x47, 0x61, 0x6d, 0x65, 0x52, 0x70, 0x63, 0x4d, 0x65, 0x73, 0x73,
	0x61, 0x67, 0x65, 0x12, 0x24, 0x0a, 0x04, 0x74, 0x79, 0x70, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x0e, 0x32, 0x10, 0x2e, 0x47, 0x61, 0x6d, 0x65, 0x4d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x54,
	0x79, 0x70, 0x65, 0x52, 0x04, 0x74, 0x79, 0x70, 0x65, 0x12, 0x18, 0x0a, 0x07, 0x72, 0x65, 0x71,
	0x75, 0x65, 0x73, 0x74, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x07, 0x72, 0x65, 0x71, 0x75,
	0x65, 0x73, 0x74, 0x12, 0x1a, 0x0a, 0x08, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x18,
	0x03, 0x20, 0x01, 0x28, 0x0c, 0x52, 0x08, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12,
	0x24, 0x0a, 0x05, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x18, 0x04, 0x20, 0x01, 0x28, 0x0e, 0x32, 0x0e,
	0x2e, 0x47, 0x61, 0x6d, 0x65, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x43, 0x6f, 0x64, 0x65, 0x52, 0x05,
	0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x1d, 0x0a, 0x0a, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
	0x5f, 0x69, 0x64, 0x18, 0x05, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x6d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x49, 0x64, 0x2a, 0x63, 0x0a, 0x0f, 0x47, 0x61, 0x6d, 0x65, 0x4d, 0x65, 0x73, 0x73,
	0x61, 0x67, 0x65, 0x54, 0x79, 0x70, 0x65, 0x12, 0x0b, 0x0a, 0x07, 0x52, 0x45, 0x51, 0x55, 0x45,
	0x53, 0x54, 0x10, 0x00, 0x12, 0x0c, 0x0a, 0x08, 0x52, 0x45, 0x53, 0x50, 0x4f, 0x4e, 0x53, 0x45,
	0x10, 0x01, 0x12, 0x16, 0x0a, 0x12, 0x52, 0x50, 0x43, 0x5f, 0x43, 0x4c, 0x49, 0x45, 0x4e, 0x54,
	0x5f, 0x52, 0x45, 0x51, 0x55, 0x45, 0x53, 0x54, 0x10, 0x02, 0x12, 0x0e, 0x0a, 0x0a, 0x4e, 0x4f,
	0x44, 0x45, 0x5f, 0x52, 0x4f, 0x55, 0x54, 0x45, 0x10, 0x03, 0x12, 0x0d, 0x0a, 0x09, 0x52, 0x50,
	0x43, 0x5f, 0x45, 0x52, 0x52, 0x4f, 0x52, 0x10, 0x04, 0x2a, 0x89, 0x01, 0x0a, 0x0d, 0x47, 0x61,
	0x6d, 0x65, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x43, 0x6f, 0x64, 0x65, 0x12, 0x10, 0x0a, 0x0c, 0x52,
	0x50, 0x43, 0x5f, 0x4e, 0x4f, 0x5f, 0x45, 0x52, 0x52, 0x4f, 0x52, 0x10, 0x00, 0x12, 0x0f, 0x0a,
	0x0b, 0x57, 0x52, 0x4f, 0x4e, 0x47, 0x5f, 0x50, 0x52, 0x4f, 0x54, 0x4f, 0x10, 0x01, 0x12, 0x0e,
	0x0a, 0x0a, 0x4e, 0x4f, 0x5f, 0x53, 0x45, 0x52, 0x56, 0x49, 0x43, 0x45, 0x10, 0x02, 0x12, 0x0d,
	0x0a, 0x09, 0x4e, 0x4f, 0x5f, 0x4d, 0x45, 0x54, 0x48, 0x4f, 0x44, 0x10, 0x03, 0x12, 0x13, 0x0a,
	0x0f, 0x49, 0x4e, 0x56, 0x41, 0x4c, 0x49, 0x44, 0x5f, 0x52, 0x45, 0x51, 0x55, 0x45, 0x53, 0x54,
	0x10, 0x04, 0x12, 0x14, 0x0a, 0x10, 0x49, 0x4e, 0x56, 0x41, 0x4c, 0x49, 0x44, 0x5f, 0x52, 0x45,
	0x53, 0x50, 0x4f, 0x4e, 0x53, 0x45, 0x10, 0x05, 0x12, 0x0b, 0x0a, 0x07, 0x54, 0x49, 0x4d, 0x45,
	0x4f, 0x55, 0x54, 0x10, 0x06, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65,
	0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_game_rpc_proto_rawDescOnce sync.Once
	file_game_rpc_proto_rawDescData = file_game_rpc_proto_rawDesc
)

func file_game_rpc_proto_rawDescGZIP() []byte {
	file_game_rpc_proto_rawDescOnce.Do(func() {
		file_game_rpc_proto_rawDescData = protoimpl.X.CompressGZIP(file_game_rpc_proto_rawDescData)
	})
	return file_game_rpc_proto_rawDescData
}

var file_game_rpc_proto_enumTypes = make([]protoimpl.EnumInfo, 2)
var file_game_rpc_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_game_rpc_proto_goTypes = []any{
	(GameMessageType)(0),   // 0: GameMessageType
	(GameErrorCode)(0),     // 1: GameErrorCode
	(*GameRpcMessage)(nil), // 2: GameRpcMessage
}
var file_game_rpc_proto_depIdxs = []int32{
	0, // 0: GameRpcMessage.type:type_name -> GameMessageType
	1, // 1: GameRpcMessage.error:type_name -> GameErrorCode
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_game_rpc_proto_init() }
func file_game_rpc_proto_init() {
	if File_game_rpc_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_game_rpc_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*GameRpcMessage); i {
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
			RawDescriptor: file_game_rpc_proto_rawDesc,
			NumEnums:      2,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_game_rpc_proto_goTypes,
		DependencyIndexes: file_game_rpc_proto_depIdxs,
		EnumInfos:         file_game_rpc_proto_enumTypes,
		MessageInfos:      file_game_rpc_proto_msgTypes,
	}.Build()
	File_game_rpc_proto = out.File
	file_game_rpc_proto_rawDesc = nil
	file_game_rpc_proto_goTypes = nil
	file_game_rpc_proto_depIdxs = nil
}
