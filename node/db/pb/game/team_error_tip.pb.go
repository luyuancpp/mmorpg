// Proto file for team_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
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
	TeamError_kTeamNotInApplicants         TeamError = 52
	TeamError_kTeamPlayerId                TeamError = 53
	TeamError_kTeamMembersFull             TeamError = 54
	TeamError_kTeamMemberInTeam            TeamError = 55
	TeamError_kTeamMemberNotInTeam         TeamError = 56
	TeamError_kTeamKickSelf                TeamError = 57
	TeamError_kTeamKickNotLeader           TeamError = 58
	TeamError_kTeamAppointSelf             TeamError = 59
	TeamError_kTeamAppointLeaderNotLeader  TeamError = 60
	TeamError_kTeamFull                    TeamError = 61
	TeamError_kTeamInApplicantList         TeamError = 62
	TeamError_kTeamNotInApplicantList      TeamError = 63
	TeamError_kTeamListMaxSize             TeamError = 64
	TeamError_kTeamHasNotTeamId            TeamError = 65
	TeamError_kTeamDismissNotLeader        TeamError = 66
	TeamError_kTeamMemberListFull          TeamError = 67
	TeamError_kTeamCreateTeamMaxMemberSize TeamError = 68
	TeamError_kTeamPlayerNotFound          TeamError = 69
)

// Enum value maps for TeamError.
var (
	TeamError_name = map[int32]string{
		0:  "kTeam_errorOK",
		52: "kTeamNotInApplicants",
		53: "kTeamPlayerId",
		54: "kTeamMembersFull",
		55: "kTeamMemberInTeam",
		56: "kTeamMemberNotInTeam",
		57: "kTeamKickSelf",
		58: "kTeamKickNotLeader",
		59: "kTeamAppointSelf",
		60: "kTeamAppointLeaderNotLeader",
		61: "kTeamFull",
		62: "kTeamInApplicantList",
		63: "kTeamNotInApplicantList",
		64: "kTeamListMaxSize",
		65: "kTeamHasNotTeamId",
		66: "kTeamDismissNotLeader",
		67: "kTeamMemberListFull",
		68: "kTeamCreateTeamMaxMemberSize",
		69: "kTeamPlayerNotFound",
	}
	TeamError_value = map[string]int32{
		"kTeam_errorOK":                0,
		"kTeamNotInApplicants":         52,
		"kTeamPlayerId":                53,
		"kTeamMembersFull":             54,
		"kTeamMemberInTeam":            55,
		"kTeamMemberNotInTeam":         56,
		"kTeamKickSelf":                57,
		"kTeamKickNotLeader":           58,
		"kTeamAppointSelf":             59,
		"kTeamAppointLeaderNotLeader":  60,
		"kTeamFull":                    61,
		"kTeamInApplicantList":         62,
		"kTeamNotInApplicantList":      63,
		"kTeamListMaxSize":             64,
		"kTeamHasNotTeamId":            65,
		"kTeamDismissNotLeader":        66,
		"kTeamMemberListFull":          67,
		"kTeamCreateTeamMaxMemberSize": 68,
		"kTeamPlayerNotFound":          69,
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
	0x10, 0x34, 0x12, 0x11, 0x0a, 0x0d, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x50, 0x6c, 0x61, 0x79, 0x65,
	0x72, 0x49, 0x64, 0x10, 0x35, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65,
	0x6d, 0x62, 0x65, 0x72, 0x73, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x36, 0x12, 0x15, 0x0a, 0x11, 0x6b,
	0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65, 0x72, 0x49, 0x6e, 0x54, 0x65, 0x61, 0x6d,
	0x10, 0x37, 0x12, 0x18, 0x0a, 0x14, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65,
	0x72, 0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x54, 0x65, 0x61, 0x6d, 0x10, 0x38, 0x12, 0x11, 0x0a, 0x0d,
	0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4b, 0x69, 0x63, 0x6b, 0x53, 0x65, 0x6c, 0x66, 0x10, 0x39, 0x12,
	0x16, 0x0a, 0x12, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4b, 0x69, 0x63, 0x6b, 0x4e, 0x6f, 0x74, 0x4c,
	0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x3a, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x41, 0x70, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x53, 0x65, 0x6c, 0x66, 0x10, 0x3b, 0x12, 0x1f, 0x0a,
	0x1b, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x41, 0x70, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x4c, 0x65, 0x61,
	0x64, 0x65, 0x72, 0x4e, 0x6f, 0x74, 0x4c, 0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x3c, 0x12, 0x0d,
	0x0a, 0x09, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x46, 0x75, 0x6c, 0x6c, 0x10, 0x3d, 0x12, 0x18, 0x0a,
	0x14, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x49, 0x6e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x6e,
	0x74, 0x4c, 0x69, 0x73, 0x74, 0x10, 0x3e, 0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x4e, 0x6f, 0x74, 0x49, 0x6e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x6e, 0x74, 0x4c, 0x69,
	0x73, 0x74, 0x10, 0x3f, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4c, 0x69, 0x73,
	0x74, 0x4d, 0x61, 0x78, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x40, 0x12, 0x15, 0x0a, 0x11, 0x6b, 0x54,
	0x65, 0x61, 0x6d, 0x48, 0x61, 0x73, 0x4e, 0x6f, 0x74, 0x54, 0x65, 0x61, 0x6d, 0x49, 0x64, 0x10,
	0x41, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x44, 0x69, 0x73, 0x6d, 0x69, 0x73,
	0x73, 0x4e, 0x6f, 0x74, 0x4c, 0x65, 0x61, 0x64, 0x65, 0x72, 0x10, 0x42, 0x12, 0x17, 0x0a, 0x13,
	0x6b, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x65, 0x6d, 0x62, 0x65, 0x72, 0x4c, 0x69, 0x73, 0x74, 0x46,
	0x75, 0x6c, 0x6c, 0x10, 0x43, 0x12, 0x20, 0x0a, 0x1c, 0x6b, 0x54, 0x65, 0x61, 0x6d, 0x43, 0x72,
	0x65, 0x61, 0x74, 0x65, 0x54, 0x65, 0x61, 0x6d, 0x4d, 0x61, 0x78, 0x4d, 0x65, 0x6d, 0x62, 0x65,
	0x72, 0x53, 0x69, 0x7a, 0x65, 0x10, 0x44, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x54, 0x65, 0x61, 0x6d,
	0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x4e, 0x6f, 0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64, 0x10, 0x45,
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
