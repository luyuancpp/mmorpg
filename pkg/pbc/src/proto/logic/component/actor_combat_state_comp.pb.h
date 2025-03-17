// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/actor_combat_state_comp.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto_2epb_2eh
#define proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto_2epb_2eh

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
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/map.h"  // IWYU pragma: export
#include "google/protobuf/map_type_handler.h"  // IWYU pragma: export
#include "google/protobuf/map_entry.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/unknown_field_set.h"
#include "proto/common/comp.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto;
}  // extern "C"
enum eCombatState : int;
extern const uint32_t eCombatState_internal_data_[];
class CombatStateCollectionPbComponent;
struct CombatStateCollectionPbComponentDefaultTypeInternal;
extern CombatStateCollectionPbComponentDefaultTypeInternal _CombatStateCollectionPbComponent_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CombatStateCollectionPbComponent_class_data_;
class CombatStateCollectionPbComponent_StatesEntry_DoNotUse;
struct CombatStateCollectionPbComponent_StatesEntry_DoNotUseDefaultTypeInternal;
extern CombatStateCollectionPbComponent_StatesEntry_DoNotUseDefaultTypeInternal _CombatStateCollectionPbComponent_StatesEntry_DoNotUse_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CombatStateCollectionPbComponent_StatesEntry_DoNotUse_class_data_;
class CombatStateDetailsPbComponent;
struct CombatStateDetailsPbComponentDefaultTypeInternal;
extern CombatStateDetailsPbComponentDefaultTypeInternal _CombatStateDetailsPbComponent_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CombatStateDetailsPbComponent_class_data_;
class CombatStateDetailsPbComponent_SourcesEntry_DoNotUse;
struct CombatStateDetailsPbComponent_SourcesEntry_DoNotUseDefaultTypeInternal;
extern CombatStateDetailsPbComponent_SourcesEntry_DoNotUseDefaultTypeInternal _CombatStateDetailsPbComponent_SourcesEntry_DoNotUse_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CombatStateDetailsPbComponent_SourcesEntry_DoNotUse_class_data_;
namespace google {
namespace protobuf {
template <>
internal::EnumTraitsT<::eCombatState_internal_data_>
    internal::EnumTraitsImpl::value<::eCombatState>;
}  // namespace protobuf
}  // namespace google

