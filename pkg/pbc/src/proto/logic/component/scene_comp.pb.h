// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/scene_comp.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto_2epb_2eh
#define proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto;
class ChangeSceneInfoPBComponent;
struct ChangeSceneInfoPBComponentDefaultTypeInternal;
extern ChangeSceneInfoPBComponentDefaultTypeInternal _ChangeSceneInfoPBComponent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

enum ChangeSceneInfoPBComponent_eChangeGsType : int {
  ChangeSceneInfoPBComponent_eChangeGsType_eSameGs = 0,
  ChangeSceneInfoPBComponent_eChangeGsType_eDifferentGs = 1,
  ChangeSceneInfoPBComponent_eChangeGsType_ChangeSceneInfoPBComponent_eChangeGsType_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  ChangeSceneInfoPBComponent_eChangeGsType_ChangeSceneInfoPBComponent_eChangeGsType_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool ChangeSceneInfoPBComponent_eChangeGsType_IsValid(int value);
extern const uint32_t ChangeSceneInfoPBComponent_eChangeGsType_internal_data_[];
constexpr ChangeSceneInfoPBComponent_eChangeGsType ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_MIN = static_cast<ChangeSceneInfoPBComponent_eChangeGsType>(0);
constexpr ChangeSceneInfoPBComponent_eChangeGsType ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_MAX = static_cast<ChangeSceneInfoPBComponent_eChangeGsType>(1);
constexpr int ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_ARRAYSIZE = 1 + 1;
const ::google::protobuf::EnumDescriptor*
ChangeSceneInfoPBComponent_eChangeGsType_descriptor();
template <typename T>
const std::string& ChangeSceneInfoPBComponent_eChangeGsType_Name(T value) {
  static_assert(std::is_same<T, ChangeSceneInfoPBComponent_eChangeGsType>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to eChangeGsType_Name().");
  return ChangeSceneInfoPBComponent_eChangeGsType_Name(static_cast<ChangeSceneInfoPBComponent_eChangeGsType>(value));
}
template <>
inline const std::string& ChangeSceneInfoPBComponent_eChangeGsType_Name(ChangeSceneInfoPBComponent_eChangeGsType value) {
  return ::google::protobuf::internal::NameOfDenseEnum<ChangeSceneInfoPBComponent_eChangeGsType_descriptor,
                                                 0, 1>(
      static_cast<int>(value));
}
inline bool ChangeSceneInfoPBComponent_eChangeGsType_Parse(absl::string_view name, ChangeSceneInfoPBComponent_eChangeGsType* value) {
  return ::google::protobuf::internal::ParseNamedEnum<ChangeSceneInfoPBComponent_eChangeGsType>(
      ChangeSceneInfoPBComponent_eChangeGsType_descriptor(), name, value);
}
enum ChangeSceneInfoPBComponent_eChangeSceneState : int {
  ChangeSceneInfoPBComponent_eChangeSceneState_ePendingLeave = 0,
  ChangeSceneInfoPBComponent_eChangeSceneState_eLeaving = 1,
  ChangeSceneInfoPBComponent_eChangeSceneState_eWaitingEnter = 2,
  ChangeSceneInfoPBComponent_eChangeSceneState_eEnterSucceed = 3,
  ChangeSceneInfoPBComponent_eChangeSceneState_eGateEnterSucceed = 4,
  ChangeSceneInfoPBComponent_eChangeSceneState_eDone = 5,
  ChangeSceneInfoPBComponent_eChangeSceneState_ChangeSceneInfoPBComponent_eChangeSceneState_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  ChangeSceneInfoPBComponent_eChangeSceneState_ChangeSceneInfoPBComponent_eChangeSceneState_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool ChangeSceneInfoPBComponent_eChangeSceneState_IsValid(int value);
extern const uint32_t ChangeSceneInfoPBComponent_eChangeSceneState_internal_data_[];
constexpr ChangeSceneInfoPBComponent_eChangeSceneState ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_MIN = static_cast<ChangeSceneInfoPBComponent_eChangeSceneState>(0);
constexpr ChangeSceneInfoPBComponent_eChangeSceneState ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_MAX = static_cast<ChangeSceneInfoPBComponent_eChangeSceneState>(5);
constexpr int ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_ARRAYSIZE = 5 + 1;
const ::google::protobuf::EnumDescriptor*
ChangeSceneInfoPBComponent_eChangeSceneState_descriptor();
template <typename T>
const std::string& ChangeSceneInfoPBComponent_eChangeSceneState_Name(T value) {
  static_assert(std::is_same<T, ChangeSceneInfoPBComponent_eChangeSceneState>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to eChangeSceneState_Name().");
  return ChangeSceneInfoPBComponent_eChangeSceneState_Name(static_cast<ChangeSceneInfoPBComponent_eChangeSceneState>(value));
}
template <>
inline const std::string& ChangeSceneInfoPBComponent_eChangeSceneState_Name(ChangeSceneInfoPBComponent_eChangeSceneState value) {
  return ::google::protobuf::internal::NameOfDenseEnum<ChangeSceneInfoPBComponent_eChangeSceneState_descriptor,
                                                 0, 5>(
      static_cast<int>(value));
}
inline bool ChangeSceneInfoPBComponent_eChangeSceneState_Parse(absl::string_view name, ChangeSceneInfoPBComponent_eChangeSceneState* value) {
  return ::google::protobuf::internal::ParseNamedEnum<ChangeSceneInfoPBComponent_eChangeSceneState>(
      ChangeSceneInfoPBComponent_eChangeSceneState_descriptor(), name, value);
}

// ===================================================================


// -------------------------------------------------------------------

class ChangeSceneInfoPBComponent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ChangeSceneInfoPBComponent) */ {
 public:
  inline ChangeSceneInfoPBComponent() : ChangeSceneInfoPBComponent(nullptr) {}
  ~ChangeSceneInfoPBComponent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(ChangeSceneInfoPBComponent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(ChangeSceneInfoPBComponent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ChangeSceneInfoPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline ChangeSceneInfoPBComponent(const ChangeSceneInfoPBComponent& from) : ChangeSceneInfoPBComponent(nullptr, from) {}
  inline ChangeSceneInfoPBComponent(ChangeSceneInfoPBComponent&& from) noexcept
      : ChangeSceneInfoPBComponent(nullptr, std::move(from)) {}
  inline ChangeSceneInfoPBComponent& operator=(const ChangeSceneInfoPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline ChangeSceneInfoPBComponent& operator=(ChangeSceneInfoPBComponent&& from) noexcept {
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
  static const ChangeSceneInfoPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const ChangeSceneInfoPBComponent* internal_default_instance() {
    return reinterpret_cast<const ChangeSceneInfoPBComponent*>(
        &_ChangeSceneInfoPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(ChangeSceneInfoPBComponent& a, ChangeSceneInfoPBComponent& b) { a.Swap(&b); }
  inline void Swap(ChangeSceneInfoPBComponent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ChangeSceneInfoPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ChangeSceneInfoPBComponent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<ChangeSceneInfoPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ChangeSceneInfoPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ChangeSceneInfoPBComponent& from) { ChangeSceneInfoPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(ChangeSceneInfoPBComponent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "ChangeSceneInfoPBComponent"; }

 protected:
  explicit ChangeSceneInfoPBComponent(::google::protobuf::Arena* arena);
  ChangeSceneInfoPBComponent(::google::protobuf::Arena* arena, const ChangeSceneInfoPBComponent& from);
  ChangeSceneInfoPBComponent(::google::protobuf::Arena* arena, ChangeSceneInfoPBComponent&& from) noexcept
      : ChangeSceneInfoPBComponent(arena) {
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
  using eChangeGsType = ChangeSceneInfoPBComponent_eChangeGsType;
  static constexpr eChangeGsType eSameGs = ChangeSceneInfoPBComponent_eChangeGsType_eSameGs;
  static constexpr eChangeGsType eDifferentGs = ChangeSceneInfoPBComponent_eChangeGsType_eDifferentGs;
  static inline bool eChangeGsType_IsValid(int value) {
    return ChangeSceneInfoPBComponent_eChangeGsType_IsValid(value);
  }
  static constexpr eChangeGsType eChangeGsType_MIN = ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_MIN;
  static constexpr eChangeGsType eChangeGsType_MAX = ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_MAX;
  static constexpr int eChangeGsType_ARRAYSIZE = ChangeSceneInfoPBComponent_eChangeGsType_eChangeGsType_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor* eChangeGsType_descriptor() {
    return ChangeSceneInfoPBComponent_eChangeGsType_descriptor();
  }
  template <typename T>
  static inline const std::string& eChangeGsType_Name(T value) {
    return ChangeSceneInfoPBComponent_eChangeGsType_Name(value);
  }
  static inline bool eChangeGsType_Parse(absl::string_view name, eChangeGsType* value) {
    return ChangeSceneInfoPBComponent_eChangeGsType_Parse(name, value);
  }
  using eChangeSceneState = ChangeSceneInfoPBComponent_eChangeSceneState;
  static constexpr eChangeSceneState ePendingLeave = ChangeSceneInfoPBComponent_eChangeSceneState_ePendingLeave;
  static constexpr eChangeSceneState eLeaving = ChangeSceneInfoPBComponent_eChangeSceneState_eLeaving;
  static constexpr eChangeSceneState eWaitingEnter = ChangeSceneInfoPBComponent_eChangeSceneState_eWaitingEnter;
  static constexpr eChangeSceneState eEnterSucceed = ChangeSceneInfoPBComponent_eChangeSceneState_eEnterSucceed;
  static constexpr eChangeSceneState eGateEnterSucceed = ChangeSceneInfoPBComponent_eChangeSceneState_eGateEnterSucceed;
  static constexpr eChangeSceneState eDone = ChangeSceneInfoPBComponent_eChangeSceneState_eDone;
  static inline bool eChangeSceneState_IsValid(int value) {
    return ChangeSceneInfoPBComponent_eChangeSceneState_IsValid(value);
  }
  static constexpr eChangeSceneState eChangeSceneState_MIN = ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_MIN;
  static constexpr eChangeSceneState eChangeSceneState_MAX = ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_MAX;
  static constexpr int eChangeSceneState_ARRAYSIZE = ChangeSceneInfoPBComponent_eChangeSceneState_eChangeSceneState_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor* eChangeSceneState_descriptor() {
    return ChangeSceneInfoPBComponent_eChangeSceneState_descriptor();
  }
  template <typename T>
  static inline const std::string& eChangeSceneState_Name(T value) {
    return ChangeSceneInfoPBComponent_eChangeSceneState_Name(value);
  }
  static inline bool eChangeSceneState_Parse(absl::string_view name, eChangeSceneState* value) {
    return ChangeSceneInfoPBComponent_eChangeSceneState_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kSceneConfidFieldNumber = 1,
    kGuidFieldNumber = 2,
    kDungenConfidFieldNumber = 3,
    kMirrorConfidFieldNumber = 4,
    kChangeSequenceFieldNumber = 5,
    kChangeGsTypeFieldNumber = 6,
    kChangeTimeFieldNumber = 7,
    kStateFieldNumber = 10,
    kIgnoreFullFieldNumber = 8,
    kProcessingFieldNumber = 9,
    kIsCrossZoneFieldNumber = 13,
    kFromZoneIdFieldNumber = 11,
    kToZoneIdFieldNumber = 12,
  };
  // uint32 scene_confid = 1;
  void clear_scene_confid() ;
  ::uint32_t scene_confid() const;
  void set_scene_confid(::uint32_t value);

  private:
  ::uint32_t _internal_scene_confid() const;
  void _internal_set_scene_confid(::uint32_t value);

  public:
  // uint32 guid = 2;
  void clear_guid() ;
  ::uint32_t guid() const;
  void set_guid(::uint32_t value);

  private:
  ::uint32_t _internal_guid() const;
  void _internal_set_guid(::uint32_t value);

  public:
  // uint32 dungen_confid = 3;
  void clear_dungen_confid() ;
  ::uint32_t dungen_confid() const;
  void set_dungen_confid(::uint32_t value);

  private:
  ::uint32_t _internal_dungen_confid() const;
  void _internal_set_dungen_confid(::uint32_t value);

  public:
  // uint32 mirror_confid = 4;
  void clear_mirror_confid() ;
  ::uint32_t mirror_confid() const;
  void set_mirror_confid(::uint32_t value);

  private:
  ::uint32_t _internal_mirror_confid() const;
  void _internal_set_mirror_confid(::uint32_t value);

  public:
  // uint32 change_sequence = 5;
  void clear_change_sequence() ;
  ::uint32_t change_sequence() const;
  void set_change_sequence(::uint32_t value);

  private:
  ::uint32_t _internal_change_sequence() const;
  void _internal_set_change_sequence(::uint32_t value);

  public:
  // uint32 change_gs_type = 6;
  void clear_change_gs_type() ;
  ::uint32_t change_gs_type() const;
  void set_change_gs_type(::uint32_t value);

  private:
  ::uint32_t _internal_change_gs_type() const;
  void _internal_set_change_gs_type(::uint32_t value);

  public:
  // uint64 change_time = 7;
  void clear_change_time() ;
  ::uint64_t change_time() const;
  void set_change_time(::uint64_t value);

  private:
  ::uint64_t _internal_change_time() const;
  void _internal_set_change_time(::uint64_t value);

  public:
  // uint32 state = 10;
  void clear_state() ;
  ::uint32_t state() const;
  void set_state(::uint32_t value);

  private:
  ::uint32_t _internal_state() const;
  void _internal_set_state(::uint32_t value);

  public:
  // bool ignore_full = 8;
  void clear_ignore_full() ;
  bool ignore_full() const;
  void set_ignore_full(bool value);

  private:
  bool _internal_ignore_full() const;
  void _internal_set_ignore_full(bool value);

  public:
  // bool processing = 9;
  void clear_processing() ;
  bool processing() const;
  void set_processing(bool value);

  private:
  bool _internal_processing() const;
  void _internal_set_processing(bool value);

  public:
  // bool is_cross_zone = 13;
  void clear_is_cross_zone() ;
  bool is_cross_zone() const;
  void set_is_cross_zone(bool value);

  private:
  bool _internal_is_cross_zone() const;
  void _internal_set_is_cross_zone(bool value);

  public:
  // uint32 from_zone_id = 11;
  void clear_from_zone_id() ;
  ::uint32_t from_zone_id() const;
  void set_from_zone_id(::uint32_t value);

  private:
  ::uint32_t _internal_from_zone_id() const;
  void _internal_set_from_zone_id(::uint32_t value);

  public:
  // uint32 to_zone_id = 12;
  void clear_to_zone_id() ;
  ::uint32_t to_zone_id() const;
  void set_to_zone_id(::uint32_t value);

  private:
  ::uint32_t _internal_to_zone_id() const;
  void _internal_set_to_zone_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:ChangeSceneInfoPBComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      4, 13, 0,
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
                          const ChangeSceneInfoPBComponent& from_msg);
    ::uint32_t scene_confid_;
    ::uint32_t guid_;
    ::uint32_t dungen_confid_;
    ::uint32_t mirror_confid_;
    ::uint32_t change_sequence_;
    ::uint32_t change_gs_type_;
    ::uint64_t change_time_;
    ::uint32_t state_;
    bool ignore_full_;
    bool processing_;
    bool is_cross_zone_;
    ::uint32_t from_zone_id_;
    ::uint32_t to_zone_id_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// ChangeSceneInfoPBComponent

// uint32 scene_confid = 1;
inline void ChangeSceneInfoPBComponent::clear_scene_confid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scene_confid_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::scene_confid() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.scene_confid)
  return _internal_scene_confid();
}
inline void ChangeSceneInfoPBComponent::set_scene_confid(::uint32_t value) {
  _internal_set_scene_confid(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.scene_confid)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_scene_confid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.scene_confid_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_scene_confid(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scene_confid_ = value;
}

// uint32 guid = 2;
inline void ChangeSceneInfoPBComponent::clear_guid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.guid_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::guid() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.guid)
  return _internal_guid();
}
inline void ChangeSceneInfoPBComponent::set_guid(::uint32_t value) {
  _internal_set_guid(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.guid)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_guid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.guid_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_guid(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.guid_ = value;
}

// uint32 dungen_confid = 3;
inline void ChangeSceneInfoPBComponent::clear_dungen_confid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.dungen_confid_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::dungen_confid() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.dungen_confid)
  return _internal_dungen_confid();
}
inline void ChangeSceneInfoPBComponent::set_dungen_confid(::uint32_t value) {
  _internal_set_dungen_confid(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.dungen_confid)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_dungen_confid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.dungen_confid_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_dungen_confid(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.dungen_confid_ = value;
}

// uint32 mirror_confid = 4;
inline void ChangeSceneInfoPBComponent::clear_mirror_confid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.mirror_confid_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::mirror_confid() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.mirror_confid)
  return _internal_mirror_confid();
}
inline void ChangeSceneInfoPBComponent::set_mirror_confid(::uint32_t value) {
  _internal_set_mirror_confid(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.mirror_confid)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_mirror_confid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.mirror_confid_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_mirror_confid(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.mirror_confid_ = value;
}

// uint32 change_sequence = 5;
inline void ChangeSceneInfoPBComponent::clear_change_sequence() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_sequence_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::change_sequence() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.change_sequence)
  return _internal_change_sequence();
}
inline void ChangeSceneInfoPBComponent::set_change_sequence(::uint32_t value) {
  _internal_set_change_sequence(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.change_sequence)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_change_sequence() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.change_sequence_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_change_sequence(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_sequence_ = value;
}

// uint32 change_gs_type = 6;
inline void ChangeSceneInfoPBComponent::clear_change_gs_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_gs_type_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::change_gs_type() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.change_gs_type)
  return _internal_change_gs_type();
}
inline void ChangeSceneInfoPBComponent::set_change_gs_type(::uint32_t value) {
  _internal_set_change_gs_type(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.change_gs_type)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_change_gs_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.change_gs_type_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_change_gs_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_gs_type_ = value;
}

// uint64 change_time = 7;
inline void ChangeSceneInfoPBComponent::clear_change_time() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_time_ = ::uint64_t{0u};
}
inline ::uint64_t ChangeSceneInfoPBComponent::change_time() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.change_time)
  return _internal_change_time();
}
inline void ChangeSceneInfoPBComponent::set_change_time(::uint64_t value) {
  _internal_set_change_time(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.change_time)
}
inline ::uint64_t ChangeSceneInfoPBComponent::_internal_change_time() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.change_time_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_change_time(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.change_time_ = value;
}

// bool ignore_full = 8;
inline void ChangeSceneInfoPBComponent::clear_ignore_full() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.ignore_full_ = false;
}
inline bool ChangeSceneInfoPBComponent::ignore_full() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.ignore_full)
  return _internal_ignore_full();
}
inline void ChangeSceneInfoPBComponent::set_ignore_full(bool value) {
  _internal_set_ignore_full(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.ignore_full)
}
inline bool ChangeSceneInfoPBComponent::_internal_ignore_full() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.ignore_full_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_ignore_full(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.ignore_full_ = value;
}

