// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: proto/logic/component/time_comp.proto

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

type TimeMeterComp struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Start    uint64 `protobuf:"varint,1,opt,name=start,proto3" json:"start,omitempty"`
	Duration uint64 `protobuf:"varint,2,opt,name=duration,proto3" json:"duration,omitempty"`
}

func (x *TimeMeterComp) Reset() {
	*x = TimeMeterComp{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_time_comp_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *TimeMeterComp) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*TimeMeterComp) ProtoMessage() {}

func (x *TimeMeterComp) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_time_comp_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use TimeMeterComp.ProtoReflect.Descriptor instead.
func (*TimeMeterComp) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_time_comp_proto_rawDescGZIP(), []int{0}
}

func (x *TimeMeterComp) GetStart() uint64 {
	if x != nil {
		return x.Start
	}
	return 0
}

func (x *TimeMeterComp) GetDuration() uint64 {
	if x != nil {
		return x.Duration
	}
	return 0
}

type CooldownTimeComp struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Start           uint64 `protobuf:"varint,1,opt,name=start,proto3" json:"start,omitempty"`
	CooldownTableId uint32 `protobuf:"varint,2,opt,name=cooldown_table_id,json=cooldownTableId,proto3" json:"cooldown_table_id,omitempty"`
}

func (x *CooldownTimeComp) Reset() {
	*x = CooldownTimeComp{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_time_comp_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CooldownTimeComp) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CooldownTimeComp) ProtoMessage() {}

func (x *CooldownTimeComp) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_time_comp_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CooldownTimeComp.ProtoReflect.Descriptor instead.
func (*CooldownTimeComp) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_time_comp_proto_rawDescGZIP(), []int{1}
}

func (x *CooldownTimeComp) GetStart() uint64 {
	if x != nil {
		return x.Start
	}
	return 0
}

func (x *CooldownTimeComp) GetCooldownTableId() uint32 {
	if x != nil {
		return x.CooldownTableId
	}
	return 0
}

