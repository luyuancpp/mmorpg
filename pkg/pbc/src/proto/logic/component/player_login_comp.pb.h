// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/player_login_comp.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto_2epb_2eh
#define proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto_2epb_2eh

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
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto;
class PlayerEnterGameStatePbComp;
struct PlayerEnterGameStatePbCompDefaultTypeInternal;
extern PlayerEnterGameStatePbCompDefaultTypeInternal _PlayerEnterGameStatePbComp_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum EnterGsType : int {
  LOGIN_NONE = 0,
  LOGIN_FIRST = 1,
  LOGIN_REPLACE = 2,
  LOGIN_RECONNECT = 3,
  EnterGsType_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  EnterGsType_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool EnterGsType_IsValid(int value);
extern const uint32_t EnterGsType_internal_data_[];
constexpr EnterGsType EnterGsType_MIN = static_cast<EnterGsType>(0);
constexpr EnterGsType EnterGsType_MAX = static_cast<EnterGsType>(3);
constexpr int EnterGsType_ARRAYSIZE = 3 + 1;
const ::google::protobuf::EnumDescriptor*
EnterGsType_descriptor();
template <typename T>
const std::string& EnterGsType_Name(T value) {
  static_assert(std::is_same<T, EnterGsType>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to EnterGsType_Name().");
  return EnterGsType_Name(static_cast<EnterGsType>(value));
}
template <>
inline const std::string& EnterGsType_Name(EnterGsType value) {
  return ::google::protobuf::internal::NameOfDenseEnum<EnterGsType_descriptor,
                                                 0, 3>(
      static_cast<int>(value));
}
inline bool EnterGsType_Parse(absl::string_view name, EnterGsType* value) {
  return ::google::protobuf::internal::ParseNamedEnum<EnterGsType>(
      EnterGsType_descriptor(), name, value);
}

// ===================================================================


// -------------------------------------------------------------------

class PlayerEnterGameStatePbComp final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerEnterGameStatePbComp) */ {
 public:
  inline PlayerEnterGameStatePbComp() : PlayerEnterGameStatePbComp(nullptr) {}
  ~PlayerEnterGameStatePbComp() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(PlayerEnterGameStatePbComp* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(PlayerEnterGameStatePbComp));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerEnterGameStatePbComp(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerEnterGameStatePbComp(const PlayerEnterGameStatePbComp& from) : PlayerEnterGameStatePbComp(nullptr, from) {}
  inline PlayerEnterGameStatePbComp(PlayerEnterGameStatePbComp&& from) noexcept
      : PlayerEnterGameStatePbComp(nullptr, std::move(from)) {}
  inline PlayerEnterGameStatePbComp& operator=(const PlayerEnterGameStatePbComp& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerEnterGameStatePbComp& operator=(PlayerEnterGameStatePbComp&& from) noexcept {
    if (this == &from) return *this;
    if (::google::protobuf::internal::CanMoveWithInternalSwap(GetArena(), from.GetArena())) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const PlayerEnterGameStatePbComp& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerEnterGameStatePbComp* internal_default_instance() {
    return reinterpret_cast<const PlayerEnterGameStatePbComp*>(
        &_PlayerEnterGameStatePbComp_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(PlayerEnterGameStatePbComp& a, PlayerEnterGameStatePbComp& b) { a.Swap(&b); }
  inline void Swap(PlayerEnterGameStatePbComp* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(PlayerEnterGameStatePbComp* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerEnterGameStatePbComp* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<PlayerEnterGameStatePbComp>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerEnterGameStatePbComp& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerEnterGameStatePbComp& from) { PlayerEnterGameStatePbComp::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* _InternalSerialize(
      const MessageLite& msg, ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(PlayerEnterGameStatePbComp* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "PlayerEnterGameStatePbComp"; }

 protected:
  explicit PlayerEnterGameStatePbComp(::google::protobuf::Arena* arena);
  PlayerEnterGameStatePbComp(::google::protobuf::Arena* arena, const PlayerEnterGameStatePbComp& from);
  PlayerEnterGameStatePbComp(::google::protobuf::Arena* arena, PlayerEnterGameStatePbComp&& from) noexcept
      : PlayerEnterGameStatePbComp(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kEnterGsTypeFieldNumber = 1,
  };
  // uint32 enter_gs_type = 1;
  void clear_enter_gs_type() ;
  ::uint32_t enter_gs_type() const;
  void set_enter_gs_type(::uint32_t value);

  private:
  ::uint32_t _internal_enter_gs_type() const;
  void _internal_set_enter_gs_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:PlayerEnterGameStatePbComp)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 0,
      0, 2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const PlayerEnterGameStatePbComp& from_msg);
    ::uint32_t enter_gs_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// PlayerEnterGameStatePbComp

// uint32 enter_gs_type = 1;
inline void PlayerEnterGameStatePbComp::clear_enter_gs_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enter_gs_type_ = 0u;
}
inline ::uint32_t PlayerEnterGameStatePbComp::enter_gs_type() const {
  // @@protoc_insertion_point(field_get:PlayerEnterGameStatePbComp.enter_gs_type)
  return _internal_enter_gs_type();
}
inline void PlayerEnterGameStatePbComp::set_enter_gs_type(::uint32_t value) {
  _internal_set_enter_gs_type(value);
  // @@protoc_insertion_point(field_set:PlayerEnterGameStatePbComp.enter_gs_type)
}
inline ::uint32_t PlayerEnterGameStatePbComp::_internal_enter_gs_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.enter_gs_type_;
}
inline void PlayerEnterGameStatePbComp::_internal_set_enter_gs_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enter_gs_type_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::EnterGsType> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::EnterGsType>() {
  return ::EnterGsType_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fcomponent_2fplayer_5flogin_5fcomp_2eproto_2epb_2eh
