// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/component/actor_status_comp.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto_2epb_2eh

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
#include "google/protobuf/message.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/unknown_field_set.h"
#include "common/comp.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto;
class CalculatedAttributesPBComponent;
struct CalculatedAttributesPBComponentDefaultTypeInternal;
extern CalculatedAttributesPBComponentDefaultTypeInternal _CalculatedAttributesPBComponent_default_instance_;
class DerivedAttributesPBComponent;
struct DerivedAttributesPBComponentDefaultTypeInternal;
extern DerivedAttributesPBComponentDefaultTypeInternal _DerivedAttributesPBComponent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class DerivedAttributesPBComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:DerivedAttributesPBComponent) */ {
 public:
  inline DerivedAttributesPBComponent() : DerivedAttributesPBComponent(nullptr) {}
  ~DerivedAttributesPBComponent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DerivedAttributesPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline DerivedAttributesPBComponent(const DerivedAttributesPBComponent& from) : DerivedAttributesPBComponent(nullptr, from) {}
  inline DerivedAttributesPBComponent(DerivedAttributesPBComponent&& from) noexcept
      : DerivedAttributesPBComponent(nullptr, std::move(from)) {}
  inline DerivedAttributesPBComponent& operator=(const DerivedAttributesPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline DerivedAttributesPBComponent& operator=(DerivedAttributesPBComponent&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
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
  static const DerivedAttributesPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const DerivedAttributesPBComponent* internal_default_instance() {
    return reinterpret_cast<const DerivedAttributesPBComponent*>(
        &_DerivedAttributesPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(DerivedAttributesPBComponent& a, DerivedAttributesPBComponent& b) { a.Swap(&b); }
  inline void Swap(DerivedAttributesPBComponent* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(DerivedAttributesPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DerivedAttributesPBComponent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<DerivedAttributesPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const DerivedAttributesPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const DerivedAttributesPBComponent& from) { DerivedAttributesPBComponent::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  ::size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::google::protobuf::internal::ParseContext* ctx) final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(DerivedAttributesPBComponent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "DerivedAttributesPBComponent"; }

 protected:
  explicit DerivedAttributesPBComponent(::google::protobuf::Arena* arena);
  DerivedAttributesPBComponent(::google::protobuf::Arena* arena, const DerivedAttributesPBComponent& from);
  DerivedAttributesPBComponent(::google::protobuf::Arena* arena, DerivedAttributesPBComponent&& from) noexcept
      : DerivedAttributesPBComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kMaxHealthFieldNumber = 1,
  };
  // uint64 max_health = 1;
  void clear_max_health() ;
  ::uint64_t max_health() const;
  void set_max_health(::uint64_t value);

  private:
  ::uint64_t _internal_max_health() const;
  void _internal_set_max_health(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:DerivedAttributesPBComponent)
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
                          ::google::protobuf::Arena* arena, const Impl_& from);
    ::uint64_t max_health_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto;
};
// -------------------------------------------------------------------

class CalculatedAttributesPBComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CalculatedAttributesPBComponent) */ {
 public:
  inline CalculatedAttributesPBComponent() : CalculatedAttributesPBComponent(nullptr) {}
  ~CalculatedAttributesPBComponent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CalculatedAttributesPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline CalculatedAttributesPBComponent(const CalculatedAttributesPBComponent& from) : CalculatedAttributesPBComponent(nullptr, from) {}
  inline CalculatedAttributesPBComponent(CalculatedAttributesPBComponent&& from) noexcept
      : CalculatedAttributesPBComponent(nullptr, std::move(from)) {}
  inline CalculatedAttributesPBComponent& operator=(const CalculatedAttributesPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline CalculatedAttributesPBComponent& operator=(CalculatedAttributesPBComponent&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
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
  static const CalculatedAttributesPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const CalculatedAttributesPBComponent* internal_default_instance() {
    return reinterpret_cast<const CalculatedAttributesPBComponent*>(
        &_CalculatedAttributesPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(CalculatedAttributesPBComponent& a, CalculatedAttributesPBComponent& b) { a.Swap(&b); }
  inline void Swap(CalculatedAttributesPBComponent* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CalculatedAttributesPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CalculatedAttributesPBComponent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<CalculatedAttributesPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CalculatedAttributesPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CalculatedAttributesPBComponent& from) { CalculatedAttributesPBComponent::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  ::size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::google::protobuf::internal::ParseContext* ctx) final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(CalculatedAttributesPBComponent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "CalculatedAttributesPBComponent"; }

 protected:
  explicit CalculatedAttributesPBComponent(::google::protobuf::Arena* arena);
  CalculatedAttributesPBComponent(::google::protobuf::Arena* arena, const CalculatedAttributesPBComponent& from);
  CalculatedAttributesPBComponent(::google::protobuf::Arena* arena, CalculatedAttributesPBComponent&& from) noexcept
      : CalculatedAttributesPBComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kAttackPowerFieldNumber = 1,
    kDefensePowerFieldNumber = 2,
    kIsDeadFieldNumber = 3,
  };
  // uint64 attack_power = 1;
  void clear_attack_power() ;
  ::uint64_t attack_power() const;
  void set_attack_power(::uint64_t value);

  private:
  ::uint64_t _internal_attack_power() const;
  void _internal_set_attack_power(::uint64_t value);

  public:
  // uint64 defense_power = 2;
  void clear_defense_power() ;
  ::uint64_t defense_power() const;
  void set_defense_power(::uint64_t value);

  private:
  ::uint64_t _internal_defense_power() const;
  void _internal_set_defense_power(::uint64_t value);

  public:
  // bool isDead = 3;
  void clear_isdead() ;
  bool isdead() const;
  void set_isdead(bool value);

  private:
  bool _internal_isdead() const;
  void _internal_set_isdead(bool value);

  public:
  // @@protoc_insertion_point(class_scope:CalculatedAttributesPBComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
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
                          ::google::protobuf::Arena* arena, const Impl_& from);
    ::uint64_t attack_power_;
    ::uint64_t defense_power_;
    bool isdead_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// CalculatedAttributesPBComponent

// uint64 attack_power = 1;
inline void CalculatedAttributesPBComponent::clear_attack_power() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.attack_power_ = ::uint64_t{0u};
}
inline ::uint64_t CalculatedAttributesPBComponent::attack_power() const {
  // @@protoc_insertion_point(field_get:CalculatedAttributesPBComponent.attack_power)
  return _internal_attack_power();
}
inline void CalculatedAttributesPBComponent::set_attack_power(::uint64_t value) {
  _internal_set_attack_power(value);
  // @@protoc_insertion_point(field_set:CalculatedAttributesPBComponent.attack_power)
}
inline ::uint64_t CalculatedAttributesPBComponent::_internal_attack_power() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.attack_power_;
}
inline void CalculatedAttributesPBComponent::_internal_set_attack_power(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.attack_power_ = value;
}

// uint64 defense_power = 2;
inline void CalculatedAttributesPBComponent::clear_defense_power() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.defense_power_ = ::uint64_t{0u};
}
inline ::uint64_t CalculatedAttributesPBComponent::defense_power() const {
  // @@protoc_insertion_point(field_get:CalculatedAttributesPBComponent.defense_power)
  return _internal_defense_power();
}
inline void CalculatedAttributesPBComponent::set_defense_power(::uint64_t value) {
  _internal_set_defense_power(value);
  // @@protoc_insertion_point(field_set:CalculatedAttributesPBComponent.defense_power)
}
inline ::uint64_t CalculatedAttributesPBComponent::_internal_defense_power() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.defense_power_;
}
inline void CalculatedAttributesPBComponent::_internal_set_defense_power(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.defense_power_ = value;
}

// bool isDead = 3;
inline void CalculatedAttributesPBComponent::clear_isdead() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.isdead_ = false;
}
inline bool CalculatedAttributesPBComponent::isdead() const {
  // @@protoc_insertion_point(field_get:CalculatedAttributesPBComponent.isDead)
  return _internal_isdead();
}
inline void CalculatedAttributesPBComponent::set_isdead(bool value) {
  _internal_set_isdead(value);
  // @@protoc_insertion_point(field_set:CalculatedAttributesPBComponent.isDead)
}
inline bool CalculatedAttributesPBComponent::_internal_isdead() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.isdead_;
}
inline void CalculatedAttributesPBComponent::_internal_set_isdead(bool value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.isdead_ = value;
}

// -------------------------------------------------------------------

// DerivedAttributesPBComponent

// uint64 max_health = 1;
inline void DerivedAttributesPBComponent::clear_max_health() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.max_health_ = ::uint64_t{0u};
}
inline ::uint64_t DerivedAttributesPBComponent::max_health() const {
  // @@protoc_insertion_point(field_get:DerivedAttributesPBComponent.max_health)
  return _internal_max_health();
}
inline void DerivedAttributesPBComponent::set_max_health(::uint64_t value) {
  _internal_set_max_health(value);
  // @@protoc_insertion_point(field_set:DerivedAttributesPBComponent.max_health)
}
inline ::uint64_t DerivedAttributesPBComponent::_internal_max_health() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.max_health_;
}
inline void DerivedAttributesPBComponent::_internal_set_max_health(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.max_health_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2factor_5fstatus_5fcomp_2eproto_2epb_2eh
