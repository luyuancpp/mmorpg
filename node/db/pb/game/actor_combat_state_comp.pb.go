// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/component/actor_combat_state_comp.proto

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

type ECombatState int32

const (
	ECombatState_kSilence ECombatState = 0
)

// Enum value maps for ECombatState.
var (
	ECombatState_name = map[int32]string{
		0: "kSilence",
	}
	ECombatState_value = map[string]int32{
		"kSilence": 0,
	}
)

func (x ECombatState) Enum() *ECombatState {
	p := new(ECombatState)
	*p = x
	return p
}

func (x ECombatState) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (ECombatState) Descriptor() protoreflect.EnumDescriptor {
	return file_proto_logic_component_actor_combat_state_comp_proto_enumTypes[0].Descriptor()
}

func (ECombatState) Type() protoreflect.EnumType {
	return &file_proto_logic_component_actor_combat_state_comp_proto_enumTypes[0]
}

func (x ECombatState) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use ECombatState.Descriptor instead.
func (ECombatState) EnumDescriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_combat_state_comp_proto_rawDescGZIP(), []int{0}
}

type CombatStateDetailsPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Sources map[uint64]bool `protobuf:"bytes,1,rep,name=sources,proto3" json:"sources,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
}

func (x *CombatStateDetailsPbComponent) Reset() {
	*x = CombatStateDetailsPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CombatStateDetailsPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CombatStateDetailsPbComponent) ProtoMessage() {}

func (x *CombatStateDetailsPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CombatStateDetailsPbComponent.ProtoReflect.Descriptor instead.
func (*CombatStateDetailsPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_combat_state_comp_proto_rawDescGZIP(), []int{0}
}

func (x *CombatStateDetailsPbComponent) GetSources() map[uint64]bool {
	if x != nil {
		return x.Sources
	}
	return nil
}

type CombatStateCollectionPbComponent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	States map[uint32]*CombatStateDetailsPbComponent `protobuf:"bytes,1,rep,name=states,proto3" json:"states,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
}

func (x *CombatStateCollectionPbComponent) Reset() {
	*x = CombatStateCollectionPbComponent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CombatStateCollectionPbComponent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CombatStateCollectionPbComponent) ProtoMessage() {}

func (x *CombatStateCollectionPbComponent) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CombatStateCollectionPbComponent.ProtoReflect.Descriptor instead.
func (*CombatStateCollectionPbComponent) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_actor_combat_state_comp_proto_rawDescGZIP(), []int{1}
}

func (x *CombatStateCollectionPbComponent) GetStates() map[uint32]*CombatStateDetailsPbComponent {
	if x != nil {
		return x.States
	}
	return nil
}

var File_proto_logic_component_actor_combat_state_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_actor_combat_state_comp_proto_rawDesc = []byte{
	0x0a, 0x33, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x63, 0x6f,
	0x6d, 0x62, 0x61, 0x74, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x1a, 0x20, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67,
	0x69, 0x63, 0x2f, 0x63, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x63, 0x6f, 0x6d,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0xa2, 0x01, 0x0a, 0x1d, 0x43, 0x6f, 0x6d, 0x62,
	0x61, 0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x44, 0x65, 0x74, 0x61, 0x69, 0x6c, 0x73, 0x50, 0x62,
	0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x12, 0x45, 0x0a, 0x07, 0x73, 0x6f, 0x75,
	0x72, 0x63, 0x65, 0x73, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x2b, 0x2e, 0x43, 0x6f, 0x6d,
	0x62, 0x61, 0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x44, 0x65, 0x74, 0x61, 0x69, 0x6c, 0x73, 0x50,
	0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x53, 0x6f, 0x75, 0x72, 0x63,
	0x65, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x07, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x73,
	0x1a, 0x3a, 0x0a, 0x0c, 0x53, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79,
	0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x03, 0x6b,
	0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28,
	0x08, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x22, 0xc4, 0x01, 0x0a,
	0x20, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x43, 0x6f, 0x6c, 0x6c,
	0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e,
	0x74, 0x12, 0x45, 0x0a, 0x06, 0x73, 0x74, 0x61, 0x74, 0x65, 0x73, 0x18, 0x01, 0x20, 0x03, 0x28,
	0x0b, 0x32, 0x2d, 0x2e, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x43,
	0x6f, 0x6c, 0x6c, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x50, 0x62, 0x43, 0x6f, 0x6d, 0x70, 0x6f,
	0x6e, 0x65, 0x6e, 0x74, 0x2e, 0x53, 0x74, 0x61, 0x74, 0x65, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79,
	0x52, 0x06, 0x73, 0x74, 0x61, 0x74, 0x65, 0x73, 0x1a, 0x59, 0x0a, 0x0b, 0x53, 0x74, 0x61, 0x74,
	0x65, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x34, 0x0a, 0x05, 0x76, 0x61, 0x6c,
	0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x1e, 0x2e, 0x43, 0x6f, 0x6d, 0x62, 0x61,
	0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x44, 0x65, 0x74, 0x61, 0x69, 0x6c, 0x73, 0x50, 0x62, 0x43,
	0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a,
	0x02, 0x38, 0x01, 0x2a, 0x1c, 0x0a, 0x0c, 0x65, 0x43, 0x6f, 0x6d, 0x62, 0x61, 0x74, 0x53, 0x74,
	0x61, 0x74, 0x65, 0x12, 0x0c, 0x0a, 0x08, 0x6b, 0x53, 0x69, 0x6c, 0x65, 0x6e, 0x63, 0x65, 0x10,
	0x00, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_component_actor_combat_state_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_actor_combat_state_comp_proto_rawDescData = file_proto_logic_component_actor_combat_state_comp_proto_rawDesc
)

