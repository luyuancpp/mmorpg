// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/component/player_scene_comp.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto_2epb_2eh

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
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto;
class PlayerSceneContextPBComponent;
struct PlayerSceneContextPBComponentDefaultTypeInternal;
extern PlayerSceneContextPBComponentDefaultTypeInternal _PlayerSceneContextPBComponent_default_instance_;
class PlayerSceneInfoPBComponent;
struct PlayerSceneInfoPBComponentDefaultTypeInternal;
extern PlayerSceneInfoPBComponentDefaultTypeInternal _PlayerSceneInfoPBComponent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class PlayerSceneInfoPBComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerSceneInfoPBComponent) */ {
 public:
  inline PlayerSceneInfoPBComponent() : PlayerSceneInfoPBComponent(nullptr) {}
  ~PlayerSceneInfoPBComponent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerSceneInfoPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerSceneInfoPBComponent(const PlayerSceneInfoPBComponent& from) : PlayerSceneInfoPBComponent(nullptr, from) {}
  inline PlayerSceneInfoPBComponent(PlayerSceneInfoPBComponent&& from) noexcept
      : PlayerSceneInfoPBComponent(nullptr, std::move(from)) {}
  inline PlayerSceneInfoPBComponent& operator=(const PlayerSceneInfoPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerSceneInfoPBComponent& operator=(PlayerSceneInfoPBComponent&& from) noexcept {
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
  static const PlayerSceneInfoPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerSceneInfoPBComponent* internal_default_instance() {
    return reinterpret_cast<const PlayerSceneInfoPBComponent*>(
        &_PlayerSceneInfoPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(PlayerSceneInfoPBComponent& a, PlayerSceneInfoPBComponent& b) { a.Swap(&b); }
  inline void Swap(PlayerSceneInfoPBComponent* other) {
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
  void UnsafeArenaSwap(PlayerSceneInfoPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerSceneInfoPBComponent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<PlayerSceneInfoPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerSceneInfoPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerSceneInfoPBComponent& from) { PlayerSceneInfoPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerSceneInfoPBComponent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "PlayerSceneInfoPBComponent"; }

 protected:
  explicit PlayerSceneInfoPBComponent(::google::protobuf::Arena* arena);
  PlayerSceneInfoPBComponent(::google::protobuf::Arena* arena, const PlayerSceneInfoPBComponent& from);
  PlayerSceneInfoPBComponent(::google::protobuf::Arena* arena, PlayerSceneInfoPBComponent&& from) noexcept
      : PlayerSceneInfoPBComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSceneConfidFieldNumber = 1,
    kGuidFieldNumber = 2,
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
  // @@protoc_insertion_point(class_scope:PlayerSceneInfoPBComponent)
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
                          ::google::protobuf::Arena* arena, const Impl_& from);
    ::uint32_t scene_confid_;
    ::uint32_t guid_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto;
};
// -------------------------------------------------------------------

class PlayerSceneContextPBComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerSceneContextPBComponent) */ {
 public:
  inline PlayerSceneContextPBComponent() : PlayerSceneContextPBComponent(nullptr) {}
  ~PlayerSceneContextPBComponent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerSceneContextPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerSceneContextPBComponent(const PlayerSceneContextPBComponent& from) : PlayerSceneContextPBComponent(nullptr, from) {}
  inline PlayerSceneContextPBComponent(PlayerSceneContextPBComponent&& from) noexcept
      : PlayerSceneContextPBComponent(nullptr, std::move(from)) {}
  inline PlayerSceneContextPBComponent& operator=(const PlayerSceneContextPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerSceneContextPBComponent& operator=(PlayerSceneContextPBComponent&& from) noexcept {
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
  static const PlayerSceneContextPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerSceneContextPBComponent* internal_default_instance() {
    return reinterpret_cast<const PlayerSceneContextPBComponent*>(
        &_PlayerSceneContextPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(PlayerSceneContextPBComponent& a, PlayerSceneContextPBComponent& b) { a.Swap(&b); }
  inline void Swap(PlayerSceneContextPBComponent* other) {
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
  void UnsafeArenaSwap(PlayerSceneContextPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerSceneContextPBComponent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<PlayerSceneContextPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerSceneContextPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerSceneContextPBComponent& from) { PlayerSceneContextPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerSceneContextPBComponent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "PlayerSceneContextPBComponent"; }

 protected:
  explicit PlayerSceneContextPBComponent(::google::protobuf::Arena* arena);
  PlayerSceneContextPBComponent(::google::protobuf::Arena* arena, const PlayerSceneContextPBComponent& from);
  PlayerSceneContextPBComponent(::google::protobuf::Arena* arena, PlayerSceneContextPBComponent&& from) noexcept
      : PlayerSceneContextPBComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSceneInfoFieldNumber = 1,
    kSceneInfoLastTimeFieldNumber = 2,
  };
  // .PlayerSceneInfoPBComponent scene_info = 1;
  bool has_scene_info() const;
  void clear_scene_info() ;
  const ::PlayerSceneInfoPBComponent& scene_info() const;
  PROTOBUF_NODISCARD ::PlayerSceneInfoPBComponent* release_scene_info();
  ::PlayerSceneInfoPBComponent* mutable_scene_info();
  void set_allocated_scene_info(::PlayerSceneInfoPBComponent* value);
  void unsafe_arena_set_allocated_scene_info(::PlayerSceneInfoPBComponent* value);
  ::PlayerSceneInfoPBComponent* unsafe_arena_release_scene_info();

  private:
  const ::PlayerSceneInfoPBComponent& _internal_scene_info() const;
  ::PlayerSceneInfoPBComponent* _internal_mutable_scene_info();

  public:
  // .PlayerSceneInfoPBComponent scene_info_last_time = 2;
  bool has_scene_info_last_time() const;
  void clear_scene_info_last_time() ;
  const ::PlayerSceneInfoPBComponent& scene_info_last_time() const;
  PROTOBUF_NODISCARD ::PlayerSceneInfoPBComponent* release_scene_info_last_time();
  ::PlayerSceneInfoPBComponent* mutable_scene_info_last_time();
  void set_allocated_scene_info_last_time(::PlayerSceneInfoPBComponent* value);
  void unsafe_arena_set_allocated_scene_info_last_time(::PlayerSceneInfoPBComponent* value);
  ::PlayerSceneInfoPBComponent* unsafe_arena_release_scene_info_last_time();

  private:
  const ::PlayerSceneInfoPBComponent& _internal_scene_info_last_time() const;
  ::PlayerSceneInfoPBComponent* _internal_mutable_scene_info_last_time();

  public:
  // @@protoc_insertion_point(class_scope:PlayerSceneContextPBComponent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 2,
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
    ::PlayerSceneInfoPBComponent* scene_info_;
    ::PlayerSceneInfoPBComponent* scene_info_last_time_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// PlayerSceneInfoPBComponent

// uint32 scene_confid = 1;
inline void PlayerSceneInfoPBComponent::clear_scene_confid() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.scene_confid_ = 0u;
}
inline ::uint32_t PlayerSceneInfoPBComponent::scene_confid() const {
  // @@protoc_insertion_point(field_get:PlayerSceneInfoPBComponent.scene_confid)
  return _internal_scene_confid();
}
inline void PlayerSceneInfoPBComponent::set_scene_confid(::uint32_t value) {
  _internal_set_scene_confid(value);
  // @@protoc_insertion_point(field_set:PlayerSceneInfoPBComponent.scene_confid)
}
inline ::uint32_t PlayerSceneInfoPBComponent::_internal_scene_confid() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.scene_confid_;
}
inline void PlayerSceneInfoPBComponent::_internal_set_scene_confid(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.scene_confid_ = value;
}

// uint32 guid = 2;
inline void PlayerSceneInfoPBComponent::clear_guid() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.guid_ = 0u;
}
inline ::uint32_t PlayerSceneInfoPBComponent::guid() const {
  // @@protoc_insertion_point(field_get:PlayerSceneInfoPBComponent.guid)
  return _internal_guid();
}
inline void PlayerSceneInfoPBComponent::set_guid(::uint32_t value) {
  _internal_set_guid(value);
  // @@protoc_insertion_point(field_set:PlayerSceneInfoPBComponent.guid)
}
inline ::uint32_t PlayerSceneInfoPBComponent::_internal_guid() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.guid_;
}
inline void PlayerSceneInfoPBComponent::_internal_set_guid(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.guid_ = value;
}

// -------------------------------------------------------------------

// PlayerSceneContextPBComponent

// .PlayerSceneInfoPBComponent scene_info = 1;
inline bool PlayerSceneContextPBComponent::has_scene_info() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.scene_info_ != nullptr);
  return value;
}
inline void PlayerSceneContextPBComponent::clear_scene_info() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.scene_info_ != nullptr) _impl_.scene_info_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::PlayerSceneInfoPBComponent& PlayerSceneContextPBComponent::_internal_scene_info() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  const ::PlayerSceneInfoPBComponent* p = _impl_.scene_info_;
  return p != nullptr ? *p : reinterpret_cast<const ::PlayerSceneInfoPBComponent&>(::_PlayerSceneInfoPBComponent_default_instance_);
}
inline const ::PlayerSceneInfoPBComponent& PlayerSceneContextPBComponent::scene_info() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:PlayerSceneContextPBComponent.scene_info)
  return _internal_scene_info();
}
inline void PlayerSceneContextPBComponent::unsafe_arena_set_allocated_scene_info(::PlayerSceneInfoPBComponent* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_);
  }
  _impl_.scene_info_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:PlayerSceneContextPBComponent.scene_info)
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::release_scene_info() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::PlayerSceneInfoPBComponent* released = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
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
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::unsafe_arena_release_scene_info() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:PlayerSceneContextPBComponent.scene_info)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::PlayerSceneInfoPBComponent* temp = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
  return temp;
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::_internal_mutable_scene_info() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.scene_info_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::PlayerSceneInfoPBComponent>(GetArena());
    _impl_.scene_info_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(p);
  }
  return _impl_.scene_info_;
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::mutable_scene_info() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::PlayerSceneInfoPBComponent* _msg = _internal_mutable_scene_info();
  // @@protoc_insertion_point(field_mutable:PlayerSceneContextPBComponent.scene_info)
  return _msg;
}
inline void PlayerSceneContextPBComponent::set_allocated_scene_info(::PlayerSceneInfoPBComponent* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (message_arena == nullptr) {
    delete (_impl_.scene_info_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }

  _impl_.scene_info_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(value);
  // @@protoc_insertion_point(field_set_allocated:PlayerSceneContextPBComponent.scene_info)
}

// .PlayerSceneInfoPBComponent scene_info_last_time = 2;
inline bool PlayerSceneContextPBComponent::has_scene_info_last_time() const {
  bool value = (_impl_._has_bits_[0] & 0x00000002u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.scene_info_last_time_ != nullptr);
  return value;
}
inline void PlayerSceneContextPBComponent::clear_scene_info_last_time() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.scene_info_last_time_ != nullptr) _impl_.scene_info_last_time_->Clear();
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline const ::PlayerSceneInfoPBComponent& PlayerSceneContextPBComponent::_internal_scene_info_last_time() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  const ::PlayerSceneInfoPBComponent* p = _impl_.scene_info_last_time_;
  return p != nullptr ? *p : reinterpret_cast<const ::PlayerSceneInfoPBComponent&>(::_PlayerSceneInfoPBComponent_default_instance_);
}
inline const ::PlayerSceneInfoPBComponent& PlayerSceneContextPBComponent::scene_info_last_time() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:PlayerSceneContextPBComponent.scene_info_last_time)
  return _internal_scene_info_last_time();
}
inline void PlayerSceneContextPBComponent::unsafe_arena_set_allocated_scene_info_last_time(::PlayerSceneInfoPBComponent* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_last_time_);
  }
  _impl_.scene_info_last_time_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:PlayerSceneContextPBComponent.scene_info_last_time)
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::release_scene_info_last_time() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::PlayerSceneInfoPBComponent* released = _impl_.scene_info_last_time_;
  _impl_.scene_info_last_time_ = nullptr;
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
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::unsafe_arena_release_scene_info_last_time() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:PlayerSceneContextPBComponent.scene_info_last_time)

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::PlayerSceneInfoPBComponent* temp = _impl_.scene_info_last_time_;
  _impl_.scene_info_last_time_ = nullptr;
  return temp;
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::_internal_mutable_scene_info_last_time() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.scene_info_last_time_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::PlayerSceneInfoPBComponent>(GetArena());
    _impl_.scene_info_last_time_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(p);
  }
  return _impl_.scene_info_last_time_;
}
inline ::PlayerSceneInfoPBComponent* PlayerSceneContextPBComponent::mutable_scene_info_last_time() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000002u;
  ::PlayerSceneInfoPBComponent* _msg = _internal_mutable_scene_info_last_time();
  // @@protoc_insertion_point(field_mutable:PlayerSceneContextPBComponent.scene_info_last_time)
  return _msg;
}
inline void PlayerSceneContextPBComponent::set_allocated_scene_info_last_time(::PlayerSceneInfoPBComponent* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (message_arena == nullptr) {
    delete (_impl_.scene_info_last_time_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }

  _impl_.scene_info_last_time_ = reinterpret_cast<::PlayerSceneInfoPBComponent*>(value);
  // @@protoc_insertion_point(field_set_allocated:PlayerSceneContextPBComponent.scene_info_last_time)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_logic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto_2epb_2eh