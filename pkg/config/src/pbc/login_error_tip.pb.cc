// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: login_error_tip.proto
// Protobuf C++ Version: 5.26.1

#include "login_error_tip.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/generated_message_tctable_impl.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_login_5ferror_5ftip_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_login_5ferror_5ftip_2eproto = nullptr;
const ::uint32_t TableStruct_login_5ferror_5ftip_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* schemas = nullptr;
static constexpr ::_pb::Message* const* file_default_instances = nullptr;
const char descriptor_table_protodef_login_5ferror_5ftip_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\025login_error_tip.proto*\316\004\n\013login_error\022"
    "\022\n\016kLogin_errorOK\020\000\022\031\n\025kLoginAccountNotF"
    "ound\020\013\022\033\n\027kLoginAccountPlayerFull\020\014\022#\n\037k"
    "LoginCreatePlayerUnLoadAccount\020\r\022-\n)kLog"
    "inCreatePlayerConnectionHasNotAccount\020\016\022"
    "\021\n\rkLoginUnLogin\020\017\022\024\n\020kLoginInProgress\020\020"
    "\022\031\n\025kLoginPlayerGuidError\020\021\022\026\n\022kLoginEnt"
    "eringGame\020\022\022\021\n\rkLoginPlaying\020\023\022\030\n\024kLogin"
    "CreatingPlayer\020\024\022\032\n\026kLoginWaitingEnterGa"
    "me\020\025\022\027\n\023kLoginEnterGameGuid\020\026\022\032\n\026kLoginA"
    "ccountNameEmpty\020\027\022&\n\"kLoginCreateConnect"
    "ionAccountEmpty\020\030\022)\n%kLoginEnterGameConn"
    "ectionAccountEmpty\020\031\022\026\n\022kLoginUnknownErr"
    "or\020\032\022\033\n\027kLoginSessionDisconnect\020\033\022 \n\034kLo"
    "ginBeKickByAnOtherAccount\020\034\022\033\n\027kLoginSes"
    "sionIdNotFound\020\035B\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_login_5ferror_5ftip_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_login_5ferror_5ftip_2eproto = {
    false,
    false,
    635,
    descriptor_table_protodef_login_5ferror_5ftip_2eproto,
    "login_error_tip.proto",
    &descriptor_table_login_5ferror_5ftip_2eproto_once,
    nullptr,
    0,
    0,
    schemas,
    file_default_instances,
    TableStruct_login_5ferror_5ftip_2eproto::offsets,
    nullptr,
    file_level_enum_descriptors_login_5ferror_5ftip_2eproto,
    file_level_service_descriptors_login_5ferror_5ftip_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_login_5ferror_5ftip_2eproto_getter() {
  return &descriptor_table_login_5ferror_5ftip_2eproto;
}
const ::google::protobuf::EnumDescriptor* login_error_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_login_5ferror_5ftip_2eproto);
  return file_level_enum_descriptors_login_5ferror_5ftip_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t login_error_internal_data_[] = {
    65536u, 32u, 536869888u, };
bool login_error_IsValid(int value) {
  return 0 <= value && value <= 29 && ((1073739777u >> value) & 1) != 0;
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_login_5ferror_5ftip_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
