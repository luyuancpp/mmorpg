// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: scene_error_tip.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef scene_5ferror_5ftip_2eproto_2epb_2eh
#define scene_5ferror_5ftip_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 6031000
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

#define PROTOBUF_INTERNAL_EXPORT_scene_5ferror_5ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_scene_5ferror_5ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_scene_5ferror_5ftip_2eproto;
}  // extern "C"
enum scene_error : int;
extern const uint32_t scene_error_internal_data_[];
namespace google {
namespace protobuf {
template <>
internal::EnumTraitsT<::scene_error_internal_data_>
    internal::EnumTraitsImpl::value<::scene_error>;
}  // namespace protobuf
}  // namespace google

enum scene_error : int {
  kScene_errorOK = 0,
  kEnterSceneNotFound = 30,
  kEnterSceneNotFull = 31,
  kEnterSceneMainFull = 32,
  kEnterGameNodeCrash = 33,
  kEnterSceneServerType = 34,
  kEnterSceneParamError = 35,
  kEnterSceneSceneFull = 36,
  kEnterSceneSceneNotFound = 37,
  kEnterSceneYouInCurrentScene = 38,
  kEnterSceneEnterCrossRoomScene = 39,
  kEnterSceneGsInfoNull = 40,
  kEnterSceneGsFull = 41,
  kEnterSceneYourSceneIsNull = 42,
  kEnterSceneChangeSceneOffLine = 43,
  kEnterSceneChangingScene = 44,
  kEnterSceneChangingGs = 45,
  kChangeScenePlayerQueueNotFound = 46,
  kChangeScenePlayerQueueFull = 47,
  kChangeScenePlayerQueueComponentGsNull = 48,
  kChangeScenePlayerQueueComponentEmpty = 49,
  kChangeSceneEnQueueNotSameGs = 50,
  kInvalidEnterSceneParameters = 51,
  kCheckEnterSceneCreator = 52,
  scene_error_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  scene_error_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

extern const uint32_t scene_error_internal_data_[];
inline constexpr scene_error scene_error_MIN =
    static_cast<scene_error>(0);
inline constexpr scene_error scene_error_MAX =
    static_cast<scene_error>(52);
inline bool scene_error_IsValid(int value) {
  return 0 <= value && value <= 52 && ((9007198180999169u >> value) & 1) != 0;
}
inline constexpr int scene_error_ARRAYSIZE = 52 + 1;
const ::google::protobuf::EnumDescriptor* PROTOBUF_NONNULL scene_error_descriptor();
template <typename T>
const std::string& scene_error_Name(T value) {
  static_assert(std::is_same<T, scene_error>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to scene_error_Name().");
  return ::google::protobuf::internal::NameOfEnum(scene_error_descriptor(), value);
}
inline bool scene_error_Parse(
    absl::string_view name, scene_error* PROTOBUF_NONNULL value) {
  return ::google::protobuf::internal::ParseNamedEnum<scene_error>(scene_error_descriptor(), name,
                                           value);
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
struct is_proto_enum<::scene_error> : std::true_type {};
template <>
inline const EnumDescriptor* PROTOBUF_NONNULL GetEnumDescriptor<::scene_error>() {
  return ::scene_error_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // scene_5ferror_5ftip_2eproto_2epb_2eh