// bool processing = 9;
inline void ChangeSceneInfoPBComponent::clear_processing() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.processing_ = false;
}
inline bool ChangeSceneInfoPBComponent::processing() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.processing)
  return _internal_processing();
}
inline void ChangeSceneInfoPBComponent::set_processing(bool value) {
  _internal_set_processing(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.processing)
}
inline bool ChangeSceneInfoPBComponent::_internal_processing() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.processing_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_processing(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.processing_ = value;
}

// uint32 state = 10;
inline void ChangeSceneInfoPBComponent::clear_state() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::state() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.state)
  return _internal_state();
}
inline void ChangeSceneInfoPBComponent::set_state(::uint32_t value) {
  _internal_set_state(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.state)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_state() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_state(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_ = value;
}

// uint32 from_zone_id = 11;
inline void ChangeSceneInfoPBComponent::clear_from_zone_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.from_zone_id_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::from_zone_id() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.from_zone_id)
  return _internal_from_zone_id();
}
inline void ChangeSceneInfoPBComponent::set_from_zone_id(::uint32_t value) {
  _internal_set_from_zone_id(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.from_zone_id)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_from_zone_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.from_zone_id_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_from_zone_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.from_zone_id_ = value;
}

// uint32 to_zone_id = 12;
inline void ChangeSceneInfoPBComponent::clear_to_zone_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.to_zone_id_ = 0u;
}
inline ::uint32_t ChangeSceneInfoPBComponent::to_zone_id() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.to_zone_id)
  return _internal_to_zone_id();
}
inline void ChangeSceneInfoPBComponent::set_to_zone_id(::uint32_t value) {
  _internal_set_to_zone_id(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.to_zone_id)
}
inline ::uint32_t ChangeSceneInfoPBComponent::_internal_to_zone_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.to_zone_id_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_to_zone_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.to_zone_id_ = value;
}

