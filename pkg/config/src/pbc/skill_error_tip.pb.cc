// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: skill_error_tip.proto
// Protobuf C++ Version: 6.31.0-dev

#include "skill_error_tip.pb.h"

#include <algorithm>
#include <type_traits>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/generated_message_tctable_impl.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
static const ::_pb::EnumDescriptor* PROTOBUF_NONNULL
    file_level_enum_descriptors_skill_5ferror_5ftip_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_service_descriptors_skill_5ferror_5ftip_2eproto = nullptr;
const ::uint32_t TableStruct_skill_5ferror_5ftip_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* PROTOBUF_NULLABLE schemas = nullptr;
static constexpr ::_pb::Message* PROTOBUF_NONNULL const* PROTOBUF_NULLABLE
    file_default_instances = nullptr;
const char descriptor_table_protodef_skill_5ferror_5ftip_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\025skill_error_tip.proto*\203\002\n\013skill_error\022"
    "\022\n\016kSkill_errorOK\020\000\022\031\n\025kSkillUnInterrupt"
    "ible\020]\022\031\n\025kSkillInvalidTargetId\020^\022\027\n\023kSk"
    "illInvalidTarget\020_\022\032\n\026kSkillCooldownNotR"
    "eady\020`\022$\n kSkillCannotBeCastInCurrentSta"
    "te\020a\022(\n$kSkillCannotBeCastSilenceRestric"
    "tion\020b\022%\n!kSkillCannotBeCastStunRestrict"
    "ion\020cB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_skill_5ferror_5ftip_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_skill_5ferror_5ftip_2eproto = {
    false,
    false,
    304,
    descriptor_table_protodef_skill_5ferror_5ftip_2eproto,
    "skill_error_tip.proto",
    &descriptor_table_skill_5ferror_5ftip_2eproto_once,
    nullptr,
    0,
    0,
    schemas,
    file_default_instances,
    TableStruct_skill_5ferror_5ftip_2eproto::offsets,
    file_level_enum_descriptors_skill_5ferror_5ftip_2eproto,
    file_level_service_descriptors_skill_5ferror_5ftip_2eproto,
};
const ::google::protobuf::EnumDescriptor* PROTOBUF_NONNULL skill_error_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_skill_5ferror_5ftip_2eproto);
  return file_level_enum_descriptors_skill_5ferror_5ftip_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t skill_error_internal_data_[] = {
    65536u, 128u, 0u, 0u, 4026531840u, 7u, };
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ [[maybe_unused]] =
        (::_pbi::AddDescriptors(&descriptor_table_skill_5ferror_5ftip_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
