// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: common_error_tip.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_common_5ferror_5ftip_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_common_5ferror_5ftip_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_common_5ferror_5ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_common_5ferror_5ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_common_5ferror_5ftip_2eproto;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum common_error : int {
  kCommon_errorOK = 0,
  kSuccess = 0,
  kInvalidTableId = 1,
  kInvalidTableData = 2,
  kServiceUnavailable = 3,
  kEntityIsNull = 4,
  kInvalidParameter = 5,
  kServerCrashed = 6,
  common_error_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  common_error_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool common_error_IsValid(int value);
extern const uint32_t common_error_internal_data_[];
constexpr common_error common_error_MIN = static_cast<common_error>(0);
constexpr common_error common_error_MAX = static_cast<common_error>(6);
constexpr int common_error_ARRAYSIZE = 6 + 1;
const ::google::protobuf::EnumDescriptor*
common_error_descriptor();
template <typename T>
const std::string& common_error_Name(T value) {
  static_assert(std::is_same<T, common_error>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to common_error_Name().");
  return common_error_Name(static_cast<common_error>(value));
}
template <>
inline const std::string& common_error_Name(common_error value) {
  return ::google::protobuf::internal::NameOfDenseEnum<common_error_descriptor,
                                                 0, 6>(
      static_cast<int>(value));
}
inline bool common_error_Parse(absl::string_view name, common_error* value) {
  return ::google::protobuf::internal::ParseNamedEnum<common_error>(
      common_error_descriptor(), name, value);
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
struct is_proto_enum<::common_error> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::common_error>() {
  return ::common_error_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_common_5ferror_5ftip_2eproto_2epb_2eh
