// Proto file for team_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: team_error_tip.proto

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

type TeamError int32

const (
	TeamError_kTeam_errorOK                TeamError = 0
	TeamError_kTeamNotInApplicants         TeamError = 68
	TeamError_kTeamPlayerId                TeamError = 69
	TeamError_kTeamMembersFull             TeamError = 70
	TeamError_kTeamMemberInTeam            TeamError = 71
	TeamError_kTeamMemberNotInTeam         TeamError = 72
	TeamError_kTeamKickSelf                TeamError = 73
	TeamError_kTeamKickNotLeader           TeamError = 74
	TeamError_kTeamAppointSelf             TeamError = 75
	TeamError_kTeamAppointLeaderNotLeader  TeamError = 76
	TeamError_kTeamFull                    TeamError = 77
	TeamError_kTeamInApplicantList         TeamError = 78
	TeamError_kTeamNotInApplicantList      TeamError = 79
	TeamError_kTeamListMaxSize             TeamError = 80
	TeamError_kTeamHasNotTeamId            TeamError = 81
	TeamError_kTeamDismissNotLeader        TeamError = 82
	TeamError_kTeamMemberListFull          TeamError = 83
	TeamError_kTeamCreateTeamMaxMemberSize TeamError = 84
	TeamError_kTeamPlayerNotFound          TeamError = 85
)

// Enum value maps for TeamError.
var (
	TeamError_name = map[int32]string{
		0:  "kTeam_errorOK",
		68: "kTeamNotInApplicants",
		69: "kTeamPlayerId",
		70: "kTeamMembersFull",
		71: "kTeamMemberInTeam",
		72: "kTeamMemberNotInTeam",
		73: "kTeamKickSelf",
		74: "kTeamKickNotLeader",
		75: "kTeamAppointSelf",
		76: "kTeamAppointLeaderNotLeader",
		77: "kTeamFull",
		78: "kTeamInApplicantList",
		79: "kTeamNotInApplicantList",
		80: "kTeamListMaxSize",
		81: "kTeamHasNotTeamId",
		82: "kTeamDismissNotLeader",
		83: "kTeamMemberListFull",
		84: "kTeamCreateTeamMaxMemberSize",
		85: "kTeamPlayerNotFound",
	}
	TeamError_value = map[string]int32{
		"kTeam_errorOK":                0,
		"kTeamNotInApplicants":         68,
		"kTeamPlayerId":                69,
		"kTeamMembersFull":             70,
		"kTeamMemberInTeam":            71,
		"kTeamMemberNotInTeam":         72,
		"kTeamKickSelf":                73,
		"kTeamKickNotLeader":           74,
		"kTeamAppointSelf":             75,
		"kTeamAppointLeaderNotLeader":  76,
		"kTeamFull":                    77,
		"kTeamInApplicantList":         78,
		"kTeamNotInApplicantList":      79,
		"kTeamListMaxSize":             80,
		"kTeamHasNotTeamId":            81,
		"kTeamDismissNotLeader":        82,
		"kTeamMemberListFull":          83,
		"kTeamCreateTeamMaxMemberSize": 84,
		"kTeamPlayerNotFound":          85,
	}
)

func (x TeamError) Enum() *TeamError {
	p := new(TeamError)
	*p = x
	return p
}

