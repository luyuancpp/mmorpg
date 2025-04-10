// Proto file for login_error

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.29.0
// source: login_error_tip.proto

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

type LoginError int32

const (
	LoginError_kLogin_errorOK                            LoginError = 0
	LoginError_kLoginAccountNotFound                     LoginError = 11
	LoginError_kLoginAccountPlayerFull                   LoginError = 12
	LoginError_kLoginCreatePlayerUnLoadAccount           LoginError = 13
	LoginError_kLoginCreatePlayerConnectionHasNotAccount LoginError = 14
	LoginError_kLoginUnLogin                             LoginError = 15
	LoginError_kLoginInProgress                          LoginError = 16
	LoginError_kLoginPlayerGuidError                     LoginError = 17
	LoginError_kLoginEnteringGame                        LoginError = 18
	LoginError_kLoginPlaying                             LoginError = 19
	LoginError_kLoginCreatingPlayer                      LoginError = 20
	LoginError_kLoginWaitingEnterGame                    LoginError = 21
	LoginError_kLoginEnterGameGuid                       LoginError = 22
	LoginError_kLoginAccountNameEmpty                    LoginError = 23
	LoginError_kLoginCreateConnectionAccountEmpty        LoginError = 24
	LoginError_kLoginEnterGameConnectionAccountEmpty     LoginError = 25
	LoginError_kLoginUnknownError                        LoginError = 26
	LoginError_kLoginSessionDisconnect                   LoginError = 27
	LoginError_kLoginBeKickByAnOtherAccount              LoginError = 28
	LoginError_kLoginSessionIdNotFound                   LoginError = 29
)

// Enum value maps for LoginError.
var (
	LoginError_name = map[int32]string{
		0:  "kLogin_errorOK",
		11: "kLoginAccountNotFound",
		12: "kLoginAccountPlayerFull",
		13: "kLoginCreatePlayerUnLoadAccount",
		14: "kLoginCreatePlayerConnectionHasNotAccount",
		15: "kLoginUnLogin",
		16: "kLoginInProgress",
		17: "kLoginPlayerGuidError",
		18: "kLoginEnteringGame",
		19: "kLoginPlaying",
		20: "kLoginCreatingPlayer",
		21: "kLoginWaitingEnterGame",
		22: "kLoginEnterGameGuid",
		23: "kLoginAccountNameEmpty",
		24: "kLoginCreateConnectionAccountEmpty",
		25: "kLoginEnterGameConnectionAccountEmpty",
		26: "kLoginUnknownError",
		27: "kLoginSessionDisconnect",
		28: "kLoginBeKickByAnOtherAccount",
		29: "kLoginSessionIdNotFound",
	}
	LoginError_value = map[string]int32{
		"kLogin_errorOK":                            0,
		"kLoginAccountNotFound":                     11,
		"kLoginAccountPlayerFull":                   12,
		"kLoginCreatePlayerUnLoadAccount":           13,
		"kLoginCreatePlayerConnectionHasNotAccount": 14,
		"kLoginUnLogin":                             15,
		"kLoginInProgress":                          16,
		"kLoginPlayerGuidError":                     17,
		"kLoginEnteringGame":                        18,
		"kLoginPlaying":                             19,
		"kLoginCreatingPlayer":                      20,
		"kLoginWaitingEnterGame":                    21,
		"kLoginEnterGameGuid":                       22,
		"kLoginAccountNameEmpty":                    23,
		"kLoginCreateConnectionAccountEmpty":        24,
		"kLoginEnterGameConnectionAccountEmpty":     25,
		"kLoginUnknownError":                        26,
		"kLoginSessionDisconnect":                   27,
		"kLoginBeKickByAnOtherAccount":              28,
		"kLoginSessionIdNotFound":                   29,
	}
)

func (x LoginError) Enum() *LoginError {
	p := new(LoginError)
	*p = x
	return p
}

func (x LoginError) String() string {
	return protoimpl.X.EnumStringOf(x.Descriptor(), protoreflect.EnumNumber(x))
}

func (LoginError) Descriptor() protoreflect.EnumDescriptor {
	return file_login_error_tip_proto_enumTypes[0].Descriptor()
}

func (LoginError) Type() protoreflect.EnumType {
	return &file_login_error_tip_proto_enumTypes[0]
}

func (x LoginError) Number() protoreflect.EnumNumber {
	return protoreflect.EnumNumber(x)
}

// Deprecated: Use LoginError.Descriptor instead.
func (LoginError) EnumDescriptor() ([]byte, []int) {
	return file_login_error_tip_proto_rawDescGZIP(), []int{0}
}

var File_login_error_tip_proto protoreflect.FileDescriptor

