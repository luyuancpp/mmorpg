// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/combat_event.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fevent_2fcombat_5fevent_2eproto_2epb_2eh
#define proto_2flogic_2fevent_2fcombat_5fevent_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fevent_2fcombat_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fevent_2fcombat_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fevent_2fcombat_5fevent_2eproto;
class BeKillEvent;
struct BeKillEventDefaultTypeInternal;
extern BeKillEventDefaultTypeInternal _BeKillEvent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class BeKillEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:BeKillEvent) */ {
 public:
  inline BeKillEvent() : BeKillEvent(nullptr) {}
  ~BeKillEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(BeKillEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(BeKillEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR BeKillEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline BeKillEvent(const BeKillEvent& from) : BeKillEvent(nullptr, from) {}
  inline BeKillEvent(BeKillEvent&& from) noexcept
      : BeKillEvent(nullptr, std::move(from)) {}
  inline BeKillEvent& operator=(const BeKillEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline BeKillEvent& operator=(BeKillEvent&& from) noexcept {
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
  static const BeKillEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const BeKillEvent* internal_default_instance() {
    return reinterpret_cast<const BeKillEvent*>(
        &_BeKillEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(BeKillEvent& a, BeKillEvent& b) { a.Swap(&b); }
  inline void Swap(BeKillEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(BeKillEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  BeKillEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<BeKillEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const BeKillEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const BeKillEvent& from) { BeKillEvent::MergeImpl(*this, from); }

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
  void InternalSwap(BeKillEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "BeKillEvent"; }

 protected:
  explicit BeKillEvent(::google::protobuf::Arena* arena);
  BeKillEvent(::google::protobuf::Arena* arena, const BeKillEvent& from);
  BeKillEvent(::google::protobuf::Arena* arena, BeKillEvent&& from) noexcept
      : BeKillEvent(arena) {
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
    kCasterFieldNumber = 1,
    kTargetFieldNumber = 2,
  };
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
  // @@protoc_insertion_point(class_scope:BeKillEvent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
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
                          const BeKillEvent& from_msg);
    ::uint64_t caster_;
    ::uint64_t target_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fcombat_5fevent_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// BeKillEvent

// uint64 caster = 1;
inline void BeKillEvent::clear_caster() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = ::uint64_t{0u};
}
inline ::uint64_t BeKillEvent::caster() const {
  // @@protoc_insertion_point(field_get:BeKillEvent.caster)
  return _internal_caster();
}
inline void BeKillEvent::set_caster(::uint64_t value) {
  _internal_set_caster(value);
  // @@protoc_insertion_point(field_set:BeKillEvent.caster)
}
inline ::uint64_t BeKillEvent::_internal_caster() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.caster_;
}
inline void BeKillEvent::_internal_set_caster(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.caster_ = value;
}

// uint64 target = 2;
inline void BeKillEvent::clear_target() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = ::uint64_t{0u};
}
inline ::uint64_t BeKillEvent::target() const {
  // @@protoc_insertion_point(field_get:BeKillEvent.target)
  return _internal_target();
}
inline void BeKillEvent::set_target(::uint64_t value) {
  _internal_set_target(value);
  // @@protoc_insertion_point(field_set:BeKillEvent.target)
}
inline ::uint64_t BeKillEvent::_internal_target() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.target_;
}
inline void BeKillEvent::_internal_set_target(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fevent_2fcombat_5fevent_2eproto_2epb_2eh
