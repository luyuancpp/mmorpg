// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/common/session.proto

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

type SessionDetails struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	SessionId     uint64                 `protobuf:"varint,1,opt,name=session_id,json=sessionId,proto3" json:"session_id,omitempty"`
	PlayerId      uint64                 `protobuf:"varint,2,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *SessionDetails) Reset() {
	*x = SessionDetails{}
	mi := &file_proto_common_session_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *SessionDetails) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SessionDetails) ProtoMessage() {}

func (x *SessionDetails) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_session_proto_msgTypes[0]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SessionDetails.ProtoReflect.Descriptor instead.
func (*SessionDetails) Descriptor() ([]byte, []int) {
	return file_proto_common_session_proto_rawDescGZIP(), []int{0}
}

func (x *SessionDetails) GetSessionId() uint64 {
	if x != nil {
		return x.SessionId
	}
	return 0
}

func (x *SessionDetails) GetPlayerId() uint64 {
	if x != nil {
		return x.PlayerId
	}
	return 0
}

var File_proto_common_session_proto protoreflect.FileDescriptor

const file_proto_common_session_proto_rawDesc = "" +
	"\n" +
	"\x1aproto/common/session.proto\"L\n" +
	"\x0eSessionDetails\x12\x1d\n" +
	"\n" +
	"session_id\x18\x01 \x01(\x04R\tsessionId\x12\x1b\n" +
	"\tplayer_id\x18\x02 \x01(\x04R\bplayerIdB\tZ\apb/gameb\x06proto3"

var (
	file_proto_common_session_proto_rawDescOnce sync.Once
	file_proto_common_session_proto_rawDescData []byte
)

func file_proto_common_session_proto_rawDescGZIP() []byte {
	file_proto_common_session_proto_rawDescOnce.Do(func() {
		file_proto_common_session_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_common_session_proto_rawDesc), len(file_proto_common_session_proto_rawDesc)))
	})
	return file_proto_common_session_proto_rawDescData
}

var file_proto_common_session_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_proto_common_session_proto_goTypes = []any{
	(*SessionDetails)(nil), // 0: SessionDetails
}
var file_proto_common_session_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_proto_common_session_proto_init() }
func file_proto_common_session_proto_init() {
	if File_proto_common_session_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_common_session_proto_rawDesc), len(file_proto_common_session_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_common_session_proto_goTypes,
		DependencyIndexes: file_proto_common_session_proto_depIdxs,
		MessageInfos:      file_proto_common_session_proto_msgTypes,
	}.Build()
	File_proto_common_session_proto = out.File
	file_proto_common_session_proto_goTypes = nil
	file_proto_common_session_proto_depIdxs = nil
}
