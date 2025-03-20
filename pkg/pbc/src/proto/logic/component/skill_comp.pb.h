// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/skill_comp.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto_2epb_2eh
#define proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto_2epb_2eh

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
#include "google/protobuf/map.h"  // IWYU pragma: export
#include "google/protobuf/map_entry.h"
#include "google/protobuf/map_field_inl.h"
#include "google/protobuf/unknown_field_set.h"
#include "proto/logic/component/actor_comp.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto;
class SkillContextPBComponent;
struct SkillContextPBComponentDefaultTypeInternal;
extern SkillContextPBComponentDefaultTypeInternal _SkillContextPBComponent_default_instance_;
class SkillContextPBComponent_AdditionalDataEntry_DoNotUse;
struct SkillContextPBComponent_AdditionalDataEntry_DoNotUseDefaultTypeInternal;
extern SkillContextPBComponent_AdditionalDataEntry_DoNotUseDefaultTypeInternal _SkillContextPBComponent_AdditionalDataEntry_DoNotUse_default_instance_;
class SkillPBComponent;
struct SkillPBComponentDefaultTypeInternal;
extern SkillPBComponentDefaultTypeInternal _SkillPBComponent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class SkillPBComponent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SkillPBComponent) */ {
 public:
  inline SkillPBComponent() : SkillPBComponent(nullptr) {}
  ~SkillPBComponent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(SkillPBComponent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(SkillPBComponent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline SkillPBComponent(const SkillPBComponent& from) : SkillPBComponent(nullptr, from) {}
  inline SkillPBComponent(SkillPBComponent&& from) noexcept
      : SkillPBComponent(nullptr, std::move(from)) {}
  inline SkillPBComponent& operator=(const SkillPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline SkillPBComponent& operator=(SkillPBComponent&& from) noexcept {
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
  static const SkillPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const SkillPBComponent* internal_default_instance() {
    return reinterpret_cast<const SkillPBComponent*>(
        &_SkillPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(SkillPBComponent& a, SkillPBComponent& b) { a.Swap(&b); }
  inline void Swap(SkillPBComponent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SkillPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SkillPBComponent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<SkillPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SkillPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SkillPBComponent& from) { SkillPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(SkillPBComponent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "SkillPBComponent"; }

 protected:
  explicit SkillPBComponent(::google::protobuf::Arena* arena);
  SkillPBComponent(::google::protobuf::Arena* arena, const SkillPBComponent& from);
  SkillPBComponent(::google::protobuf::Arena* arena, SkillPBComponent&& from) noexcept
      : SkillPBComponent(arena) {
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
    kSkillTableIdFieldNumber = 1,
  };
  // uint64 skill_table_id = 1;
  void clear_skill_table_id() ;
  ::uint64_t skill_table_id() const;
  void set_skill_table_id(::uint64_t value);

  private:
  ::uint64_t _internal_skill_table_id() const;
  void _internal_set_skill_table_id(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:SkillPBComponent)
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
                          const SkillPBComponent& from_msg);
    ::uint64_t skill_table_id_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto;
};
// -------------------------------------------------------------------

class SkillContextPBComponent_AdditionalDataEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          std::string, ::uint32_t,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_UINT32> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      std::string, ::uint32_t,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>;
  SkillContextPBComponent_AdditionalDataEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillContextPBComponent_AdditionalDataEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit SkillContextPBComponent_AdditionalDataEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const SkillContextPBComponent_AdditionalDataEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const SkillContextPBComponent_AdditionalDataEntry_DoNotUse*>(
        &_SkillContextPBComponent_AdditionalDataEntry_DoNotUse_default_instance_);
  }


 private:
  friend class ::google::protobuf::MessageLite;
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto;

  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      55, 2>
      _table_;

  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;
};
// -------------------------------------------------------------------

class SkillContextPBComponent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SkillContextPBComponent) */ {
 public:
  inline SkillContextPBComponent() : SkillContextPBComponent(nullptr) {}
  ~SkillContextPBComponent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(SkillContextPBComponent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(SkillContextPBComponent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillContextPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline SkillContextPBComponent(const SkillContextPBComponent& from) : SkillContextPBComponent(nullptr, from) {}
  inline SkillContextPBComponent(SkillContextPBComponent&& from) noexcept
      : SkillContextPBComponent(nullptr, std::move(from)) {}
  inline SkillContextPBComponent& operator=(const SkillContextPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline SkillContextPBComponent& operator=(SkillContextPBComponent&& from) noexcept {
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
  static const SkillContextPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const SkillContextPBComponent* internal_default_instance() {
    return reinterpret_cast<const SkillContextPBComponent*>(
        &_SkillContextPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(SkillContextPBComponent& a, SkillContextPBComponent& b) { a.Swap(&b); }
  inline void Swap(SkillContextPBComponent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SkillContextPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SkillContextPBComponent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<SkillContextPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SkillContextPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SkillContextPBComponent& from) { SkillContextPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(SkillContextPBComponent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "SkillContextPBComponent"; }

 protected:
  explicit SkillContextPBComponent(::google::protobuf::Arena* arena);
  SkillContextPBComponent(::google::protobuf::Arena* arena, const SkillContextPBComponent& from);
  SkillContextPBComponent(::google::protobuf::Arena* arena, SkillContextPBComponent&& from) noexcept
      : SkillContextPBComponent(arena) {
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
    kAdditionalDataFieldNumber = 8,
    kStateFieldNumber = 7,
    kCastPositionFieldNumber = 5,
    kCasterFieldNumber = 1,
    kTargetFieldNumber = 2,
    kSkillIdFieldNumber = 3,
    kCastTimeFieldNumber = 6,
    kSkillTableIdFieldNumber = 4,
  };
  // map<string, uint32> additionalData = 8;
  int additionaldata_size() const;
  private:
  int _internal_additionaldata_size() const;

  public:
  void clear_additionaldata() ;
  const ::google::protobuf::Map<std::string, ::uint32_t>& additionaldata() const;
  ::google::protobuf::Map<std::string, ::uint32_t>* mutable_additionaldata();

  private:
  const ::google::protobuf::Map<std::string, ::uint32_t>& _internal_additionaldata() const;
  ::google::protobuf::Map<std::string, ::uint32_t>* _internal_mutable_additionaldata();

  public:
  // string state = 7;
  void clear_state() ;
  const std::string& state() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_state(Arg_&& arg, Args_... args);
  std::string* mutable_state();
  PROTOBUF_NODISCARD std::string* release_state();
  void set_allocated_state(std::string* value);

  private:
  const std::string& _internal_state() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_state(
      const std::string& value);
  std::string* _internal_mutable_state();

  public:
  // .Transform castPosition = 5;
  bool has_castposition() const;
  void clear_castposition() ;
  const ::Transform& castposition() const;
  PROTOBUF_NODISCARD ::Transform* release_castposition();
  ::Transform* mutable_castposition();
  void set_allocated_castposition(::Transform* value);
  void unsafe_arena_set_allocated_castposition(::Transform* value);
  ::Transform* unsafe_arena_release_castposition();

  private:
  const ::Transform& _internal_castposition() const;
  ::Transform* _internal_mutable_castposition();

  public:
  // uint64 caster = 1;
  void clear_caster() ;
  ::uint64_t caster() const;
  void set_caster(::uint64_t value);

  private:
  ::uint64_t _internal_caster() const;
  void _internal_set_caster(::uint64_t value);

  public:
  // uint64 target = 2;
  void clear_target() ;
  ::uint64_t target() const;
  void set_target(::uint64_t value);

  private:
  ::uint64_t _internal_target() const;
  void _internal_set_target(::uint64_t value);

  public:
  // uint64 SkillId = 3;
  void clear_skillid() ;
  ::uint64_t skillid() const;
  void set_skillid(::uint64_t value);

  private:
  ::uint64_t _internal_skillid() const;
  void _internal_set_skillid(::uint64_t value);

  public:
  // uint64 castTime = 6;
  void clear_casttime() ;
  ::uint64_t casttime() const;
  void set_casttime(::uint64_t value);

  private:
  ::uint64_t _internal_casttime() const;
  void _internal_set_casttime(::uint64_t value);

  public:
  // uint32 skillTableId = 4;
  void clear_skilltableid() ;
  ::uint32_t skilltableid() const;
  void set_skilltableid(::uint32_t value);

  private:
  ::uint32_t _internal_skilltableid() const;
  void _internal_set_skilltableid(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:SkillContextPBComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 8, 2,
      59, 2>
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
                          const SkillContextPBComponent& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::MapField<SkillContextPBComponent_AdditionalDataEntry_DoNotUse, std::string, ::uint32_t,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>
        additionaldata_;
    ::google::protobuf::internal::ArenaStringPtr state_;
    ::Transform* castposition_;
    ::uint64_t caster_;
    ::uint64_t target_;
    ::uint64_t skillid_;
    ::uint64_t casttime_;
    ::uint32_t skilltableid_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// SkillPBComponent

// uint64 skill_table_id = 1;
inline void SkillPBComponent::clear_skill_table_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_table_id_ = ::uint64_t{0u};
}
inline ::uint64_t SkillPBComponent::skill_table_id() const {
  // @@protoc_insertion_point(field_get:SkillPBComponent.skill_table_id)
  return _internal_skill_table_id();
}
inline void SkillPBComponent::set_skill_table_id(::uint64_t value) {
  _internal_set_skill_table_id(value);
  // @@protoc_insertion_point(field_set:SkillPBComponent.skill_table_id)
}
inline ::uint64_t SkillPBComponent::_internal_skill_table_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skill_table_id_;
}
inline void SkillPBComponent::_internal_set_skill_table_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_table_id_ = value;
}

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// SkillContextPBComponent

// uint64 caster = 1;
inline void SkillContextPBComponent::clear_caster() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = ::uint64_t{0u};
}
inline ::uint64_t SkillContextPBComponent::caster() const {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.caster)
  return _internal_caster();
}
inline void SkillContextPBComponent::set_caster(::uint64_t value) {
  _internal_set_caster(value);
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.caster)
}
inline ::uint64_t SkillContextPBComponent::_internal_caster() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.caster_;
}
inline void SkillContextPBComponent::_internal_set_caster(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = value;
}

// uint64 target = 2;
inline void SkillContextPBComponent::clear_target() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = ::uint64_t{0u};
}
inline ::uint64_t SkillContextPBComponent::target() const {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.target)
  return _internal_target();
}
inline void SkillContextPBComponent::set_target(::uint64_t value) {
  _internal_set_target(value);
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.target)
}
inline ::uint64_t SkillContextPBComponent::_internal_target() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.target_;
}
inline void SkillContextPBComponent::_internal_set_target(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = value;
}

// uint64 SkillId = 3;
inline void SkillContextPBComponent::clear_skillid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skillid_ = ::uint64_t{0u};
}
inline ::uint64_t SkillContextPBComponent::skillid() const {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.SkillId)
  return _internal_skillid();
}
inline void SkillContextPBComponent::set_skillid(::uint64_t value) {
  _internal_set_skillid(value);
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.SkillId)
}
inline ::uint64_t SkillContextPBComponent::_internal_skillid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skillid_;
}
inline void SkillContextPBComponent::_internal_set_skillid(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skillid_ = value;
}