func file_proto_logic_component_actor_combat_state_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_actor_combat_state_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_actor_combat_state_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_actor_combat_state_comp_proto_rawDescData)
	})
	return file_proto_logic_component_actor_combat_state_comp_proto_rawDescData
}

var file_proto_logic_component_actor_combat_state_comp_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_proto_logic_component_actor_combat_state_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_proto_logic_component_actor_combat_state_comp_proto_goTypes = []any{
	(ECombatState)(0),                        // 0: eCombatState
	(*CombatStateDetailsPbComponent)(nil),    // 1: CombatStateDetailsPbComponent
	(*CombatStateCollectionPbComponent)(nil), // 2: CombatStateCollectionPbComponent
	nil,                                      // 3: CombatStateDetailsPbComponent.SourcesEntry
	nil,                                      // 4: CombatStateCollectionPbComponent.StatesEntry
}
var file_proto_logic_component_actor_combat_state_comp_proto_depIdxs = []int32{
	3, // 0: CombatStateDetailsPbComponent.sources:type_name -> CombatStateDetailsPbComponent.SourcesEntry
	4, // 1: CombatStateCollectionPbComponent.states:type_name -> CombatStateCollectionPbComponent.StatesEntry
	1, // 2: CombatStateCollectionPbComponent.StatesEntry.value:type_name -> CombatStateDetailsPbComponent
	3, // [3:3] is the sub-list for method output_type
	3, // [3:3] is the sub-list for method input_type
	3, // [3:3] is the sub-list for extension type_name
	3, // [3:3] is the sub-list for extension extendee
	0, // [0:3] is the sub-list for field type_name
}

func init() { file_proto_logic_component_actor_combat_state_comp_proto_init() }
func file_proto_logic_component_actor_combat_state_comp_proto_init() {
	if File_proto_logic_component_actor_combat_state_comp_proto != nil {
		return
	}
	file_proto_logic_component_comp_proto_init()
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*CombatStateDetailsPbComponent); i {
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
		file_proto_logic_component_actor_combat_state_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*CombatStateCollectionPbComponent); i {
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
			RawDescriptor: file_proto_logic_component_actor_combat_state_comp_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_actor_combat_state_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_actor_combat_state_comp_proto_depIdxs,
		EnumInfos:         file_proto_logic_component_actor_combat_state_comp_proto_enumTypes,
		MessageInfos:      file_proto_logic_component_actor_combat_state_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_actor_combat_state_comp_proto = out.File
	file_proto_logic_component_actor_combat_state_comp_proto_rawDesc = nil
	file_proto_logic_component_actor_combat_state_comp_proto_goTypes = nil
	file_proto_logic_component_actor_combat_state_comp_proto_depIdxs = nil
}