// bool is_cross_zone = 13;
inline void ChangeSceneInfoPBComponent::clear_is_cross_zone() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.is_cross_zone_ = false;
}
inline bool ChangeSceneInfoPBComponent::is_cross_zone() const {
  // @@protoc_insertion_point(field_get:ChangeSceneInfoPBComponent.is_cross_zone)
  return _internal_is_cross_zone();
}
inline void ChangeSceneInfoPBComponent::set_is_cross_zone(bool value) {
  _internal_set_is_cross_zone(value);
  // @@protoc_insertion_point(field_set:ChangeSceneInfoPBComponent.is_cross_zone)
}
inline bool ChangeSceneInfoPBComponent::_internal_is_cross_zone() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.is_cross_zone_;
}
inline void ChangeSceneInfoPBComponent::_internal_set_is_cross_zone(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.is_cross_zone_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::ChangeSceneInfoPBComponent_eChangeGsType> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::ChangeSceneInfoPBComponent_eChangeGsType>() {
  return ::ChangeSceneInfoPBComponent_eChangeGsType_descriptor();
}
template <>
struct is_proto_enum<::ChangeSceneInfoPBComponent_eChangeSceneState> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::ChangeSceneInfoPBComponent_eChangeSceneState>() {
  return ::ChangeSceneInfoPBComponent_eChangeSceneState_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fcomponent_2fscene_5fcomp_2eproto_2epb_2eh
