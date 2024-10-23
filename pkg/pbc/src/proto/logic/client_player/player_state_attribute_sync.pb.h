// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/client_player/player_state_attribute_sync.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_2epb_2eh

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
#include "google/protobuf/service.h"
#include "google/protobuf/unknown_field_set.h"
#include "common/tip.pb.h"
#include "common/comp.pb.h"
#include "common/empty.pb.h"
#include "logic/component/skill_comp.pb.h"
#include "logic/component/actor_comp.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto;
class SyncBaseStateAttributeDeltaS2C;
struct SyncBaseStateAttributeDeltaS2CDefaultTypeInternal;
extern SyncBaseStateAttributeDeltaS2CDefaultTypeInternal _SyncBaseStateAttributeDeltaS2C_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class SyncBaseStateAttributeDeltaS2C final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SyncBaseStateAttributeDeltaS2C) */ {
 public:
  inline SyncBaseStateAttributeDeltaS2C() : SyncBaseStateAttributeDeltaS2C(nullptr) {}
  ~SyncBaseStateAttributeDeltaS2C() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SyncBaseStateAttributeDeltaS2C(
      ::google::protobuf::internal::ConstantInitialized);

  inline SyncBaseStateAttributeDeltaS2C(const SyncBaseStateAttributeDeltaS2C& from) : SyncBaseStateAttributeDeltaS2C(nullptr, from) {}
  inline SyncBaseStateAttributeDeltaS2C(SyncBaseStateAttributeDeltaS2C&& from) noexcept
      : SyncBaseStateAttributeDeltaS2C(nullptr, std::move(from)) {}
  inline SyncBaseStateAttributeDeltaS2C& operator=(const SyncBaseStateAttributeDeltaS2C& from) {
    CopyFrom(from);
    return *this;
  }
  inline SyncBaseStateAttributeDeltaS2C& operator=(SyncBaseStateAttributeDeltaS2C&& from) noexcept {
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
  static const SyncBaseStateAttributeDeltaS2C& default_instance() {
    return *internal_default_instance();
  }
  static inline const SyncBaseStateAttributeDeltaS2C* internal_default_instance() {
    return reinterpret_cast<const SyncBaseStateAttributeDeltaS2C*>(
        &_SyncBaseStateAttributeDeltaS2C_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(SyncBaseStateAttributeDeltaS2C& a, SyncBaseStateAttributeDeltaS2C& b) { a.Swap(&b); }
  inline void Swap(SyncBaseStateAttributeDeltaS2C* other) {
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
  void UnsafeArenaSwap(SyncBaseStateAttributeDeltaS2C* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SyncBaseStateAttributeDeltaS2C* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<SyncBaseStateAttributeDeltaS2C>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SyncBaseStateAttributeDeltaS2C& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SyncBaseStateAttributeDeltaS2C& from) { SyncBaseStateAttributeDeltaS2C::MergeImpl(*this, from); }

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
  void InternalSwap(SyncBaseStateAttributeDeltaS2C* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "SyncBaseStateAttributeDeltaS2C"; }

 protected:
  explicit SyncBaseStateAttributeDeltaS2C(::google::protobuf::Arena* arena);
  SyncBaseStateAttributeDeltaS2C(::google::protobuf::Arena* arena, const SyncBaseStateAttributeDeltaS2C& from);
  SyncBaseStateAttributeDeltaS2C(::google::protobuf::Arena* arena, SyncBaseStateAttributeDeltaS2C&& from) noexcept
      : SyncBaseStateAttributeDeltaS2C(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kVelocityFieldNumber = 2,
    kEntityIdFieldNumber = 1,
  };
  // .Velocity velocity = 2;
  bool has_velocity() const;
  void clear_velocity() ;
  const ::Velocity& velocity() const;
  PROTOBUF_NODISCARD ::Velocity* release_velocity();
  ::Velocity* mutable_velocity();
  void set_allocated_velocity(::Velocity* value);
  void unsafe_arena_set_allocated_velocity(::Velocity* value);
  ::Velocity* unsafe_arena_release_velocity();

  private:
  const ::Velocity& _internal_velocity() const;
  ::Velocity* _internal_mutable_velocity();

  public:
  // uint64 entity_id = 1;
  void clear_entity_id() ;
  ::uint64_t entity_id() const;
  void set_entity_id(::uint64_t value);

  private:
  ::uint64_t _internal_entity_id() const;
  void _internal_set_entity_id(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:SyncBaseStateAttributeDeltaS2C)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 1,
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
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::Velocity* velocity_;
    ::uint64_t entity_id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto;
};

// ===================================================================


// -------------------------------------------------------------------

class EntityStateSyncService_Stub;
class EntityStateSyncService : public ::google::protobuf::Service {
 protected:
  EntityStateSyncService() = default;

 public:
  using Stub = EntityStateSyncService_Stub;

  EntityStateSyncService(const EntityStateSyncService&) = delete;
  EntityStateSyncService& operator=(const EntityStateSyncService&) = delete;
  virtual ~EntityStateSyncService() = default;

  static const ::google::protobuf::ServiceDescriptor* descriptor();

  virtual void SyncBaseStateAttribute(::google::protobuf::RpcController* controller,
                        const ::SyncBaseStateAttributeDeltaS2C* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done);

  // implements Service ----------------------------------------------
  const ::google::protobuf::ServiceDescriptor* GetDescriptor() override;

  void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                  ::google::protobuf::RpcController* controller,
                  const ::google::protobuf::Message* request,
                  ::google::protobuf::Message* response,
                  ::google::protobuf::Closure* done) override;

  const ::google::protobuf::Message& GetRequestPrototype(
      const ::google::protobuf::MethodDescriptor* method) const override;

  const ::google::protobuf::Message& GetResponsePrototype(
      const ::google::protobuf::MethodDescriptor* method) const override;
};

class EntityStateSyncService_Stub final : public EntityStateSyncService {
 public:
  EntityStateSyncService_Stub(::google::protobuf::RpcChannel* channel);
  EntityStateSyncService_Stub(::google::protobuf::RpcChannel* channel,
                   ::google::protobuf::Service::ChannelOwnership ownership);

  EntityStateSyncService_Stub(const EntityStateSyncService_Stub&) = delete;
  EntityStateSyncService_Stub& operator=(const EntityStateSyncService_Stub&) = delete;

  ~EntityStateSyncService_Stub() override;

  inline ::google::protobuf::RpcChannel* channel() { return channel_; }

  // implements EntityStateSyncService ------------------------------------------
  void SyncBaseStateAttribute(::google::protobuf::RpcController* controller,
                        const ::SyncBaseStateAttributeDeltaS2C* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done) override;

 private:
  ::google::protobuf::RpcChannel* channel_;
  bool owns_channel_;
};
// ===================================================================



// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// SyncBaseStateAttributeDeltaS2C

// uint64 entity_id = 1;
inline void SyncBaseStateAttributeDeltaS2C::clear_entity_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_id_ = ::uint64_t{0u};
}
inline ::uint64_t SyncBaseStateAttributeDeltaS2C::entity_id() const {
  // @@protoc_insertion_point(field_get:SyncBaseStateAttributeDeltaS2C.entity_id)
  return _internal_entity_id();
}
inline void SyncBaseStateAttributeDeltaS2C::set_entity_id(::uint64_t value) {
  _internal_set_entity_id(value);
  // @@protoc_insertion_point(field_set:SyncBaseStateAttributeDeltaS2C.entity_id)
}
inline ::uint64_t SyncBaseStateAttributeDeltaS2C::_internal_entity_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.entity_id_;
}
inline void SyncBaseStateAttributeDeltaS2C::_internal_set_entity_id(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_id_ = value;
}

// .Velocity velocity = 2;
inline bool SyncBaseStateAttributeDeltaS2C::has_velocity() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.velocity_ != nullptr);
  return value;
}
inline const ::Velocity& SyncBaseStateAttributeDeltaS2C::_internal_velocity() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  const ::Velocity* p = _impl_.velocity_;
  return p != nullptr ? *p : reinterpret_cast<const ::Velocity&>(::_Velocity_default_instance_);
}
inline const ::Velocity& SyncBaseStateAttributeDeltaS2C::velocity() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:SyncBaseStateAttributeDeltaS2C.velocity)
  return _internal_velocity();
}
inline void SyncBaseStateAttributeDeltaS2C::unsafe_arena_set_allocated_velocity(::Velocity* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.velocity_);
  }
  _impl_.velocity_ = reinterpret_cast<::Velocity*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:SyncBaseStateAttributeDeltaS2C.velocity)
}
inline ::Velocity* SyncBaseStateAttributeDeltaS2C::release_velocity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Velocity* released = _impl_.velocity_;
  _impl_.velocity_ = nullptr;
