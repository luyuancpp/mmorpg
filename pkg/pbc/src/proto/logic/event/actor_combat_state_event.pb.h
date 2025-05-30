// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/actor_combat_state_event.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto_2epb_2eh
#define proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto_2epb_2eh

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
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto;
class CombatStateAddedPbEvent;
struct CombatStateAddedPbEventDefaultTypeInternal;
extern CombatStateAddedPbEventDefaultTypeInternal _CombatStateAddedPbEvent_default_instance_;
class CombatStateRemovedPbEvent;
struct CombatStateRemovedPbEventDefaultTypeInternal;
extern CombatStateRemovedPbEventDefaultTypeInternal _CombatStateRemovedPbEvent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class CombatStateRemovedPbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CombatStateRemovedPbEvent) */ {
 public:
  inline CombatStateRemovedPbEvent() : CombatStateRemovedPbEvent(nullptr) {}
  ~CombatStateRemovedPbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CombatStateRemovedPbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CombatStateRemovedPbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateRemovedPbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline CombatStateRemovedPbEvent(const CombatStateRemovedPbEvent& from) : CombatStateRemovedPbEvent(nullptr, from) {}
  inline CombatStateRemovedPbEvent(CombatStateRemovedPbEvent&& from) noexcept
      : CombatStateRemovedPbEvent(nullptr, std::move(from)) {}
  inline CombatStateRemovedPbEvent& operator=(const CombatStateRemovedPbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline CombatStateRemovedPbEvent& operator=(CombatStateRemovedPbEvent&& from) noexcept {
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
  static const CombatStateRemovedPbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const CombatStateRemovedPbEvent* internal_default_instance() {
    return reinterpret_cast<const CombatStateRemovedPbEvent*>(
        &_CombatStateRemovedPbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(CombatStateRemovedPbEvent& a, CombatStateRemovedPbEvent& b) { a.Swap(&b); }
  inline void Swap(CombatStateRemovedPbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CombatStateRemovedPbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CombatStateRemovedPbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CombatStateRemovedPbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CombatStateRemovedPbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CombatStateRemovedPbEvent& from) { CombatStateRemovedPbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(CombatStateRemovedPbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CombatStateRemovedPbEvent"; }

 protected:
  explicit CombatStateRemovedPbEvent(::google::protobuf::Arena* arena);
  CombatStateRemovedPbEvent(::google::protobuf::Arena* arena, const CombatStateRemovedPbEvent& from);
  CombatStateRemovedPbEvent(::google::protobuf::Arena* arena, CombatStateRemovedPbEvent&& from) noexcept
      : CombatStateRemovedPbEvent(arena) {
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
    kActorEntityFieldNumber = 1,
    kSourceBuffIdFieldNumber = 2,
    kStateTypeFieldNumber = 3,
  };
  // uint64 actor_entity = 1;
  void clear_actor_entity() ;
  ::uint64_t actor_entity() const;
  void set_actor_entity(::uint64_t value);

  private:
  ::uint64_t _internal_actor_entity() const;
  void _internal_set_actor_entity(::uint64_t value);

  public:
  // uint64 source_buff_id = 2;
  void clear_source_buff_id() ;
  ::uint64_t source_buff_id() const;
  void set_source_buff_id(::uint64_t value);

  private:
  ::uint64_t _internal_source_buff_id() const;
  void _internal_set_source_buff_id(::uint64_t value);

  public:
  // uint32 state_type = 3;
  void clear_state_type() ;
  ::uint32_t state_type() const;
  void set_state_type(::uint32_t value);

  private:
  ::uint32_t _internal_state_type() const;
  void _internal_set_state_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:CombatStateRemovedPbEvent)
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
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const CombatStateRemovedPbEvent& from_msg);
    ::uint64_t actor_entity_;
    ::uint64_t source_buff_id_;
    ::uint32_t state_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto;
};
// -------------------------------------------------------------------

class CombatStateAddedPbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CombatStateAddedPbEvent) */ {
 public:
  inline CombatStateAddedPbEvent() : CombatStateAddedPbEvent(nullptr) {}
  ~CombatStateAddedPbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CombatStateAddedPbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CombatStateAddedPbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CombatStateAddedPbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline CombatStateAddedPbEvent(const CombatStateAddedPbEvent& from) : CombatStateAddedPbEvent(nullptr, from) {}
  inline CombatStateAddedPbEvent(CombatStateAddedPbEvent&& from) noexcept
      : CombatStateAddedPbEvent(nullptr, std::move(from)) {}
  inline CombatStateAddedPbEvent& operator=(const CombatStateAddedPbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline CombatStateAddedPbEvent& operator=(CombatStateAddedPbEvent&& from) noexcept {
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
  static const CombatStateAddedPbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const CombatStateAddedPbEvent* internal_default_instance() {
    return reinterpret_cast<const CombatStateAddedPbEvent*>(
        &_CombatStateAddedPbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(CombatStateAddedPbEvent& a, CombatStateAddedPbEvent& b) { a.Swap(&b); }
  inline void Swap(CombatStateAddedPbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CombatStateAddedPbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CombatStateAddedPbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CombatStateAddedPbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CombatStateAddedPbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CombatStateAddedPbEvent& from) { CombatStateAddedPbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(CombatStateAddedPbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CombatStateAddedPbEvent"; }

 protected:
  explicit CombatStateAddedPbEvent(::google::protobuf::Arena* arena);
  CombatStateAddedPbEvent(::google::protobuf::Arena* arena, const CombatStateAddedPbEvent& from);
  CombatStateAddedPbEvent(::google::protobuf::Arena* arena, CombatStateAddedPbEvent&& from) noexcept
      : CombatStateAddedPbEvent(arena) {
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
    kActorEntityFieldNumber = 1,
    kSourceBuffIdFieldNumber = 2,
    kStateTypeFieldNumber = 3,
  };
  // uint64 actor_entity = 1;
  void clear_actor_entity() ;
  ::uint64_t actor_entity() const;
  void set_actor_entity(::uint64_t value);

  private:
  ::uint64_t _internal_actor_entity() const;
  void _internal_set_actor_entity(::uint64_t value);

  public:
  // uint64 source_buff_id = 2;
  void clear_source_buff_id() ;
  ::uint64_t source_buff_id() const;
  void set_source_buff_id(::uint64_t value);

  private:
  ::uint64_t _internal_source_buff_id() const;
  void _internal_set_source_buff_id(::uint64_t value);

  public:
  // uint32 state_type = 3;
  void clear_state_type() ;
  ::uint32_t state_type() const;
  void set_state_type(::uint32_t value);

  private:
  ::uint32_t _internal_state_type() const;
  void _internal_set_state_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:CombatStateAddedPbEvent)
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
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const CombatStateAddedPbEvent& from_msg);
    ::uint64_t actor_entity_;
    ::uint64_t source_buff_id_;
    ::uint32_t state_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// CombatStateAddedPbEvent

// uint64 actor_entity = 1;
inline void CombatStateAddedPbEvent::clear_actor_entity() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.actor_entity_ = ::uint64_t{0u};
}
inline ::uint64_t CombatStateAddedPbEvent::actor_entity() const {
  // @@protoc_insertion_point(field_get:CombatStateAddedPbEvent.actor_entity)
  return _internal_actor_entity();
}
inline void CombatStateAddedPbEvent::set_actor_entity(::uint64_t value) {
  _internal_set_actor_entity(value);
  // @@protoc_insertion_point(field_set:CombatStateAddedPbEvent.actor_entity)
}
inline ::uint64_t CombatStateAddedPbEvent::_internal_actor_entity() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.actor_entity_;
}
inline void CombatStateAddedPbEvent::_internal_set_actor_entity(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.actor_entity_ = value;
}

// uint64 source_buff_id = 2;
inline void CombatStateAddedPbEvent::clear_source_buff_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_buff_id_ = ::uint64_t{0u};
}
inline ::uint64_t CombatStateAddedPbEvent::source_buff_id() const {
  // @@protoc_insertion_point(field_get:CombatStateAddedPbEvent.source_buff_id)
  return _internal_source_buff_id();
}
inline void CombatStateAddedPbEvent::set_source_buff_id(::uint64_t value) {
  _internal_set_source_buff_id(value);
  // @@protoc_insertion_point(field_set:CombatStateAddedPbEvent.source_buff_id)
}
inline ::uint64_t CombatStateAddedPbEvent::_internal_source_buff_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.source_buff_id_;
}
inline void CombatStateAddedPbEvent::_internal_set_source_buff_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_buff_id_ = value;
}

// uint32 state_type = 3;
inline void CombatStateAddedPbEvent::clear_state_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_type_ = 0u;
}
inline ::uint32_t CombatStateAddedPbEvent::state_type() const {
  // @@protoc_insertion_point(field_get:CombatStateAddedPbEvent.state_type)
  return _internal_state_type();
}
inline void CombatStateAddedPbEvent::set_state_type(::uint32_t value) {
  _internal_set_state_type(value);
  // @@protoc_insertion_point(field_set:CombatStateAddedPbEvent.state_type)
}
inline ::uint32_t CombatStateAddedPbEvent::_internal_state_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_type_;
}
inline void CombatStateAddedPbEvent::_internal_set_state_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_type_ = value;
}

