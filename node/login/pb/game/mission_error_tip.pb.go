// Proto file for mission_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: mission_error_tip.proto

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

type MissionError int32

const (
	MissionError_kMission_errorOK                MissionError = 0
	MissionError_kMissionTypeAlreadyExists       MissionError = 71
	MissionError_kMissionAlreadyCompleted        MissionError = 72
	MissionError_kMissionIdNotInRewardList       MissionError = 73
	MissionError_kPlayerMissionComponentNotFound MissionError = 74
	MissionError_kMissionIdRepeated              MissionError = 75
	MissionError_kConditionIdOutOfRange          MissionError = 76
	MissionError_kMissionNotInProgress           MissionError = 77
)

// Enum value maps for MissionError.
var (
	MissionError_name = map[int32]string{
		0:  "kMission_errorOK",
		71: "kMissionTypeAlreadyExists",
		72: "kMissionAlreadyCompleted",
		73: "kMissionIdNotInRewardList",
		74: "kPlayerMissionComponentNotFound",
		75: "kMissionIdRepeated",
		76: "kConditionIdOutOfRange",
		77: "kMissionNotInProgress",
	}
	MissionError_value = map[string]int32{
		"kMission_errorOK":                0,
		"kMissionTypeAlreadyExists":       71,
		"kMissionAlreadyCompleted":        72,
		"kMissionIdNotInRewardList":       73,
		"kPlayerMissionComponentNotFound": 74,
		"kMissionIdRepeated":              75,
		"kConditionIdOutOfRange":          76,
		"kMissionNotInProgress":           77,
	}
)

func (x MissionError) Enum() *MissionError {
	p := new(MissionError)
	*p = x
	return p
}

func (x MissionError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (MissionError) Descriptor() protoreflect.EnumDescriptor {
	return file_mission_error_tip_proto_enumTypes[0].Descriptor()
}

func (MissionError) Type() protoreflect.EnumType {
	return &file_mission_error_tip_proto_enumTypes[0]
}

func (x MissionError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use MissionError.Descriptor instead.
func (MissionError) EnumDescriptor() ([]byte, []int) {
	return file_mission_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_mission_error_tip_proto protoreflect.FileDescriptor

var file_mission_error_tip_proto_rawDesc = []byte{
	0x0a, 0x17, 0x6d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f,
	0x74, 0x69, 0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xf5, 0x01, 0x0a, 0x0d, 0x6d, 0x69,
	0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x14, 0x0a, 0x10, 0x6b,
	0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10,
	0x00, 0x12, 0x1d, 0x0a, 0x19, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x54, 0x79, 0x70,
	0x65, 0x41, 0x6c, 0x72, 0x65, 0x61, 0x64, 0x79, 0x45, 0x78, 0x69, 0x73, 0x74, 0x73, 0x10, 0x47,
	0x12, 0x1c, 0x0a, 0x18, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x41, 0x6c, 0x72, 0x65,
	0x61, 0x64, 0x79, 0x43, 0x6f, 0x6d, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x64, 0x10, 0x48, 0x12, 0x1d,
	0x0a, 0x19, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x4e, 0x6f, 0x74, 0x49,
	0x6e, 0x52, 0x65, 0x77, 0x61, 0x72, 0x64, 0x4c, 0x69, 0x73, 0x74, 0x10, 0x49, 0x12, 0x23, 0x0a,
	0x1f, 0x6b, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x43,
	0x6f, 0x6d, 0x70, 0x6f, 0x6e, 0x65, 0x6e, 0x74, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64,
	0x10, 0x4a, 0x12, 0x16, 0x0a, 0x12, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64,
	0x52, 0x65, 0x70, 0x65, 0x61, 0x74, 0x65, 0x64, 0x10, 0x4b, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x43,
	0x6f, 0x6e, 0x64, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x4f, 0x75, 0x74, 0x4f, 0x66, 0x52,
	0x61, 0x6e, 0x67, 0x65, 0x10, 0x4c, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x4d, 0x69, 0x73, 0x73, 0x69,
	0x6f, 0x6e, 0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x10,
	0x4d, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72,
	0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_mission_error_tip_proto_rawDescOnce sync.Once
	file_mission_error_tip_proto_rawDescData = file_mission_error_tip_proto_rawDesc
)

func file_mission_error_tip_proto_rawDescGZIP() []byte {
	file_mission_error_tip_proto_rawDescOnce.Do(func() {
		file_mission_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_mission_error_tip_proto_rawDescData)
	})
	return file_mission_error_tip_proto_rawDescData
}

var file_mission_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_mission_error_tip_proto_goTypes = []any{
	(MissionError)(0), // 0: mission_error
}
var file_mission_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_mission_error_tip_proto_init() }
func file_mission_error_tip_proto_init() {
	if File_mission_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_mission_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_mission_error_tip_proto_goTypes,
		DependencyIndexes: file_mission_error_tip_proto_depIdxs,
		EnumInfos:         file_mission_error_tip_proto_enumTypes,
	}.Build()
	File_mission_error_tip_proto = out.File
	file_mission_error_tip_proto_rawDesc = nil
	file_mission_error_tip_proto_goTypes = nil
	file_mission_error_tip_proto_depIdxs = nil
}
