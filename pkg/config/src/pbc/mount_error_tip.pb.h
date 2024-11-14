// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: mount_error_tip.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_mount_5ferror_5ftip_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_mount_5ferror_5ftip_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_mount_5ferror_5ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_mount_5ferror_5ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_mount_5ferror_5ftip_2eproto;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum mount_error : int {
  kMount_errorOK = 0,
  kMountNotMounted = 100,
  mount_error_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  mount_error_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool mount_error_IsValid(int value);
extern const uint32_t mount_error_internal_data_[];
constexpr mount_error mount_error_MIN = static_cast<mount_error>(0);
constexpr mount_error mount_error_MAX = static_cast<mount_error>(100);
constexpr int mount_error_ARRAYSIZE = 100 + 1;
const ::google::protobuf::EnumDescriptor*
mount_error_descriptor();
template <typename T>
const std::string& mount_error_Name(T value) {
  static_assert(std::is_same<T, mount_error>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to mount_error_Name().");
  return ::google::protobuf::internal::NameOfEnum(mount_error_descriptor(), value);
}
inline bool mount_error_Parse(absl::string_view name, mount_error* value) {
  return ::google::protobuf::internal::ParseNamedEnum<mount_error>(
      mount_error_descriptor(), name, value);
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
struct is_proto_enum<::mount_error> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::mount_error>() {
  return ::mount_error_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_mount_5ferror_5ftip_2eproto_2epb_2eh