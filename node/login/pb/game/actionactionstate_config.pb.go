// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: actionactionstate_config.proto

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

type State struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	StateMode uint32 `protobuf:"varint,1,opt,name=state_mode,json=stateMode,proto3" json:"state_mode,omitempty"`
	StateTip  uint32 `protobuf:"varint,2,opt,name=state_tip,json=stateTip,proto3" json:"state_tip,omitempty"`
}

func (x *State) Reset() {
	*x = State{}
	if protoimpl.UnsafeEnabled {
		mi := &file_actionactionstate_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *State) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*State) ProtoMessage() {}

func (x *State) ProtoReflect() protoreflect.Message {
	mi := &file_actionactionstate_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use State.ProtoReflect.Descriptor instead.
func (*State) Descriptor() ([]byte, []int) {
	return file_actionactionstate_config_proto_rawDescGZIP(), []int{0}
}

func (x *State) GetStateMode() uint32 {
	if x != nil {
		return x.StateMode
	}
	return 0
}

func (x *State) GetStateTip() uint32 {
	if x != nil {
		return x.StateTip
	}
	return 0
}

type ActionActionStateTable struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id    uint32   `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	State []*State `protobuf:"bytes,2,rep,name=state,proto3" json:"state,omitempty"`
}

func (x *ActionActionStateTable) Reset() {
	*x = ActionActionStateTable{}
	if protoimpl.UnsafeEnabled {
		mi := &file_actionactionstate_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ActionActionStateTable) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ActionActionStateTable) ProtoMessage() {}

func (x *ActionActionStateTable) ProtoReflect() protoreflect.Message {
	mi := &file_actionactionstate_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ActionActionStateTable.ProtoReflect.Descriptor instead.
func (*ActionActionStateTable) Descriptor() ([]byte, []int) {
	return file_actionactionstate_config_proto_rawDescGZIP(), []int{1}
}

func (x *ActionActionStateTable) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *ActionActionStateTable) GetState() []*State {
	if x != nil {
		return x.State
	}
	return nil
}

type ActionActionStateTabledData struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*ActionActionStateTable `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *ActionActionStateTabledData) Reset() {
	*x = ActionActionStateTabledData{}
	if protoimpl.UnsafeEnabled {
		mi := &file_actionactionstate_config_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *ActionActionStateTabledData) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*ActionActionStateTabledData) ProtoMessage() {}

func (x *ActionActionStateTabledData) ProtoReflect() protoreflect.Message {
	mi := &file_actionactionstate_config_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use ActionActionStateTabledData.ProtoReflect.Descriptor instead.
func (*ActionActionStateTabledData) Descriptor() ([]byte, []int) {
	return file_actionactionstate_config_proto_rawDescGZIP(), []int{2}
}

func (x *ActionActionStateTabledData) GetData() []*ActionActionStateTable {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_actionactionstate_config_proto protoreflect.FileDescriptor

var file_actionactionstate_config_proto_rawDesc = []byte{
	0x0a, 0x1e, 0x61, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x61, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x74,
	0x61, 0x74, 0x65, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f,
	0x22, 0x43, 0x0a, 0x05, 0x73, 0x74, 0x61, 0x74, 0x65, 0x12, 0x1d, 0x0a, 0x0a, 0x73, 0x74, 0x61,
	0x74, 0x65, 0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x73,
	0x74, 0x61, 0x74, 0x65, 0x4d, 0x6f, 0x64, 0x65, 0x12, 0x1b, 0x0a, 0x09, 0x73, 0x74, 0x61, 0x74,
	0x65, 0x5f, 0x74, 0x69, 0x70, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x08, 0x73, 0x74, 0x61,
	0x74, 0x65, 0x54, 0x69, 0x70, 0x22, 0x46, 0x0a, 0x16, 0x41, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x41,
	0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x12,
	0x0e, 0x0a, 0x02, 0x69, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12,
	0x1c, 0x0a, 0x05, 0x73, 0x74, 0x61, 0x74, 0x65, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x06,
	0x2e, 0x73, 0x74, 0x61, 0x74, 0x65, 0x52, 0x05, 0x73, 0x74, 0x61, 0x74, 0x65, 0x22, 0x4a, 0x0a,
	0x1b, 0x41, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x41, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x61,
	0x74, 0x65, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44, 0x61, 0x74, 0x61, 0x12, 0x2b, 0x0a, 0x04,
	0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x17, 0x2e, 0x41, 0x63, 0x74,
	0x69, 0x6f, 0x6e, 0x41, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x54, 0x61,
	0x62, 0x6c, 0x65, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f,
	0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_actionactionstate_config_proto_rawDescOnce sync.Once
	file_actionactionstate_config_proto_rawDescData = file_actionactionstate_config_proto_rawDesc
)

func file_actionactionstate_config_proto_rawDescGZIP() []byte {
	file_actionactionstate_config_proto_rawDescOnce.Do(func() {
		file_actionactionstate_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_actionactionstate_config_proto_rawDescData)
	})
	return file_actionactionstate_config_proto_rawDescData
}

var file_actionactionstate_config_proto_msgTypes = make([]protoimpl.MessageInfo, 3)
var file_actionactionstate_config_proto_goTypes = []any{
	(*State)(nil),                       // 0: state
	(*ActionActionStateTable)(nil),      // 1: ActionActionStateTable
	(*ActionActionStateTabledData)(nil), // 2: ActionActionStateTabledData
}
var file_actionactionstate_config_proto_depIdxs = []int32{
	0, // 0: ActionActionStateTable.state:type_name -> state
	1, // 1: ActionActionStateTabledData.data:type_name -> ActionActionStateTable
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_actionactionstate_config_proto_init() }
func file_actionactionstate_config_proto_init() {
	if File_actionactionstate_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_actionactionstate_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*State); i {
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
		file_actionactionstate_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*ActionActionStateTable); i {
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
		file_actionactionstate_config_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*ActionActionStateTabledData); i {
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
			RawDescriptor: file_actionactionstate_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   3,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_actionactionstate_config_proto_goTypes,
		DependencyIndexes: file_actionactionstate_config_proto_depIdxs,
		MessageInfos:      file_actionactionstate_config_proto_msgTypes,
	}.Build()
	File_actionactionstate_config_proto = out.File
	file_actionactionstate_config_proto_rawDesc = nil
	file_actionactionstate_config_proto_goTypes = nil
	file_actionactionstate_config_proto_depIdxs = nil
}