var file_login_error_tip_proto_rawDesc = []byte{
	0x0a, 0x15, 0x6c, 0x6f, 0x67, 0x69, 0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x5f, 0x74, 0x69,
	0x70, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x2a, 0xce, 0x04, 0x0a, 0x0b, 0x6c, 0x6f, 0x67, 0x69,
	0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x12, 0x12, 0x0a, 0x0e, 0x6b, 0x4c, 0x6f, 0x67, 0x69,
	0x6e, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x4f, 0x4b, 0x10, 0x00, 0x12, 0x19, 0x0a, 0x15, 0x6b,
	0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x4e, 0x6f, 0x74, 0x46,
	0x6f, 0x75, 0x6e, 0x64, 0x10, 0x0b, 0x12, 0x1b, 0x0a, 0x17, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e,
	0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x46, 0x75, 0x6c,
	0x6c, 0x10, 0x0c, 0x12, 0x23, 0x0a, 0x1f, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x43, 0x72, 0x65,
	0x61, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x55, 0x6e, 0x4c, 0x6f, 0x61, 0x64, 0x41,
	0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x10, 0x0d, 0x12, 0x2d, 0x0a, 0x29, 0x6b, 0x4c, 0x6f, 0x67,
	0x69, 0x6e, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x43, 0x6f,
	0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x48, 0x61, 0x73, 0x4e, 0x6f, 0x74, 0x41, 0x63,
	0x63, 0x6f, 0x75, 0x6e, 0x74, 0x10, 0x0e, 0x12, 0x11, 0x0a, 0x0d, 0x6b, 0x4c, 0x6f, 0x67, 0x69,
	0x6e, 0x55, 0x6e, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x10, 0x0f, 0x12, 0x14, 0x0a, 0x10, 0x6b, 0x4c,
	0x6f, 0x67, 0x69, 0x6e, 0x49, 0x6e, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x10, 0x10,
	0x12, 0x19, 0x0a, 0x15, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72,
	0x47, 0x75, 0x69, 0x64, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x10, 0x11, 0x12, 0x16, 0x0a, 0x12, 0x6b,
	0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6e, 0x67, 0x47, 0x61, 0x6d,
	0x65, 0x10, 0x12, 0x12, 0x11, 0x0a, 0x0d, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x50, 0x6c, 0x61,
	0x79, 0x69, 0x6e, 0x67, 0x10, 0x13, 0x12, 0x18, 0x0a, 0x14, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e,
	0x43, 0x72, 0x65, 0x61, 0x74, 0x69, 0x6e, 0x67, 0x50, 0x6c, 0x61, 0x79, 0x65, 0x72, 0x10, 0x14,
	0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6e,
	0x67, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x10, 0x15, 0x12, 0x17, 0x0a, 0x13,
	0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x47,
	0x75, 0x69, 0x64, 0x10, 0x16, 0x12, 0x1a, 0x0a, 0x16, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x41,
	0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x4e, 0x61, 0x6d, 0x65, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x10,
	0x17, 0x12, 0x26, 0x0a, 0x22, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x43, 0x72, 0x65, 0x61, 0x74,
	0x65, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x41, 0x63, 0x63, 0x6f, 0x75,
	0x6e, 0x74, 0x45, 0x6d, 0x70, 0x74, 0x79, 0x10, 0x18, 0x12, 0x29, 0x0a, 0x25, 0x6b, 0x4c, 0x6f,
	0x67, 0x69, 0x6e, 0x45, 0x6e, 0x74, 0x65, 0x72, 0x47, 0x61, 0x6d, 0x65, 0x43, 0x6f, 0x6e, 0x6e,
	0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x45, 0x6d, 0x70,
	0x74, 0x79, 0x10, 0x19, 0x12, 0x16, 0x0a, 0x12, 0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x55, 0x6e,
	0x6b, 0x6e, 0x6f, 0x77, 0x6e, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x10, 0x1a, 0x12, 0x1b, 0x0a, 0x17,
	0x6b, 0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x44, 0x69, 0x73,
	0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x10, 0x1b, 0x12, 0x20, 0x0a, 0x1c, 0x6b, 0x4c, 0x6f,
	0x67, 0x69, 0x6e, 0x42, 0x65, 0x4b, 0x69, 0x63, 0x6b, 0x42, 0x79, 0x41, 0x6e, 0x4f, 0x74, 0x68,
	0x65, 0x72, 0x41, 0x63, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x10, 0x1c, 0x12, 0x1b, 0x0a, 0x17, 0x6b,
	0x4c, 0x6f, 0x67, 0x69, 0x6e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x49, 0x64, 0x4e, 0x6f,
	0x74, 0x46, 0x6f, 0x75, 0x6e, 0x64, 0x10, 0x1d, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62, 0x2f, 0x67,
	0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_login_error_tip_proto_rawDescOnce sync.Once
	file_login_error_tip_proto_rawDescData = file_login_error_tip_proto_rawDesc
)

func file_login_error_tip_proto_rawDescGZIP() []byte {
	file_login_error_tip_proto_rawDescOnce.Do(func() {
		file_login_error_tip_proto_rawDescData = protoimpl.X.CompressGZIP(file_login_error_tip_proto_rawDescData)
	})
	return file_login_error_tip_proto_rawDescData
}

var file_login_error_tip_proto_enumTypes = make([]protoimpl.EnumInfo, 1)
var file_login_error_tip_proto_goTypes = []any{
	(LoginError)(0), // 0: login_error
}
var file_login_error_tip_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_login_error_tip_proto_init() }
func file_login_error_tip_proto_init() {
	if File_login_error_tip_proto != nil {
		return
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_login_error_tip_proto_rawDesc,
			NumEnums:      1,
			NumMessages:   0,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_login_error_tip_proto_goTypes,
		DependencyIndexes: file_login_error_tip_proto_depIdxs,
		EnumInfos:         file_login_error_tip_proto_enumTypes,
	}.Build()
	File_login_error_tip_proto = out.File
	file_login_error_tip_proto_rawDesc = nil
	file_login_error_tip_proto_goTypes = nil
	file_login_error_tip_proto_depIdxs = nil
}