// uint32 skillTableId = 4;
inline void SkillContextPBComponent::clear_skilltableid() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skilltableid_ = 0u;
}
inline ::uint32_t SkillContextPBComponent::skilltableid() const {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.skillTableId)
  return _internal_skilltableid();
}
inline void SkillContextPBComponent::set_skilltableid(::uint32_t value) {
  _internal_set_skilltableid(value);
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.skillTableId)
}
inline ::uint32_t SkillContextPBComponent::_internal_skilltableid() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skilltableid_;
}
inline void SkillContextPBComponent::_internal_set_skilltableid(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skilltableid_ = value;
}

// .Transform castPosition = 5;
inline bool SkillContextPBComponent::has_castposition() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.castposition_ != nullptr);
  return value;
}
inline const ::Transform& SkillContextPBComponent::_internal_castposition() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Transform* p = _impl_.castposition_;
  return p != nullptr ? *p : reinterpret_cast<const ::Transform&>(::_Transform_default_instance_);
}
inline const ::Transform& SkillContextPBComponent::castposition() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.castPosition)
  return _internal_castposition();
}
inline void SkillContextPBComponent::unsafe_arena_set_allocated_castposition(::Transform* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.castposition_);
  }
  _impl_.castposition_ = reinterpret_cast<::Transform*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:SkillContextPBComponent.castPosition)
}
inline ::Transform* SkillContextPBComponent::release_castposition() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Transform* released = _impl_.castposition_;
  _impl_.castposition_ = nullptr;
  if (::google::protobuf::internal::DebugHardenForceCopyInRelease()) {
    auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
    if (GetArena() == nullptr) {
      delete old;
    }
  } else {
    if (GetArena() != nullptr) {
      released = ::google::protobuf::internal::DuplicateIfNonNull(released);
    }
  }
  return released;
}
inline ::Transform* SkillContextPBComponent::unsafe_arena_release_castposition() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:SkillContextPBComponent.castPosition)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Transform* temp = _impl_.castposition_;
  _impl_.castposition_ = nullptr;
  return temp;
}
inline ::Transform* SkillContextPBComponent::_internal_mutable_castposition() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.castposition_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::Transform>(GetArena());
    _impl_.castposition_ = reinterpret_cast<::Transform*>(p);
  }
  return _impl_.castposition_;
}
inline ::Transform* SkillContextPBComponent::mutable_castposition() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Transform* _msg = _internal_mutable_castposition();
  // @@protoc_insertion_point(field_mutable:SkillContextPBComponent.castPosition)
  return _msg;
}
inline void SkillContextPBComponent::set_allocated_castposition(::Transform* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.castposition_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = reinterpret_cast<::google::protobuf::MessageLite*>(value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }

  _impl_.castposition_ = reinterpret_cast<::Transform*>(value);
  // @@protoc_insertion_point(field_set_allocated:SkillContextPBComponent.castPosition)
}

