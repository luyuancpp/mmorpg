// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: actor_action_error_tip.proto
// Protobuf C++ Version: 5.26.1

#include "actor_action_error_tip.pb.h"

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
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_actor_5faction_5ferror_5ftip_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_actor_5faction_5ferror_5ftip_2eproto = nullptr;
const ::uint32_t TableStruct_actor_5faction_5ferror_5ftip_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* schemas = nullptr;
static constexpr ::_pb::Message* const* file_default_instances = nullptr;
const char descriptor_table_protodef_actor_5faction_5ferror_5ftip_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\034actor_action_error_tip.proto*r\n\022actor_"
    "action_error\022\031\n\025kActor_action_errorOK\020\000\022"
    "A\n=kActorActionStateActionCannotBePerfor"
    "medWhileInTeamFollowMode\020gB\tZ\007pb/gameb\006p"
    "roto3"
};
static ::absl::once_flag descriptor_table_actor_5faction_5ferror_5ftip_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_actor_5faction_5ferror_5ftip_2eproto = {
    false,
    false,
    165,
    descriptor_table_protodef_actor_5faction_5ferror_5ftip_2eproto,
    "actor_action_error_tip.proto",
    &descriptor_table_actor_5faction_5ferror_5ftip_2eproto_once,
    nullptr,
    0,
    0,
    schemas,
    file_default_instances,
    TableStruct_actor_5faction_5ferror_5ftip_2eproto::offsets,
    nullptr,
    file_level_enum_descriptors_actor_5faction_5ferror_5ftip_2eproto,
    file_level_service_descriptors_actor_5faction_5ferror_5ftip_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_actor_5faction_5ferror_5ftip_2eproto_getter() {
  return &descriptor_table_actor_5faction_5ferror_5ftip_2eproto;
}
const ::google::protobuf::EnumDescriptor* actor_action_error_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_actor_5faction_5ferror_5ftip_2eproto);
  return file_level_enum_descriptors_actor_5faction_5ferror_5ftip_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t actor_action_error_internal_data_[] = {
    65536u, 65536u, 103u, };
bool actor_action_error_IsValid(int value) {
  return ::_pbi::ValidateEnum(value, actor_action_error_internal_data_);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_actor_5faction_5ferror_5ftip_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
