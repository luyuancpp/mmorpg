// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: bag_error_tip.proto
// Protobuf C++ Version: 5.29.0

#ifndef bag_5ferror_5ftip_2eproto_2epb_2eh
#define bag_5ferror_5ftip_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 5029000
#error "Protobuf C++ gencode is built with an incompatible version of"
#error "Protobuf C++ headers/runtime. See"
#error "https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp"
#endif
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

#define PROTOBUF_INTERNAL_EXPORT_bag_5ferror_5ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_bag_5ferror_5ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_bag_5ferror_5ftip_2eproto;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum bag_error : int {
  kBag_errorOK = 0,
  kBagDeleteItemFindGuid = 78,
  kBagDeleteItemAlreadyHasGuid = 79,
  kBagAddItemHasNotBaseComponent = 80,
  kBagAddItemInvalidGuid = 81,
  kBagAddItemInvalidParam = 82,
  kBagAddItemBagFull = 83,
  kBagItemNotStacked = 84,
  kBagInsufficientItems = 85,
  kBagDelItemPos = 86,
  kBagDelItemConfig = 87,
  kBagDelItemGuid = 88,
  kBagDelItemFindItem = 89,
  kBagDelItemSize = 90,
  kBagItemDeletionSizeMismatch = 91,
  kBagInsufficientBagSpace = 92,
  bag_error_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  bag_error_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool bag_error_IsValid(int value);
extern const uint32_t bag_error_internal_data_[];
constexpr bag_error bag_error_MIN = static_cast<bag_error>(0);
constexpr bag_error bag_error_MAX = static_cast<bag_error>(92);
constexpr int bag_error_ARRAYSIZE = 92 + 1;
const ::google::protobuf::EnumDescriptor*
bag_error_descriptor();
template <typename T>
const std::string& bag_error_Name(T value) {
  static_assert(std::is_same<T, bag_error>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to bag_error_Name().");
  return ::google::protobuf::internal::NameOfEnum(bag_error_descriptor(), value);
}
inline bool bag_error_Parse(absl::string_view name, bag_error* value) {
  return ::google::protobuf::internal::ParseNamedEnum<bag_error>(
      bag_error_descriptor(), name, value);
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
struct is_proto_enum<::bag_error> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::bag_error>() {
  return ::bag_error_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // bag_5ferror_5ftip_2eproto_2epb_2eh