enum eCombatState : int {
  kSilence = 0,
  eCombatState_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  eCombatState_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

extern const uint32_t eCombatState_internal_data_[];
inline constexpr eCombatState eCombatState_MIN =
    static_cast<eCombatState>(0);
inline constexpr eCombatState eCombatState_MAX =
    static_cast<eCombatState>(0);
inline bool eCombatState_IsValid(int value) {
  return 0 <= value && value <= 0;
}
inline constexpr int eCombatState_ARRAYSIZE = 0 + 1;
const ::google::protobuf::EnumDescriptor* PROTOBUF_NONNULL eCombatState_descriptor();
template <typename T>
const std::string& eCombatState_Name(T value) {
  static_assert(std::is_same<T, eCombatState>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to eCombatState_Name().");
  return eCombatState_Name(static_cast<eCombatState>(value));
}
template <>
inline const std::string& eCombatState_Name(eCombatState value) {
  return ::google::protobuf::internal::NameOfDenseEnum<eCombatState_descriptor, 0, 0>(
      static_cast<int>(value));
}
inline bool eCombatState_Parse(
    absl::string_view name, eCombatState* PROTOBUF_NONNULL value) {
  return ::google::protobuf::internal::ParseNamedEnum<eCombatState>(eCombatState_descriptor(), name,
                                           value);
}

// ===================================================================


// -------------------------------------------------------------------

class CombatStateDetailsPbComponent_SourcesEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<::uint64_t, bool,
                             ::google::protobuf::internal::WireFormatLite::TYPE_UINT64,
                             ::google::protobuf::internal::WireFormatLite::TYPE_BOOL> {
 public:
  using SuperType =
      ::google::protobuf::internal::MapEntry<::uint64_t, bool,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT64,
                      ::google::protobuf::internal::WireFormatLite::TYPE_BOOL>;
  CombatStateDetailsPbComponent_SourcesEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateDetailsPbComponent_SourcesEntry_DoNotUse(::google::protobuf::internal::ConstantInitialized);
  explicit CombatStateDetailsPbComponent_SourcesEntry_DoNotUse(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr const void* PROTOBUF_NONNULL internal_default_instance() {
    return &_CombatStateDetailsPbComponent_SourcesEntry_DoNotUse_default_instance_;
  }


  static constexpr auto InternalGenerateClassData_();

 private:
  friend class ::google::protobuf::MessageLite;
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto;

  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
                                   0, 0,
                                   2>
      _table_;

  const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GetClassData() const PROTOBUF_FINAL;
  static void* PROTOBUF_NONNULL PlacementNew_(
      const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr auto InternalNewImpl_();
};
extern const ::google::protobuf::internal::ClassDataFull CombatStateDetailsPbComponent_SourcesEntry_DoNotUse_class_data_;
// -------------------------------------------------------------------

class CombatStateDetailsPbComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CombatStateDetailsPbComponent) */ {
 public:
  inline CombatStateDetailsPbComponent() : CombatStateDetailsPbComponent(nullptr) {}
  ~CombatStateDetailsPbComponent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CombatStateDetailsPbComponent* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CombatStateDetailsPbComponent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateDetailsPbComponent(::google::protobuf::internal::ConstantInitialized);

  inline CombatStateDetailsPbComponent(const CombatStateDetailsPbComponent& from) : CombatStateDetailsPbComponent(nullptr, from) {}
  inline CombatStateDetailsPbComponent(CombatStateDetailsPbComponent&& from) noexcept
      : CombatStateDetailsPbComponent(nullptr, std::move(from)) {}
  inline CombatStateDetailsPbComponent& operator=(const CombatStateDetailsPbComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline CombatStateDetailsPbComponent& operator=(CombatStateDetailsPbComponent&& from) noexcept {
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
  inline ::google::protobuf::UnknownFieldSet* PROTOBUF_NONNULL mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* PROTOBUF_NONNULL GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const CombatStateDetailsPbComponent& default_instance() {
    return *reinterpret_cast<const CombatStateDetailsPbComponent*>(
        &_CombatStateDetailsPbComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(CombatStateDetailsPbComponent& a, CombatStateDetailsPbComponent& b) { a.Swap(&b); }
  inline void Swap(CombatStateDetailsPbComponent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CombatStateDetailsPbComponent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CombatStateDetailsPbComponent* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CombatStateDetailsPbComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CombatStateDetailsPbComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CombatStateDetailsPbComponent& from) { CombatStateDetailsPbComponent::MergeImpl(*this, from); }

  private:
  static void MergeImpl(::google::protobuf::MessageLite& to_msg,
                        const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      const ::google::protobuf::MessageLite& msg, ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(CombatStateDetailsPbComponent* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CombatStateDetailsPbComponent"; }

 protected:
  explicit CombatStateDetailsPbComponent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  CombatStateDetailsPbComponent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const CombatStateDetailsPbComponent& from);
  CombatStateDetailsPbComponent(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, CombatStateDetailsPbComponent&& from) noexcept
      : CombatStateDetailsPbComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GetClassData() const PROTOBUF_FINAL;
  static void* PROTOBUF_NONNULL PlacementNew_(
      const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr auto InternalNewImpl_();

 public:
  static constexpr auto InternalGenerateClassData_();

  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSourcesFieldNumber = 1,
  };
  // map<uint64, bool> sources = 1;
  int sources_size() const;
  private:
  int _internal_sources_size() const;

  public:
  void clear_sources() ;
  const ::google::protobuf::Map<::uint64_t, bool>& sources() const;
  ::google::protobuf::Map<::uint64_t, bool>* PROTOBUF_NONNULL mutable_sources();

  private:
  const ::google::protobuf::Map<::uint64_t, bool>& _internal_sources() const;
  ::google::protobuf::Map<::uint64_t, bool>* PROTOBUF_NONNULL _internal_mutable_sources();

  public:
  // @@protoc_insertion_point(class_scope:CombatStateDetailsPbComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<0, 1,
                                   1, 0,
                                   2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const Impl_& from,
        const CombatStateDetailsPbComponent& from_msg);
    ::google::protobuf::internal::MapField<CombatStateDetailsPbComponent_SourcesEntry_DoNotUse, ::uint64_t, bool,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT64,
                      ::google::protobuf::internal::WireFormatLite::TYPE_BOOL>
        sources_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull CombatStateDetailsPbComponent_class_data_;
// -------------------------------------------------------------------

class CombatStateCollectionPbComponent_StatesEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<::uint32_t, ::google::protobuf::Message,
                             ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
                             ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE> {
 public:
  using SuperType =
      ::google::protobuf::internal::MapEntry<::uint32_t, ::google::protobuf::Message,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
                      ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE>;
  CombatStateCollectionPbComponent_StatesEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateCollectionPbComponent_StatesEntry_DoNotUse(::google::protobuf::internal::ConstantInitialized);
  explicit CombatStateCollectionPbComponent_StatesEntry_DoNotUse(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr const void* PROTOBUF_NONNULL internal_default_instance() {
    return &_CombatStateCollectionPbComponent_StatesEntry_DoNotUse_default_instance_;
  }


  static constexpr auto InternalGenerateClassData_();

 private:
  friend class ::google::protobuf::MessageLite;
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto;

  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
                                   1, 0,
                                   2>
      _table_;

  const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GetClassData() const PROTOBUF_FINAL;
  static void* PROTOBUF_NONNULL PlacementNew_(
      const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr auto InternalNewImpl_();
};
extern const ::google::protobuf::internal::ClassDataFull CombatStateCollectionPbComponent_StatesEntry_DoNotUse_class_data_;
// -------------------------------------------------------------------

class CombatStateCollectionPbComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CombatStateCollectionPbComponent) */ {
 public:
  inline CombatStateCollectionPbComponent() : CombatStateCollectionPbComponent(nullptr) {}
  ~CombatStateCollectionPbComponent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CombatStateCollectionPbComponent* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CombatStateCollectionPbComponent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateCollectionPbComponent(::google::protobuf::internal::ConstantInitialized);

  inline CombatStateCollectionPbComponent(const CombatStateCollectionPbComponent& from) : CombatStateCollectionPbComponent(nullptr, from) {}
  inline CombatStateCollectionPbComponent(CombatStateCollectionPbComponent&& from) noexcept
      : CombatStateCollectionPbComponent(nullptr, std::move(from)) {}
  inline CombatStateCollectionPbComponent& operator=(const CombatStateCollectionPbComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline CombatStateCollectionPbComponent& operator=(CombatStateCollectionPbComponent&& from) noexcept {
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
  inline ::google::protobuf::UnknownFieldSet* PROTOBUF_NONNULL mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* PROTOBUF_NONNULL GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const CombatStateCollectionPbComponent& default_instance() {
    return *reinterpret_cast<const CombatStateCollectionPbComponent*>(
        &_CombatStateCollectionPbComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(CombatStateCollectionPbComponent& a, CombatStateCollectionPbComponent& b) { a.Swap(&b); }
  inline void Swap(CombatStateCollectionPbComponent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CombatStateCollectionPbComponent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CombatStateCollectionPbComponent* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CombatStateCollectionPbComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CombatStateCollectionPbComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CombatStateCollectionPbComponent& from) { CombatStateCollectionPbComponent::MergeImpl(*this, from); }

  private:
  static void MergeImpl(::google::protobuf::MessageLite& to_msg,
                        const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      const ::google::protobuf::MessageLite& msg, ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(CombatStateCollectionPbComponent* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CombatStateCollectionPbComponent"; }

 protected:
  explicit CombatStateCollectionPbComponent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  CombatStateCollectionPbComponent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const CombatStateCollectionPbComponent& from);
  CombatStateCollectionPbComponent(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, CombatStateCollectionPbComponent&& from) noexcept
      : CombatStateCollectionPbComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GetClassData() const PROTOBUF_FINAL;
  static void* PROTOBUF_NONNULL PlacementNew_(
      const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr auto InternalNewImpl_();

 public:
  static constexpr auto InternalGenerateClassData_();

  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kStatesFieldNumber = 1,
  };
  // map<uint32, .CombatStateDetailsPbComponent> states = 1;
  int states_size() const;
  private:
  int _internal_states_size() const;

  public:
  void clear_states() ;
  const ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>& states() const;
  ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>* PROTOBUF_NONNULL mutable_states();

  private:
  const ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>& _internal_states() const;
  ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>* PROTOBUF_NONNULL _internal_mutable_states();

  public:
  // @@protoc_insertion_point(class_scope:CombatStateCollectionPbComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<0, 1,
                                   2, 0,
                                   2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const Impl_& from,
        const CombatStateCollectionPbComponent& from_msg);
    ::google::protobuf::internal::MapField<CombatStateCollectionPbComponent_StatesEntry_DoNotUse, ::uint32_t, ::CombatStateDetailsPbComponent,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
                      ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE>
        states_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull CombatStateCollectionPbComponent_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// -------------------------------------------------------------------

// CombatStateDetailsPbComponent

// map<uint64, bool> sources = 1;
inline int CombatStateDetailsPbComponent::_internal_sources_size() const {
  return _internal_sources().size();
}
inline int CombatStateDetailsPbComponent::sources_size() const {
  return _internal_sources_size();
}
inline void CombatStateDetailsPbComponent::clear_sources() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sources_.Clear();
}
inline const ::google::protobuf::Map<::uint64_t, bool>& CombatStateDetailsPbComponent::_internal_sources() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sources_.GetMap();
}
inline const ::google::protobuf::Map<::uint64_t, bool>& CombatStateDetailsPbComponent::sources() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:CombatStateDetailsPbComponent.sources)
  return _internal_sources();
}
inline ::google::protobuf::Map<::uint64_t, bool>* PROTOBUF_NONNULL CombatStateDetailsPbComponent::_internal_mutable_sources() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.sources_.MutableMap();
}
inline ::google::protobuf::Map<::uint64_t, bool>* PROTOBUF_NONNULL CombatStateDetailsPbComponent::mutable_sources()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:CombatStateDetailsPbComponent.sources)
  return _internal_mutable_sources();
}

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// CombatStateCollectionPbComponent

// map<uint32, .CombatStateDetailsPbComponent> states = 1;
inline int CombatStateCollectionPbComponent::_internal_states_size() const {
  return _internal_states().size();
}
inline int CombatStateCollectionPbComponent::states_size() const {
  return _internal_states_size();
}
inline void CombatStateCollectionPbComponent::clear_states() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.states_.Clear();
}
inline const ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>& CombatStateCollectionPbComponent::_internal_states() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.states_.GetMap();
}
inline const ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>& CombatStateCollectionPbComponent::states() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:CombatStateCollectionPbComponent.states)
  return _internal_states();
}
inline ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>* PROTOBUF_NONNULL CombatStateCollectionPbComponent::_internal_mutable_states() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.states_.MutableMap();
}
inline ::google::protobuf::Map<::uint32_t, ::CombatStateDetailsPbComponent>* PROTOBUF_NONNULL CombatStateCollectionPbComponent::mutable_states()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:CombatStateCollectionPbComponent.states)
  return _internal_mutable_states();
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::eCombatState> : std::true_type {};
template <>
inline const EnumDescriptor* PROTOBUF_NONNULL GetEnumDescriptor<::eCombatState>() {
  return ::eCombatState_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fcomponent_2factor_5fcombat_5fstate_5fcomp_2eproto_2epb_2eh
