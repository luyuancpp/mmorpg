// Proto file for skill_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: skill_error_tip.proto

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

type SkillError int32

const (
	SkillError_kSkill_errorOK                   SkillError = 0
	SkillError_kSkillUnInterruptible            SkillError = 89
	SkillError_kSkillInvalidTargetId            SkillError = 90
	SkillError_kSkillInvalidTarget              SkillError = 91
	SkillError_kSkillCooldownNotReady           SkillError = 92
	SkillError_kSkillCannotBeCastInCurrentState SkillError = 93
)

// Enum value maps for SkillError.
var (
	SkillError_name = map[int32]string{
		0:  "kSkill_errorOK",
		89: "kSkillUnInterruptible",
		90: "kSkillInvalidTargetId",
		91: "kSkillInvalidTarget",
		92: "kSkillCooldownNotReady",
		93: "kSkillCannotBeCastInCurrentState",
	}
	SkillError_value = map[string]int32{
		"kSkill_errorOK":                   0,
		"kSkillUnInterruptible":            89,
		"kSkillInvalidTargetId":            90,
		"kSkillInvalidTarget":              91,
		"kSkillCooldownNotReady":           92,
		"kSkillCannotBeCastInCurrentState": 93,
	}
)

func (x SkillError) Enum() *SkillError {
	p := new(SkillError)
	*p = x
	return p
}

func (x SkillError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (SkillError) Descriptor() protoreflect.EnumDescriptor {
	return file_skill_error_tip_proto_enumTypes[0].Descriptor()
}

func (SkillError) Type() protoreflect.EnumType {
	return &file_skill_error_tip_proto_enumTypes[0]
}

func (x SkillError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use SkillError.Descriptor instead.
func (SkillError) EnumDescriptor() ([]byte, []int) {
	return file_skill_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_skill_error_tip_proto protoreflect.FileDescriptor

var file_skill_error_tip_proto_rawDesc = []byte{
	0x0a, 0x15, 0x73, 0x6b, 0x69, 0x6c, 0x6c, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xb2, 0x01, 0x0a, 0x0b, 0x73, 0x6b, 0x69, 0x6c,
	0x6c, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x53, 0x6b, 0x69, 0x6c,
	0x6c, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x19, 0x0a, 0x15, 0x6b,
	0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x55, 0x6e, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74,
	0x69, 0x62, 0x6c, 0x65, 0x10, 0x59, 0x12, 0x19, 0x0a, 0x15, 0x6b, 0x53, 0x6b, 0x69, 0x6c, 0x6c,
	0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x49, 0x64, 0x10,
	0x5a, 0x12, 0x17, 0x0a, 0x13, 0x6b, 0x53, 0x6b, 0x69, 0x6c, 0x6c, 0x49, 0x6e, 0x76, 0x61, 0x6c,
	0x69, 0x64, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x10, 0x5b, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x53,
	0x6b, 0x69, 0x6c, 0x6c, 0x43, 0x6f, 0x6f, 0x6c, 0x64, 0x6f, 0x77, 0x6e, 0x4e, 0x6f, 0x74, 0x52,
	0x65, 0x61, 0x64, 0x79, 0x10, 0x5c, 0x12, 0x24, 0x0a, 0x20, 0x6b, 0x53, 0x6b, 0x69, 0x6c, 0x6c,
	0x43, 0x61, 0x6e, 0x6e, 0x6f, 0x74, 0x42, 0x65, 0x43, 0x61, 0x73, 0x74, 0x49, 0x6e, 0x43, 0x75,
	0x72, 0x72, 0x65, 0x6e, 0x74, 0x53, 0x74, 0x61, 0x74, 0x65, 0x10, 0x5d, 0x42, 0x09, 0x5a, 0x07,
	0x70, 0x62, 0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_skill_error_tip_proto_rawDescOnce sync.Once
	file_skill_error_tip_proto_rawDescData = file_skill_error_tip_proto_rawDesc
)

func file_skill_error_tip_proto_rawDescGZIP() []byte {
	file_skill_error_tip_proto_rawDescOnce.Do(func() {
		file_skill_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_skill_error_tip_proto_rawDescData)
	})
	return file_skill_error_tip_proto_rawDescData
}

var file_skill_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_skill_error_tip_proto_goTypes = []any{
	(SkillError)(0), // 0: skill_error
}
var file_skill_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_skill_error_tip_proto_init() }
func file_skill_error_tip_proto_init() {
	if File_skill_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_skill_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_skill_error_tip_proto_goTypes,
		DependencyIndexes: file_skill_error_tip_proto_depIdxs,
		EnumInfos:         file_skill_error_tip_proto_enumTypes,
	}.Build()
	File_skill_error_tip_proto = out.File
	file_skill_error_tip_proto_rawDesc = nil
	file_skill_error_tip_proto_goTypes = nil
	file_skill_error_tip_proto_depIdxs = nil
}
