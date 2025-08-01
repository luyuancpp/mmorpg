// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.36.6
// 	protoc        v5.29.0
// source: proto/common/common.proto

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

type EndpointPBComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	Ip            string                 `protobuf:"bytes,1,opt,name=ip,proto3" json:"ip,omitempty"`
	Port          uint32                 `protobuf:"varint,2,opt,name=port,proto3" json:"port,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *EndpointPBComponent) Reset() {
	*x = EndpointPBComponent{}
	mi := &file_proto_common_common_proto_msgTypes[0]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *EndpointPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*EndpointPBComponent) ProtoMessage() {}

func (x *EndpointPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_common_proto_msgTypes[0]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use EndpointPBComponent.ProtoReflect.Descriptor instead.
func (*EndpointPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_common_common_proto_rawDescGZIP(), []int{0}
}

func (x *EndpointPBComponent) GetIp() string {
	if x != nil {
		return x.Ip
	}
	return ""
}

func (x *EndpointPBComponent) GetPort() uint32 {
	if x != nil {
		return x.Port
	}
	return 0
}

type NodeInfo struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	NodeId        uint32                 `protobuf:"varint,1,opt,name=node_id,json=nodeId,proto3" json:"node_id,omitempty"`
	NodeType      uint32                 `protobuf:"varint,2,opt,name=node_type,json=nodeType,proto3" json:"node_type,omitempty"`
	LaunchTime    uint64                 `protobuf:"varint,3,opt,name=launch_time,json=launchTime,proto3" json:"launch_time,omitempty"`
	SceneNodeType uint32                 `protobuf:"varint,4,opt,name=scene_node_type,json=sceneNodeType,proto3" json:"scene_node_type,omitempty"`
	Endpoint      *EndpointPBComponent   `protobuf:"bytes,5,opt,name=endpoint,proto3" json:"endpoint,omitempty"`
	ZoneId        uint32                 `protobuf:"varint,6,opt,name=zone_id,json=zoneId,proto3" json:"zone_id,omitempty"`
	ProtocolType  uint32                 `protobuf:"varint,7,opt,name=protocol_type,json=protocolType,proto3" json:"protocol_type,omitempty"` // 节点通信协议类型
	NodeUuid      string                 `protobuf:"bytes,8,opt,name=node_uuid,json=nodeUuid,proto3" json:"node_uuid,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *NodeInfo) Reset() {
	*x = NodeInfo{}
	mi := &file_proto_common_common_proto_msgTypes[1]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *NodeInfo) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*NodeInfo) ProtoMessage() {}

func (x *NodeInfo) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_common_proto_msgTypes[1]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use NodeInfo.ProtoReflect.Descriptor instead.
func (*NodeInfo) Descriptor() ([]byte, []int) {
	return file_proto_common_common_proto_rawDescGZIP(), []int{1}
}

func (x *NodeInfo) GetNodeId() uint32 {
	if x != nil {
		return x.NodeId
	}
	return 0
}

func (x *NodeInfo) GetNodeType() uint32 {
	if x != nil {
		return x.NodeType
	}
	return 0
}

func (x *NodeInfo) GetLaunchTime() uint64 {
	if x != nil {
		return x.LaunchTime
	}
	return 0
}

func (x *NodeInfo) GetSceneNodeType() uint32 {
	if x != nil {
		return x.SceneNodeType
	}
	return 0
}

func (x *NodeInfo) GetEndpoint() *EndpointPBComponent {
	if x != nil {
		return x.Endpoint
	}
	return nil
}

func (x *NodeInfo) GetZoneId() uint32 {
	if x != nil {
		return x.ZoneId
	}
	return 0
}

func (x *NodeInfo) GetProtocolType() uint32 {
	if x != nil {
		return x.ProtocolType
	}
	return 0
}

func (x *NodeInfo) GetNodeUuid() string {
	if x != nil {
		return x.NodeUuid
	}
	return ""
}

type NodeInfoListPBComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	NodeList      []*NodeInfo            `protobuf:"bytes,1,rep,name=node_list,json=nodeList,proto3" json:"node_list,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *NodeInfoListPBComponent) Reset() {
	*x = NodeInfoListPBComponent{}
	mi := &file_proto_common_common_proto_msgTypes[2]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *NodeInfoListPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*NodeInfoListPBComponent) ProtoMessage() {}

func (x *NodeInfoListPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_common_proto_msgTypes[2]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use NodeInfoListPBComponent.ProtoReflect.Descriptor instead.
func (*NodeInfoListPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_common_common_proto_rawDescGZIP(), []int{2}
}

func (x *NodeInfoListPBComponent) GetNodeList() []*NodeInfo {
	if x != nil {
		return x.NodeList
	}
	return nil
}

type NetworkAddress struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	Ip            string                 `protobuf:"bytes,1,opt,name=ip,proto3" json:"ip,omitempty"`
	Port          uint32                 `protobuf:"varint,2,opt,name=port,proto3" json:"port,omitempty"`
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *NetworkAddress) Reset() {
	*x = NetworkAddress{}
	mi := &file_proto_common_common_proto_msgTypes[3]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *NetworkAddress) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*NetworkAddress) ProtoMessage() {}

func (x *NetworkAddress) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_common_proto_msgTypes[3]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use NetworkAddress.ProtoReflect.Descriptor instead.
func (*NetworkAddress) Descriptor() ([]byte, []int) {
	return file_proto_common_common_proto_rawDescGZIP(), []int{3}
}

func (x *NetworkAddress) GetIp() string {
	if x != nil {
		return x.Ip
	}
	return ""
}

func (x *NetworkAddress) GetPort() uint32 {
	if x != nil {
		return x.Port
	}
	return 0
}

type SceneInfoPBComponent struct {
	state         protoimpl.MessageState `protogen:"open.v1"`
	SceneConfid   uint32                 `protobuf:"varint,1,opt,name=scene_confid,json=sceneConfid,proto3" json:"scene_confid,omitempty"`                                                   //场景id
	Guid          uint32                 `protobuf:"varint,2,opt,name=guid,proto3" json:"guid,omitempty"`                                                                                    //场景唯一id
	MirrorConfid  uint32                 `protobuf:"varint,3,opt,name=mirror_confid,json=mirrorConfid,proto3" json:"mirror_confid,omitempty"`                                                //镜像id
	DungenConfid  uint32                 `protobuf:"varint,4,opt,name=dungen_confid,json=dungenConfid,proto3" json:"dungen_confid,omitempty"`                                                //副本id
	Creators      map[uint64]bool        `protobuf:"bytes,5,rep,name=creators,proto3" json:"creators,omitempty" protobuf_key:"varint,1,opt,name=key" protobuf_val:"varint,2,opt,name=value"` //创建者
	unknownFields protoimpl.UnknownFields
	sizeCache     protoimpl.SizeCache
}

func (x *SceneInfoPBComponent) Reset() {
	*x = SceneInfoPBComponent{}
	mi := &file_proto_common_common_proto_msgTypes[4]
	ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
	ms.StoreMessageInfo(mi)
}

func (x *SceneInfoPBComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SceneInfoPBComponent) ProtoMessage() {}

func (x *SceneInfoPBComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_common_common_proto_msgTypes[4]
	if x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SceneInfoPBComponent.ProtoReflect.Descriptor instead.
func (*SceneInfoPBComponent) Descriptor() ([]byte, []int) {
	return file_proto_common_common_proto_rawDescGZIP(), []int{4}
}

func (x *SceneInfoPBComponent) GetSceneConfid() uint32 {
	if x != nil {
		return x.SceneConfid
	}
	return 0
}

func (x *SceneInfoPBComponent) GetGuid() uint32 {
	if x != nil {
		return x.Guid
	}
	return 0
}

func (x *SceneInfoPBComponent) GetMirrorConfid() uint32 {
	if x != nil {
		return x.MirrorConfid
	}
	return 0
}

func (x *SceneInfoPBComponent) GetDungenConfid() uint32 {
	if x != nil {
		return x.DungenConfid
	}
	return 0
}

func (x *SceneInfoPBComponent) GetCreators() map[uint64]bool {
	if x != nil {
		return x.Creators
	}
	return nil
}

var File_proto_common_common_proto protoreflect.FileDescriptor

const file_proto_common_common_proto_rawDesc = "" +
	"\n" +
	"\x19proto/common/common.proto\"9\n" +
	"\x13EndpointPBComponent\x12\x0e\n" +
	"\x02ip\x18\x01 \x01(\tR\x02ip\x12\x12\n" +
	"\x04port\x18\x02 \x01(\rR\x04port\"\x96\x02\n" +
	"\bNodeInfo\x12\x17\n" +
	"\anode_id\x18\x01 \x01(\rR\x06nodeId\x12\x1b\n" +
	"\tnode_type\x18\x02 \x01(\rR\bnodeType\x12\x1f\n" +
	"\vlaunch_time\x18\x03 \x01(\x04R\n" +
	"launchTime\x12&\n" +
	"\x0fscene_node_type\x18\x04 \x01(\rR\rsceneNodeType\x120\n" +
	"\bendpoint\x18\x05 \x01(\v2\x14.EndpointPBComponentR\bendpoint\x12\x17\n" +
	"\azone_id\x18\x06 \x01(\rR\x06zoneId\x12#\n" +
	"\rprotocol_type\x18\a \x01(\rR\fprotocolType\x12\x1b\n" +
	"\tnode_uuid\x18\b \x01(\tR\bnodeUuid\"A\n" +
	"\x17NodeInfoListPBComponent\x12&\n" +
	"\tnode_list\x18\x01 \x03(\v2\t.NodeInfoR\bnodeList\"4\n" +
	"\x0eNetworkAddress\x12\x0e\n" +
	"\x02ip\x18\x01 \x01(\tR\x02ip\x12\x12\n" +
	"\x04port\x18\x02 \x01(\rR\x04port\"\x95\x02\n" +
	"\x14SceneInfoPBComponent\x12!\n" +
	"\fscene_confid\x18\x01 \x01(\rR\vsceneConfid\x12\x12\n" +
	"\x04guid\x18\x02 \x01(\rR\x04guid\x12#\n" +
	"\rmirror_confid\x18\x03 \x01(\rR\fmirrorConfid\x12#\n" +
	"\rdungen_confid\x18\x04 \x01(\rR\fdungenConfid\x12?\n" +
	"\bcreators\x18\x05 \x03(\v2#.SceneInfoPBComponent.CreatorsEntryR\bcreators\x1a;\n" +
	"\rCreatorsEntry\x12\x10\n" +
	"\x03key\x18\x01 \x01(\x04R\x03key\x12\x14\n" +
	"\x05value\x18\x02 \x01(\bR\x05value:\x028\x01B\tZ\apb/gameb\x06proto3"

var (
	file_proto_common_common_proto_rawDescOnce sync.Once
	file_proto_common_common_proto_rawDescData []byte
)

func file_proto_common_common_proto_rawDescGZIP() []byte {
	file_proto_common_common_proto_rawDescOnce.Do(func() {
		file_proto_common_common_proto_rawDescData = protoimpl.X.CompressGZIP(unsafe.Slice(unsafe.StringData(file_proto_common_common_proto_rawDesc), len(file_proto_common_common_proto_rawDesc)))
	})
	return file_proto_common_common_proto_rawDescData
}

var file_proto_common_common_proto_msgTypes = make([]protoimpl.MessageInfo, 6)
var file_proto_common_common_proto_goTypes = []any{
	(*EndpointPBComponent)(nil),     // 0: EndpointPBComponent
	(*NodeInfo)(nil),                // 1: NodeInfo
	(*NodeInfoListPBComponent)(nil), // 2: NodeInfoListPBComponent
	(*NetworkAddress)(nil),          // 3: NetworkAddress
	(*SceneInfoPBComponent)(nil),    // 4: SceneInfoPBComponent
	nil,                             // 5: SceneInfoPBComponent.CreatorsEntry
}
var file_proto_common_common_proto_depIdxs = []int32{
	0, // 0: NodeInfo.endpoint:type_name -> EndpointPBComponent
	1, // 1: NodeInfoListPBComponent.node_list:type_name -> NodeInfo
	5, // 2: SceneInfoPBComponent.creators:type_name -> SceneInfoPBComponent.CreatorsEntry
	3, // [3:3] is the sub-list for method output_type
	3, // [3:3] is the sub-list for method input_type
	3, // [3:3] is the sub-list for extension type_name
	3, // [3:3] is the sub-list for extension extendee
	0, // [0:3] is the sub-list for field type_name
}

func init() { file_proto_common_common_proto_init() }
func file_proto_common_common_proto_init() {
	if File_proto_common_common_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: unsafe.Slice(unsafe.StringData(file_proto_common_common_proto_rawDesc), len(file_proto_common_common_proto_rawDesc)),
			NumEnums:      0,
			NumMessages:   6,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_common_common_proto_goTypes,
		DependencyIndexes: file_proto_common_common_proto_depIdxs,
		MessageInfos:      file_proto_common_common_proto_msgTypes,
	}.Build()
	File_proto_common_common_proto = out.File
	file_proto_common_common_proto_goTypes = nil
	file_proto_common_common_proto_depIdxs = nil
}
