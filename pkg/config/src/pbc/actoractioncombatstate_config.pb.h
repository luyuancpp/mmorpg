// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: actoractioncombatstate_config.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef actoractioncombatstate_5fconfig_2eproto_2epb_2eh
#define actoractioncombatstate_5fconfig_2eproto_2epb_2eh

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
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_actoractioncombatstate_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_actoractioncombatstate_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_actoractioncombatstate_5fconfig_2eproto;
}  // extern "C"
class ActorActionCombatStateTable;
struct ActorActionCombatStateTableDefaultTypeInternal;
extern ActorActionCombatStateTableDefaultTypeInternal _ActorActionCombatStateTable_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStateTable_class_data_;
class ActorActionCombatStateTabledData;
struct ActorActionCombatStateTabledDataDefaultTypeInternal;
extern ActorActionCombatStateTabledDataDefaultTypeInternal _ActorActionCombatStateTabledData_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStateTabledData_class_data_;
class ActorActionCombatStatestate;
struct ActorActionCombatStatestateDefaultTypeInternal;
extern ActorActionCombatStatestateDefaultTypeInternal _ActorActionCombatStatestate_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStatestate_class_data_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class ActorActionCombatStatestate final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ActorActionCombatStatestate) */ {
 public:
  inline ActorActionCombatStatestate() : ActorActionCombatStatestate(nullptr) {}
  ~ActorActionCombatStatestate() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(ActorActionCombatStatestate* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(ActorActionCombatStatestate));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ActorActionCombatStatestate(::google::protobuf::internal::ConstantInitialized);

  inline ActorActionCombatStatestate(const ActorActionCombatStatestate& from) : ActorActionCombatStatestate(nullptr, from) {}
  inline ActorActionCombatStatestate(ActorActionCombatStatestate&& from) noexcept
      : ActorActionCombatStatestate(nullptr, std::move(from)) {}
  inline ActorActionCombatStatestate& operator=(const ActorActionCombatStatestate& from) {
    CopyFrom(from);
    return *this;
  }
  inline ActorActionCombatStatestate& operator=(ActorActionCombatStatestate&& from) noexcept {
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
  static const ActorActionCombatStatestate& default_instance() {
    return *reinterpret_cast<const ActorActionCombatStatestate*>(
        &_ActorActionCombatStatestate_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(ActorActionCombatStatestate& a, ActorActionCombatStatestate& b) { a.Swap(&b); }
  inline void Swap(ActorActionCombatStatestate* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ActorActionCombatStatestate* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ActorActionCombatStatestate* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<ActorActionCombatStatestate>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ActorActionCombatStatestate& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ActorActionCombatStatestate& from) { ActorActionCombatStatestate::MergeImpl(*this, from); }

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
  void InternalSwap(ActorActionCombatStatestate* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "ActorActionCombatStatestate"; }

 protected:
  explicit ActorActionCombatStatestate(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  ActorActionCombatStatestate(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const ActorActionCombatStatestate& from);
  ActorActionCombatStatestate(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, ActorActionCombatStatestate&& from) noexcept
      : ActorActionCombatStatestate(arena) {
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
    kStateModeFieldNumber = 1,
    kStateTipFieldNumber = 2,
  };
  // uint32 state_mode = 1;
  void clear_state_mode() ;
  ::uint32_t state_mode() const;
  void set_state_mode(::uint32_t value);

  private:
  ::uint32_t _internal_state_mode() const;
  void _internal_set_state_mode(::uint32_t value);

  public:
  // uint32 state_tip = 2;
  void clear_state_tip() ;
  ::uint32_t state_tip() const;
  void set_state_tip(::uint32_t value);

  private:
  ::uint32_t _internal_state_tip() const;
  void _internal_set_state_tip(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:ActorActionCombatStatestate)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
                                   0, 0,
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
        const ActorActionCombatStatestate& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint32_t state_mode_;
    ::uint32_t state_tip_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_actoractioncombatstate_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStatestate_class_data_;
// -------------------------------------------------------------------

class ActorActionCombatStateTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ActorActionCombatStateTable) */ {
 public:
  inline ActorActionCombatStateTable() : ActorActionCombatStateTable(nullptr) {}
  ~ActorActionCombatStateTable() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(ActorActionCombatStateTable* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(ActorActionCombatStateTable));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ActorActionCombatStateTable(::google::protobuf::internal::ConstantInitialized);

  inline ActorActionCombatStateTable(const ActorActionCombatStateTable& from) : ActorActionCombatStateTable(nullptr, from) {}
  inline ActorActionCombatStateTable(ActorActionCombatStateTable&& from) noexcept
      : ActorActionCombatStateTable(nullptr, std::move(from)) {}
  inline ActorActionCombatStateTable& operator=(const ActorActionCombatStateTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline ActorActionCombatStateTable& operator=(ActorActionCombatStateTable&& from) noexcept {
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
  static const ActorActionCombatStateTable& default_instance() {
    return *reinterpret_cast<const ActorActionCombatStateTable*>(
        &_ActorActionCombatStateTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(ActorActionCombatStateTable& a, ActorActionCombatStateTable& b) { a.Swap(&b); }
  inline void Swap(ActorActionCombatStateTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ActorActionCombatStateTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ActorActionCombatStateTable* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<ActorActionCombatStateTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ActorActionCombatStateTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ActorActionCombatStateTable& from) { ActorActionCombatStateTable::MergeImpl(*this, from); }

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
  void InternalSwap(ActorActionCombatStateTable* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "ActorActionCombatStateTable"; }

 protected:
  explicit ActorActionCombatStateTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  ActorActionCombatStateTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const ActorActionCombatStateTable& from);
  ActorActionCombatStateTable(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, ActorActionCombatStateTable&& from) noexcept
      : ActorActionCombatStateTable(arena) {
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
    kStateFieldNumber = 2,
    kIdFieldNumber = 1,
  };
  // repeated .ActorActionCombatStatestate state = 2;
  int state_size() const;
  private:
  int _internal_state_size() const;

  public:
  void clear_state() ;
  ::ActorActionCombatStatestate* PROTOBUF_NONNULL mutable_state(int index);
  ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>* PROTOBUF_NONNULL mutable_state();

  private:
  const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>& _internal_state() const;
  ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>* PROTOBUF_NONNULL _internal_mutable_state();
  public:
  const ::ActorActionCombatStatestate& state(int index) const;
  ::ActorActionCombatStatestate* PROTOBUF_NONNULL add_state();
  const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>& state() const;
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:ActorActionCombatStateTable)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
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
        const ActorActionCombatStateTable& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::ActorActionCombatStatestate > state_;
    ::uint32_t id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_actoractioncombatstate_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStateTable_class_data_;
// -------------------------------------------------------------------

class ActorActionCombatStateTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ActorActionCombatStateTabledData) */ {
 public:
  inline ActorActionCombatStateTabledData() : ActorActionCombatStateTabledData(nullptr) {}
  ~ActorActionCombatStateTabledData() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(ActorActionCombatStateTabledData* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(ActorActionCombatStateTabledData));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ActorActionCombatStateTabledData(::google::protobuf::internal::ConstantInitialized);

  inline ActorActionCombatStateTabledData(const ActorActionCombatStateTabledData& from) : ActorActionCombatStateTabledData(nullptr, from) {}
  inline ActorActionCombatStateTabledData(ActorActionCombatStateTabledData&& from) noexcept
      : ActorActionCombatStateTabledData(nullptr, std::move(from)) {}
  inline ActorActionCombatStateTabledData& operator=(const ActorActionCombatStateTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline ActorActionCombatStateTabledData& operator=(ActorActionCombatStateTabledData&& from) noexcept {
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
  static const ActorActionCombatStateTabledData& default_instance() {
    return *reinterpret_cast<const ActorActionCombatStateTabledData*>(
        &_ActorActionCombatStateTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(ActorActionCombatStateTabledData& a, ActorActionCombatStateTabledData& b) { a.Swap(&b); }
  inline void Swap(ActorActionCombatStateTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ActorActionCombatStateTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ActorActionCombatStateTabledData* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<ActorActionCombatStateTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ActorActionCombatStateTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ActorActionCombatStateTabledData& from) { ActorActionCombatStateTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(ActorActionCombatStateTabledData* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "ActorActionCombatStateTabledData"; }

 protected:
  explicit ActorActionCombatStateTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  ActorActionCombatStateTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const ActorActionCombatStateTabledData& from);
  ActorActionCombatStateTabledData(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, ActorActionCombatStateTabledData&& from) noexcept
      : ActorActionCombatStateTabledData(arena) {
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
    kDataFieldNumber = 1,
  };
  // repeated .ActorActionCombatStateTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::ActorActionCombatStateTable* PROTOBUF_NONNULL mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>* PROTOBUF_NONNULL mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>* PROTOBUF_NONNULL _internal_mutable_data();
  public:
  const ::ActorActionCombatStateTable& data(int index) const;
  ::ActorActionCombatStateTable* PROTOBUF_NONNULL add_data();
  const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>& data() const;
  // @@protoc_insertion_point(class_scope:ActorActionCombatStateTabledData)
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
        const ActorActionCombatStateTabledData& from_msg);
    ::google::protobuf::RepeatedPtrField< ::ActorActionCombatStateTable > data_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_actoractioncombatstate_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull ActorActionCombatStateTabledData_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// ActorActionCombatStatestate

// uint32 state_mode = 1;
inline void ActorActionCombatStatestate::clear_state_mode() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_mode_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t ActorActionCombatStatestate::state_mode() const {
  // @@protoc_insertion_point(field_get:ActorActionCombatStatestate.state_mode)
  return _internal_state_mode();
}
inline void ActorActionCombatStatestate::set_state_mode(::uint32_t value) {
  _internal_set_state_mode(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:ActorActionCombatStatestate.state_mode)
}
inline ::uint32_t ActorActionCombatStatestate::_internal_state_mode() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_mode_;
}
inline void ActorActionCombatStatestate::_internal_set_state_mode(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_mode_ = value;
}

// uint32 state_tip = 2;
inline void ActorActionCombatStatestate::clear_state_tip() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_tip_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline ::uint32_t ActorActionCombatStatestate::state_tip() const {
  // @@protoc_insertion_point(field_get:ActorActionCombatStatestate.state_tip)
  return _internal_state_tip();
}
inline void ActorActionCombatStatestate::set_state_tip(::uint32_t value) {
  _internal_set_state_tip(value);
  _impl_._has_bits_[0] |= 0x00000002u;
  // @@protoc_insertion_point(field_set:ActorActionCombatStatestate.state_tip)
}
inline ::uint32_t ActorActionCombatStatestate::_internal_state_tip() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_tip_;
}
inline void ActorActionCombatStatestate::_internal_set_state_tip(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_tip_ = value;
}

// -------------------------------------------------------------------

// ActorActionCombatStateTable

// uint32 id = 1;
inline void ActorActionCombatStateTable::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t ActorActionCombatStateTable::id() const {
  // @@protoc_insertion_point(field_get:ActorActionCombatStateTable.id)
  return _internal_id();
}
inline void ActorActionCombatStateTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:ActorActionCombatStateTable.id)
}
inline ::uint32_t ActorActionCombatStateTable::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void ActorActionCombatStateTable::_internal_set_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// repeated .ActorActionCombatStatestate state = 2;
inline int ActorActionCombatStateTable::_internal_state_size() const {
  return _internal_state().size();
}
inline int ActorActionCombatStateTable::state_size() const {
  return _internal_state_size();
}
inline void ActorActionCombatStateTable::clear_state() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_.Clear();
}
inline ::ActorActionCombatStatestate* PROTOBUF_NONNULL ActorActionCombatStateTable::mutable_state(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:ActorActionCombatStateTable.state)
  return _internal_mutable_state()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>* PROTOBUF_NONNULL ActorActionCombatStateTable::mutable_state()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:ActorActionCombatStateTable.state)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_state();
}
inline const ::ActorActionCombatStatestate& ActorActionCombatStateTable::state(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:ActorActionCombatStateTable.state)
  return _internal_state().Get(index);
}
inline ::ActorActionCombatStatestate* PROTOBUF_NONNULL ActorActionCombatStateTable::add_state()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::ActorActionCombatStatestate* _add = _internal_mutable_state()->Add();
  // @@protoc_insertion_point(field_add:ActorActionCombatStateTable.state)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>& ActorActionCombatStateTable::state() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:ActorActionCombatStateTable.state)
  return _internal_state();
}
inline const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>&
ActorActionCombatStateTable::_internal_state() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_;
}
inline ::google::protobuf::RepeatedPtrField<::ActorActionCombatStatestate>* PROTOBUF_NONNULL
ActorActionCombatStateTable::_internal_mutable_state() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.state_;
}