#ifdef PROTOBUF_FORCE_COPY_IN_RELEASE
  auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
  released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  if (GetArena() == nullptr) {
    delete old;
  }
#else   // PROTOBUF_FORCE_COPY_IN_RELEASE
  if (GetArena() != nullptr) {
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  }
#endif  // !PROTOBUF_FORCE_COPY_IN_RELEASE
  return released;
}
inline ::Velocity* SyncBaseStateAttributeDeltaS2C::unsafe_arena_release_velocity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:SyncBaseStateAttributeDeltaS2C.velocity)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Velocity* temp = _impl_.velocity_;
  _impl_.velocity_ = nullptr;
  return temp;
}
inline ::Velocity* SyncBaseStateAttributeDeltaS2C::_internal_mutable_velocity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.velocity_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::Velocity>(GetArena());
    _impl_.velocity_ = reinterpret_cast<::Velocity*>(p);
  }
  return _impl_.velocity_;
}
inline ::Velocity* SyncBaseStateAttributeDeltaS2C::mutable_velocity() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Velocity* _msg = _internal_mutable_velocity();
  // @@protoc_insertion_point(field_mutable:SyncBaseStateAttributeDeltaS2C.velocity)
  return _msg;
}
inline void SyncBaseStateAttributeDeltaS2C::set_allocated_velocity(::Velocity* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (message_arena == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.velocity_);
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

  _impl_.velocity_ = reinterpret_cast<::Velocity*>(value);
  // @@protoc_insertion_point(field_set_allocated:SyncBaseStateAttributeDeltaS2C.velocity)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_2epb_2eh
