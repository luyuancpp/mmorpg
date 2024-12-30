// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: skill_error_tip.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_skill_5ferror_5ftip_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_skill_5ferror_5ftip_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/port_def.inc"
#if PROTOBUF_VERSION != 5026001
#error "Protobuf C++ gencode is built with an incompatible version of"
#error "Protobuf C++ headers/runtime. See"
#error "https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp"
#endif
#include "google/protobuf/port_undef.inc"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arenastring.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/generated_enum_reflection.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_skill_5ferror_5ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_skill_5ferror_5ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_skill_5ferror_5ftip_2eproto;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum skill_error : int {
  kSkill_errorOK = 0,
  kSkillUnInterruptible = 91,
  kSkillInvalidTargetId = 92,
  kSkillInvalidTarget = 93,
  kSkillCooldownNotReady = 94,
  kSkillCannotBeCastInCurrentState = 95,
  kSkillCannotBeCastSilenceRestriction = 96,
  kSkillCannotBeCastStunRestriction = 97,
  skill_error_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  skill_error_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool skill_error_IsValid(int value);
extern const uint32_t skill_error_internal_data_[];
constexpr skill_error skill_error_MIN = static_cast<skill_error>(0);
constexpr skill_error skill_error_MAX = static_cast<skill_error>(97);
constexpr int skill_error_ARRAYSIZE = 97 + 1;
const ::google::protobuf::EnumDescriptor*
skill_error_descriptor();
template <typename T>
const std::string& skill_error_Name(T value) {
  static_assert(std::is_same<T, skill_error>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to skill_error_Name().");
  return ::google::protobuf::internal::NameOfEnum(skill_error_descriptor(), value);
}
inline bool skill_error_Parse(absl::string_view name, skill_error* value) {
  return ::google::protobuf::internal::ParseNamedEnum<skill_error>(
      skill_error_descriptor(), name, value);
}

// ===================================================================



// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::skill_error> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::skill_error>() {
  return ::skill_error_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_skill_5ferror_5ftip_2eproto_2epb_2eh