func (x TeamError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (TeamError) Descriptor() protoreflect.EnumDescriptor {
	return file_team_error_tip_proto_enumTypes[0].Descriptor()
}

func (TeamError) Type() protoreflect.EnumType {
	return &file_team_error_tip_proto_enumTypes[0]
}

func (x TeamError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use TeamError.Descriptor instead.
func (TeamError) EnumDescriptor() ([]byte, []int) {
	return file_team_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_team_error_tip_proto protoreflect.FileDescriptor

var file_team_error_tip_proto_rawDesc = []byte{
	0x0a, 0x14, 0x74, 0x65, 0x61, 0x6d, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69, 0x70,
	0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xd7, 0x03, 0x0a, 0x0a, 0x74, 0x65, 0x61, 0x6d, 0x5f,
	0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x11, 0x0a, 0x0d, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x5f, 0x65,
	0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x18, 0x0a, 0x14, 0x6b, 0x54, 0x65, 0x61,
	0x6d, 0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x6e, 0x74, 0x73,
	0x10, 0x44, 0x12, 0x11, 0x0a, 0x0d, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x50, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x49, 0x64, 0x10, 0x45, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65,
	0x6d, 0x62, 0x65, 0x72, 0x73, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x46, 0x12, 0x15, 0x0a, 0x11, 0x6b,
	0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65, 0x72, 0x49, 0x6e, 0x54, 0x65, 0x61, 0x6d,
	0x10, 0x47, 0x12, 0x18, 0x0a, 0x14, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65,
	0x72, 0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x54, 0x65, 0x61, 0x6d, 0x10, 0x48, 0x12, 0x11, 0x0a, 0x0d,
	0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4b, 0x69, 0x63, 0x6b, 0x53, 0x65, 0x6c, 0x66, 0x10, 0x49, 0x12,
	0x16, 0x0a, 0x12, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4b, 0x69, 0x63, 0x6b, 0x4e, 0x6f, 0x74, 0x4c,
	0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x4a, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x41, 0x70, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x53, 0x65, 0x6c, 0x66, 0x10, 0x4b, 0x12, 0x1f, 0x0a,
	0x1b, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x41, 0x70, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x4c, 0x65, 0x61,
	0x64, 0x65, 0x72, 0x4e, 0x6f, 0x74, 0x4c, 0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x4c, 0x12, 0x0d,
	0x0a, 0x09, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x4d, 0x12, 0x18, 0x0a,
	0x14, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x49, 0x6e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x6e,
	0x74, 0x4c, 0x69, 0x73, 0x74, 0x10, 0x4e, 0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x6e, 0x74, 0x4c, 0x69,
	0x73, 0x74, 0x10, 0x4f, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4c, 0x69, 0x73,
	0x74, 0x4d, 0x61, 0x78, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x50, 0x12, 0x15, 0x0a, 0x11, 0x6b, 0x54,
	0x65, 0x61, 0x6d, 0x48, 0x61, 0x73, 0x4e, 0x6f, 0x74, 0x54, 0x65, 0x61, 0x6d, 0x49, 0x64, 0x10,
	0x51, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x44, 0x69, 0x73, 0x6d, 0x69, 0x73,
	0x73, 0x4e, 0x6f, 0x74, 0x4c, 0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x52, 0x12, 0x17, 0x0a, 0x13,
	0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65, 0x72, 0x4c, 0x69, 0x73, 0x74, 0x46,
	0x75, 0x6c, 0x6c, 0x10, 0x53, 0x12, 0x20, 0x0a, 0x1c, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x43, 0x72,
	0x65, 0x61, 0x74, 0x65, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x61, 0x78, 0x4d, 0x65, 0x6d, 0x62, 0x65,
	0x72, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x54, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64, 0x10, 0x55,
	0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f,
	0x74, 0x6f, 0x33,
}

var (
	file_team_error_tip_proto_rawDescOnce sync.Once
	file_team_error_tip_proto_rawDescData = file_team_error_tip_proto_rawDesc
)

func file_team_error_tip_proto_rawDescGZIP() []byte {
	file_team_error_tip_proto_rawDescOnce.Do(func() {
		file_team_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_team_error_tip_proto_rawDescData)
	})
	return file_team_error_tip_proto_rawDescData
}

var file_team_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_team_error_tip_proto_goTypes = []any{
	(TeamError)(0), // 0: team_error
}
var file_team_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_team_error_tip_proto_init() }
func file_team_error_tip_proto_init() {
	if File_team_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_team_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_team_error_tip_proto_goTypes,
		DependencyIndexes: file_team_error_tip_proto_depIdxs,
		EnumInfos:         file_team_error_tip_proto_enumTypes,
	}.Build()
	File_team_error_tip_proto = out.File
	file_team_error_tip_proto_rawDesc = nil
	file_team_error_tip_proto_goTypes = nil
	file_team_error_tip_proto_depIdxs = nil
}
