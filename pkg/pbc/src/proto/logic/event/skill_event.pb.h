// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/skill_event.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef proto_2flogic_2fevent_2fskill_5fevent_2eproto_2epb_2eh
#define proto_2flogic_2fevent_2fskill_5fevent_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fevent_2fskill_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fevent_2fskill_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto;
}  // extern "C"
class SkillExecutedEvent;
struct SkillExecutedEventDefaultTypeInternal;
extern SkillExecutedEventDefaultTypeInternal _SkillExecutedEvent_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull SkillExecutedEvent_class_data_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class SkillExecutedEvent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SkillExecutedEvent) */ {
 public:
  inline SkillExecutedEvent() : SkillExecutedEvent(nullptr) {}
  ~SkillExecutedEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(SkillExecutedEvent* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(SkillExecutedEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillExecutedEvent(::google::protobuf::internal::ConstantInitialized);

  inline SkillExecutedEvent(const SkillExecutedEvent& from) : SkillExecutedEvent(nullptr, from) {}
  inline SkillExecutedEvent(SkillExecutedEvent&& from) noexcept
      : SkillExecutedEvent(nullptr, std::move(from)) {}
  inline SkillExecutedEvent& operator=(const SkillExecutedEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline SkillExecutedEvent& operator=(SkillExecutedEvent&& from) noexcept {
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
  static const SkillExecutedEvent& default_instance() {
    return *reinterpret_cast<const SkillExecutedEvent*>(
        &_SkillExecutedEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(SkillExecutedEvent& a, SkillExecutedEvent& b) { a.Swap(&b); }
  inline void Swap(SkillExecutedEvent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SkillExecutedEvent* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SkillExecutedEvent* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<SkillExecutedEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SkillExecutedEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SkillExecutedEvent& from) { SkillExecutedEvent::MergeImpl(*this, from); }

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
  void InternalSwap(SkillExecutedEvent* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "SkillExecutedEvent"; }

 protected:
  explicit SkillExecutedEvent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  SkillExecutedEvent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const SkillExecutedEvent& from);
  SkillExecutedEvent(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, SkillExecutedEvent&& from) noexcept
      : SkillExecutedEvent(arena) {
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
    kSkillIdFieldNumber = 1,
    kCasterFieldNumber = 3,
    kTargetFieldNumber = 4,
    kSkillTableIdFieldNumber = 2,
  };
  // uint64 skill_id = 1;
  void clear_skill_id() ;
  ::uint64_t skill_id() const;
  void set_skill_id(::uint64_t value);

  private:
  ::uint64_t _internal_skill_id() const;
  void _internal_set_skill_id(::uint64_t value);

  public:
  // uint64 caster = 3;
  void clear_caster() ;
  ::uint64_t caster() const;
  void set_caster(::uint64_t value);

  private:
  ::uint64_t _internal_caster() const;
  void _internal_set_caster(::uint64_t value);

  public:
  // uint64 target = 4;
  void clear_target() ;
  ::uint64_t target() const;
  void set_target(::uint64_t value);

  private:
  ::uint64_t _internal_target() const;
  void _internal_set_target(::uint64_t value);

  public:
  // uint32 skill_table_id = 2;
  void clear_skill_table_id() ;
  ::uint32_t skill_table_id() const;
  void set_skill_table_id(::uint32_t value);

  private:
  ::uint32_t _internal_skill_table_id() const;
  void _internal_set_skill_table_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:SkillExecutedEvent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<2, 4,
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
        const SkillExecutedEvent& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint64_t skill_id_;
    ::uint64_t caster_;
    ::uint64_t target_;
    ::uint32_t skill_table_id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fskill_5fevent_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull SkillExecutedEvent_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// SkillExecutedEvent

// uint64 skill_id = 1;
inline void SkillExecutedEvent::clear_skill_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_id_ = ::uint64_t{0u};
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint64_t SkillExecutedEvent::skill_id() const {
  // @@protoc_insertion_point(field_get:SkillExecutedEvent.skill_id)
  return _internal_skill_id();
}
inline void SkillExecutedEvent::set_skill_id(::uint64_t value) {
  _internal_set_skill_id(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:SkillExecutedEvent.skill_id)
}
inline ::uint64_t SkillExecutedEvent::_internal_skill_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skill_id_;
}
inline void SkillExecutedEvent::_internal_set_skill_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_id_ = value;
}

// uint32 skill_table_id = 2;
inline void SkillExecutedEvent::clear_skill_table_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_table_id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000008u;
}
inline ::uint32_t SkillExecutedEvent::skill_table_id() const {
  // @@protoc_insertion_point(field_get:SkillExecutedEvent.skill_table_id)
  return _internal_skill_table_id();
}
inline void SkillExecutedEvent::set_skill_table_id(::uint32_t value) {
  _internal_set_skill_table_id(value);
  _impl_._has_bits_[0] |= 0x00000008u;
  // @@protoc_insertion_point(field_set:SkillExecutedEvent.skill_table_id)
}
inline ::uint32_t SkillExecutedEvent::_internal_skill_table_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skill_table_id_;
}
inline void SkillExecutedEvent::_internal_set_skill_table_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skill_table_id_ = value;
}

// uint64 caster = 3;
inline void SkillExecutedEvent::clear_caster() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = ::uint64_t{0u};
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline ::uint64_t SkillExecutedEvent::caster() const {
  // @@protoc_insertion_point(field_get:SkillExecutedEvent.caster)
  return _internal_caster();
}
inline void SkillExecutedEvent::set_caster(::uint64_t value) {
  _internal_set_caster(value);
  _impl_._has_bits_[0] |= 0x00000002u;
  // @@protoc_insertion_point(field_set:SkillExecutedEvent.caster)
}
inline ::uint64_t SkillExecutedEvent::_internal_caster() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.caster_;
}
inline void SkillExecutedEvent::_internal_set_caster(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = value;
}

// uint64 target = 4;
inline void SkillExecutedEvent::clear_target() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = ::uint64_t{0u};
  _impl_._has_bits_[0] &= ~0x00000004u;
}
inline ::uint64_t SkillExecutedEvent::target() const {
  // @@protoc_insertion_point(field_get:SkillExecutedEvent.target)
  return _internal_target();
}
inline void SkillExecutedEvent::set_target(::uint64_t value) {
  _internal_set_target(value);
  _impl_._has_bits_[0] |= 0x00000004u;
  // @@protoc_insertion_point(field_set:SkillExecutedEvent.target)
}
inline ::uint64_t SkillExecutedEvent::_internal_target() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.target_;
}
inline void SkillExecutedEvent::_internal_set_target(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fevent_2fskill_5fevent_2eproto_2epb_2eh