// -------------------------------------------------------------------

// ActorActionCombatStateTabledData

// repeated .ActorActionCombatStateTable data = 1;
inline int ActorActionCombatStateTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int ActorActionCombatStateTabledData::data_size() const {
  return _internal_data_size();
}
inline void ActorActionCombatStateTabledData::clear_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Clear();
}
inline ::ActorActionCombatStateTable* PROTOBUF_NONNULL ActorActionCombatStateTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:ActorActionCombatStateTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>* PROTOBUF_NONNULL ActorActionCombatStateTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:ActorActionCombatStateTabledData.data)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_data();
}
inline const ::ActorActionCombatStateTable& ActorActionCombatStateTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:ActorActionCombatStateTabledData.data)
  return _internal_data().Get(index);
}
inline ::ActorActionCombatStateTable* PROTOBUF_NONNULL ActorActionCombatStateTabledData::add_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::ActorActionCombatStateTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:ActorActionCombatStateTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>& ActorActionCombatStateTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:ActorActionCombatStateTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>&
ActorActionCombatStateTabledData::_internal_data() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::ActorActionCombatStateTable>* PROTOBUF_NONNULL
ActorActionCombatStateTabledData::_internal_mutable_data() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // actoractioncombatstate_5fconfig_2eproto_2epb_2eh