// -------------------------------------------------------------------

// CombatStateRemovedPbEvent

// uint64 actor_entity = 1;
inline void CombatStateRemovedPbEvent::clear_actor_entity() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.actor_entity_ = ::uint64_t{0u};
}
inline ::uint64_t CombatStateRemovedPbEvent::actor_entity() const {
  // @@protoc_insertion_point(field_get:CombatStateRemovedPbEvent.actor_entity)
  return _internal_actor_entity();
}
inline void CombatStateRemovedPbEvent::set_actor_entity(::uint64_t value) {
  _internal_set_actor_entity(value);
  // @@protoc_insertion_point(field_set:CombatStateRemovedPbEvent.actor_entity)
}
inline ::uint64_t CombatStateRemovedPbEvent::_internal_actor_entity() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.actor_entity_;
}
inline void CombatStateRemovedPbEvent::_internal_set_actor_entity(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.actor_entity_ = value;
}

// uint64 source_buff_id = 2;
inline void CombatStateRemovedPbEvent::clear_source_buff_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_buff_id_ = ::uint64_t{0u};
}
inline ::uint64_t CombatStateRemovedPbEvent::source_buff_id() const {
  // @@protoc_insertion_point(field_get:CombatStateRemovedPbEvent.source_buff_id)
  return _internal_source_buff_id();
}
inline void CombatStateRemovedPbEvent::set_source_buff_id(::uint64_t value) {
  _internal_set_source_buff_id(value);
  // @@protoc_insertion_point(field_set:CombatStateRemovedPbEvent.source_buff_id)
}
inline ::uint64_t CombatStateRemovedPbEvent::_internal_source_buff_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.source_buff_id_;
}
inline void CombatStateRemovedPbEvent::_internal_set_source_buff_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_buff_id_ = value;
}

// uint32 state_type = 3;
inline void CombatStateRemovedPbEvent::clear_state_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_type_ = 0u;
}
inline ::uint32_t CombatStateRemovedPbEvent::state_type() const {
  // @@protoc_insertion_point(field_get:CombatStateRemovedPbEvent.state_type)
  return _internal_state_type();
}
inline void CombatStateRemovedPbEvent::set_state_type(::uint32_t value) {
  _internal_set_state_type(value);
  // @@protoc_insertion_point(field_set:CombatStateRemovedPbEvent.state_type)
}
inline ::uint32_t CombatStateRemovedPbEvent::_internal_state_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.state_type_;
}
inline void CombatStateRemovedPbEvent::_internal_set_state_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.state_type_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fevent_2factor_5fcombat_5fstate_5fevent_2eproto_2epb_2eh