// uint64 castTime = 6;
inline void SkillContextPBComponent::clear_casttime() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.casttime_ = ::uint64_t{0u};
}
inline ::uint64_t SkillContextPBComponent::casttime() const {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.castTime)
  return _internal_casttime();
}
inline void SkillContextPBComponent::set_casttime(::uint64_t value) {
  _internal_set_casttime(value);
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.castTime)
}
inline ::uint64_t SkillContextPBComponent::_internal_casttime() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.casttime_;
}
inline void SkillContextPBComponent::_internal_set_casttime(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.casttime_ = value;
}

// string state = 7;
inline void SkillContextPBComponent::clear_state() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_.ClearToEmpty();
}
inline const std::string& SkillContextPBComponent::state() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:SkillContextPBComponent.state)
  return _internal_state();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void SkillContextPBComponent::set_state(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:SkillContextPBComponent.state)
}
inline std::string* SkillContextPBComponent::mutable_state() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_state();
  // @@protoc_insertion_point(field_mutable:SkillContextPBComponent.state)
  return _s;
}
inline const std::string& SkillContextPBComponent::_internal_state() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_.Get();
}
inline void SkillContextPBComponent::_internal_set_state(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_.Set(value, GetArena());
}
inline std::string* SkillContextPBComponent::_internal_mutable_state() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.state_.Mutable( GetArena());
}
inline std::string* SkillContextPBComponent::release_state() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:SkillContextPBComponent.state)
  return _impl_.state_.Release();
}
inline void SkillContextPBComponent::set_allocated_state(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.state_.IsDefault()) {
    _impl_.state_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:SkillContextPBComponent.state)
}

// map<string, uint32> additionalData = 8;
inline int SkillContextPBComponent::_internal_additionaldata_size() const {
  return _internal_additionaldata().size();
}
inline int SkillContextPBComponent::additionaldata_size() const {
  return _internal_additionaldata_size();
}
inline void SkillContextPBComponent::clear_additionaldata() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.additionaldata_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::uint32_t>& SkillContextPBComponent::_internal_additionaldata() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.additionaldata_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::uint32_t>& SkillContextPBComponent::additionaldata() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:SkillContextPBComponent.additionalData)
  return _internal_additionaldata();
}
inline ::google::protobuf::Map<std::string, ::uint32_t>* SkillContextPBComponent::_internal_mutable_additionaldata() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.additionaldata_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::uint32_t>* SkillContextPBComponent::mutable_additionaldata() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:SkillContextPBComponent.additionalData)
  return _internal_mutable_additionaldata();
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fcomponent_2fskill_5fcomp_2eproto_2epb_2eh