type CooldownTimeListComp struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	CooldownList map[uint32]*CooldownTimeComp `protobuf:"bytes,1,rep,name=cooldown_list,json=cooldownList,proto3" json:"cooldown_list,omitempty" protobuf_key:"varint,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
}

func (x *CooldownTimeListComp) Reset() {
	*x = CooldownTimeListComp{}
	if protoimpl.UnsafeEnabled {
		mi := &file_proto_logic_component_time_comp_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CooldownTimeListComp) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CooldownTimeListComp) ProtoMessage() {}

func (x *CooldownTimeListComp) ProtoReflect() protoreflect.Message {
	mi := &file_proto_logic_component_time_comp_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CooldownTimeListComp.ProtoReflect.Descriptor instead.
func (*CooldownTimeListComp) Descriptor() ([]byte, []int) {
	return file_proto_logic_component_time_comp_proto_rawDescGZIP(), []int{2}
}

func (x *CooldownTimeListComp) GetCooldownList() map[uint32]*CooldownTimeComp {
	if x != nil {
		return x.CooldownList
	}
	return nil
}

var File_proto_logic_component_time_comp_proto protoreflect.FileDescriptor

var file_proto_logic_component_time_comp_proto_rawDesc = []byte{
	0x0a, 0x25, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2f, 0x6c, 0x6f, 0x67, 0x69, 0x63, 0x2f, 0x63, 0x6f,
	0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x2f, 0x74, 0x69, 0x6d, 0x65, 0x5f, 0x63, 0x6f, 0x6d,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x41, 0x0a, 0x0d, 0x54, 0x69, 0x6d, 0x65, 0x4d,
	0x65, 0x74, 0x65, 0x72, 0x43, 0x6f, 0x6d, 0x70, 0x12, 0x14, 0x0a, 0x05, 0x73, 0x74, 0x61, 0x72,
	0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x05, 0x73, 0x74, 0x61, 0x72, 0x74, 0x12, 0x1a,
	0x0a, 0x08, 0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x02, 0x20, 0x01, 0x28, 0x04,
	0x52, 0x08, 0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x22, 0x54, 0x0a, 0x10, 0x43, 0x6f,
	0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x54, 0x69, 0x6d, 0x65, 0x43, 0x6f, 0x6d, 0x70, 0x12, 0x14,
	0x0a, 0x05, 0x73, 0x74, 0x61, 0x72, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x04, 0x52, 0x05, 0x73,
	0x74, 0x61, 0x72, 0x74, 0x12, 0x2a, 0x0a, 0x11, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e,
	0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x0f, 0x63, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x49, 0x64,
	0x22, 0xb8, 0x01, 0x0a, 0x14, 0x43, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x54, 0x69, 0x6d,
	0x65, 0x4c, 0x69, 0x73, 0x74, 0x43, 0x6f, 0x6d, 0x70, 0x12, 0x4c, 0x0a, 0x0d, 0x63, 0x6f, 0x6f,
	0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x5f, 0x6c, 0x69, 0x73, 0x74, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b,
	0x32, 0x27, 0x2e, 0x43, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x54, 0x69, 0x6d, 0x65, 0x4c,
	0x69, 0x73, 0x74, 0x43, 0x6f, 0x6d, 0x70, 0x2e, 0x43, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e,
	0x4c, 0x69, 0x73, 0x74, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x0c, 0x63, 0x6f, 0x6f, 0x6c, 0x64,
	0x6f, 0x77, 0x6e, 0x4c, 0x69, 0x73, 0x74, 0x1a, 0x52, 0x0a, 0x11, 0x43, 0x6f, 0x6f, 0x6c, 0x64,
	0x6f, 0x77, 0x6e, 0x4c, 0x69, 0x73, 0x74, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03,
	0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x27,
	0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x11, 0x2e,
	0x43, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x54, 0x69, 0x6d, 0x65, 0x43, 0x6f, 0x6d, 0x70,
	0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x42, 0x09, 0x5a, 0x07, 0x70,
	0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_proto_logic_component_time_comp_proto_rawDescOnce sync.Once
	file_proto_logic_component_time_comp_proto_rawDescData = file_proto_logic_component_time_comp_proto_rawDesc
)

func file_proto_logic_component_time_comp_proto_rawDescGZIP() []byte {
	file_proto_logic_component_time_comp_proto_rawDescOnce.Do(func() {
		file_proto_logic_component_time_comp_proto_rawDescData = protoimpl.X.CompressGZIP(file_proto_logic_component_time_comp_proto_rawDescData)
	})
	return file_proto_logic_component_time_comp_proto_rawDescData
}

var file_proto_logic_component_time_comp_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_proto_logic_component_time_comp_proto_goTypes = []any{
	(*TimeMeterComp)(nil),        // 0: TimeMeterComp
	(*CooldownTimeComp)(nil),     // 1: CooldownTimeComp
	(*CooldownTimeListComp)(nil), // 2: CooldownTimeListComp
	nil,                          // 3: CooldownTimeListComp.CooldownListEntry
}
var file_proto_logic_component_time_comp_proto_depIdxs = []int32{
	3, // 0: CooldownTimeListComp.cooldown_list:type_name -> CooldownTimeListComp.CooldownListEntry
	1, // 1: CooldownTimeListComp.CooldownListEntry.value:type_name -> CooldownTimeComp
	2, // [2:2] is the sub-list for method output_type
	2, // [2:2] is the sub-list for method input_type
	2, // [2:2] is the sub-list for extension type_name
	2, // [2:2] is the sub-list for extension extendee
	0, // [0:2] is the sub-list for field type_name
}

func init() { file_proto_logic_component_time_comp_proto_init() }
func file_proto_logic_component_time_comp_proto_init() {
	if File_proto_logic_component_time_comp_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_proto_logic_component_time_comp_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*TimeMeterComp); i {
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
		file_proto_logic_component_time_comp_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*CooldownTimeComp); i {
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
		file_proto_logic_component_time_comp_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*CooldownTimeListComp); i {
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
			RawDescriptor: file_proto_logic_component_time_comp_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_proto_logic_component_time_comp_proto_goTypes,
		DependencyIndexes: file_proto_logic_component_time_comp_proto_depIdxs,
		MessageInfos:      file_proto_logic_component_time_comp_proto_msgTypes,
	}.Build()
	File_proto_logic_component_time_comp_proto = out.File
	file_proto_logic_component_time_comp_proto_rawDesc = nil
	file_proto_logic_component_time_comp_proto_goTypes = nil
	file_proto_logic_component_time_comp_proto_depIdxs = nil
}
