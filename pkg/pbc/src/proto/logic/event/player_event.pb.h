// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/event/player_event.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_logic_2fevent_2fplayer_5fevent_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_logic_2fevent_2fplayer_5fevent_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_logic_2fevent_2fplayer_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_logic_2fevent_2fplayer_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_logic_2fevent_2fplayer_5fevent_2eproto;
class InitializePlayerComponentsEvent;
struct InitializePlayerComponentsEventDefaultTypeInternal;
extern InitializePlayerComponentsEventDefaultTypeInternal _InitializePlayerComponentsEvent_default_instance_;
class PlayerUpgradeEvent;
struct PlayerUpgradeEventDefaultTypeInternal;
extern PlayerUpgradeEventDefaultTypeInternal _PlayerUpgradeEvent_default_instance_;
class RegisterPlayerEvent;
struct RegisterPlayerEventDefaultTypeInternal;
extern RegisterPlayerEventDefaultTypeInternal _RegisterPlayerEvent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class RegisterPlayerEvent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:RegisterPlayerEvent) */ {
 public:
  inline RegisterPlayerEvent() : RegisterPlayerEvent(nullptr) {}
  ~RegisterPlayerEvent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR RegisterPlayerEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline RegisterPlayerEvent(const RegisterPlayerEvent& from) : RegisterPlayerEvent(nullptr, from) {}
  inline RegisterPlayerEvent(RegisterPlayerEvent&& from) noexcept
      : RegisterPlayerEvent(nullptr, std::move(from)) {}
  inline RegisterPlayerEvent& operator=(const RegisterPlayerEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline RegisterPlayerEvent& operator=(RegisterPlayerEvent&& from) noexcept {
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
  static const RegisterPlayerEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const RegisterPlayerEvent* internal_default_instance() {
    return reinterpret_cast<const RegisterPlayerEvent*>(
        &_RegisterPlayerEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(RegisterPlayerEvent& a, RegisterPlayerEvent& b) { a.Swap(&b); }
  inline void Swap(RegisterPlayerEvent* other) {
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
  void UnsafeArenaSwap(RegisterPlayerEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RegisterPlayerEvent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<RegisterPlayerEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const RegisterPlayerEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const RegisterPlayerEvent& from) { RegisterPlayerEvent::MergeImpl(*this, from); }

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
  void InternalSwap(RegisterPlayerEvent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "RegisterPlayerEvent"; }

 protected:
  explicit RegisterPlayerEvent(::google::protobuf::Arena* arena);
  RegisterPlayerEvent(::google::protobuf::Arena* arena, const RegisterPlayerEvent& from);
  RegisterPlayerEvent(::google::protobuf::Arena* arena, RegisterPlayerEvent&& from) noexcept
      : RegisterPlayerEvent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kEntityFieldNumber = 1,
  };
  // uint64 entity = 1;
  void clear_entity() ;
  ::uint64_t entity() const;
  void set_entity(::uint64_t value);

  private:
  ::uint64_t _internal_entity() const;
  void _internal_set_entity(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:RegisterPlayerEvent)
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
    ::uint64_t entity_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fevent_2fplayer_5fevent_2eproto;
};
// -------------------------------------------------------------------

class PlayerUpgradeEvent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerUpgradeEvent) */ {
 public:
  inline PlayerUpgradeEvent() : PlayerUpgradeEvent(nullptr) {}
  ~PlayerUpgradeEvent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerUpgradeEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerUpgradeEvent(const PlayerUpgradeEvent& from) : PlayerUpgradeEvent(nullptr, from) {}
  inline PlayerUpgradeEvent(PlayerUpgradeEvent&& from) noexcept
      : PlayerUpgradeEvent(nullptr, std::move(from)) {}
  inline PlayerUpgradeEvent& operator=(const PlayerUpgradeEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerUpgradeEvent& operator=(PlayerUpgradeEvent&& from) noexcept {
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
  static const PlayerUpgradeEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerUpgradeEvent* internal_default_instance() {
    return reinterpret_cast<const PlayerUpgradeEvent*>(
        &_PlayerUpgradeEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(PlayerUpgradeEvent& a, PlayerUpgradeEvent& b) { a.Swap(&b); }
  inline void Swap(PlayerUpgradeEvent* other) {
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
  void UnsafeArenaSwap(PlayerUpgradeEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerUpgradeEvent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<PlayerUpgradeEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerUpgradeEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerUpgradeEvent& from) { PlayerUpgradeEvent::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerUpgradeEvent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "PlayerUpgradeEvent"; }

 protected:
  explicit PlayerUpgradeEvent(::google::protobuf::Arena* arena);
  PlayerUpgradeEvent(::google::protobuf::Arena* arena, const PlayerUpgradeEvent& from);
  PlayerUpgradeEvent(::google::protobuf::Arena* arena, PlayerUpgradeEvent&& from) noexcept
      : PlayerUpgradeEvent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kEntityFieldNumber = 1,
    kNewLevelFieldNumber = 2,
  };
  // uint64 entity = 1;
  void clear_entity() ;
  ::uint64_t entity() const;
  void set_entity(::uint64_t value);

  private:
  ::uint64_t _internal_entity() const;
  void _internal_set_entity(::uint64_t value);

  public:
  // uint32 new_level = 2;
  void clear_new_level() ;
  ::uint32_t new_level() const;
  void set_new_level(::uint32_t value);

  private:
  ::uint32_t _internal_new_level() const;
  void _internal_set_new_level(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:PlayerUpgradeEvent)
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
    ::uint64_t entity_;
    ::uint32_t new_level_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fevent_2fplayer_5fevent_2eproto;
};
// -------------------------------------------------------------------

class InitializePlayerComponentsEvent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:InitializePlayerComponentsEvent) */ {
 public:
  inline InitializePlayerComponentsEvent() : InitializePlayerComponentsEvent(nullptr) {}
  ~InitializePlayerComponentsEvent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR InitializePlayerComponentsEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline InitializePlayerComponentsEvent(const InitializePlayerComponentsEvent& from) : InitializePlayerComponentsEvent(nullptr, from) {}
  inline InitializePlayerComponentsEvent(InitializePlayerComponentsEvent&& from) noexcept
      : InitializePlayerComponentsEvent(nullptr, std::move(from)) {}
  inline InitializePlayerComponentsEvent& operator=(const InitializePlayerComponentsEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline InitializePlayerComponentsEvent& operator=(InitializePlayerComponentsEvent&& from) noexcept {
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
  static const InitializePlayerComponentsEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const InitializePlayerComponentsEvent* internal_default_instance() {
    return reinterpret_cast<const InitializePlayerComponentsEvent*>(
        &_InitializePlayerComponentsEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(InitializePlayerComponentsEvent& a, InitializePlayerComponentsEvent& b) { a.Swap(&b); }
  inline void Swap(InitializePlayerComponentsEvent* other) {
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
  void UnsafeArenaSwap(InitializePlayerComponentsEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  InitializePlayerComponentsEvent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<InitializePlayerComponentsEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const InitializePlayerComponentsEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const InitializePlayerComponentsEvent& from) { InitializePlayerComponentsEvent::MergeImpl(*this, from); }

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
  void InternalSwap(InitializePlayerComponentsEvent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "InitializePlayerComponentsEvent"; }

 protected:
  explicit InitializePlayerComponentsEvent(::google::protobuf::Arena* arena);
  InitializePlayerComponentsEvent(::google::protobuf::Arena* arena, const InitializePlayerComponentsEvent& from);
  InitializePlayerComponentsEvent(::google::protobuf::Arena* arena, InitializePlayerComponentsEvent&& from) noexcept
      : InitializePlayerComponentsEvent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kEntityFieldNumber = 1,
  };
  // uint64 entity = 1;
  void clear_entity() ;
  ::uint64_t entity() const;
  void set_entity(::uint64_t value);

  private:
  ::uint64_t _internal_entity() const;
  void _internal_set_entity(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:InitializePlayerComponentsEvent)
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
    ::uint64_t entity_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_logic_2fevent_2fplayer_5fevent_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// RegisterPlayerEvent

// uint64 entity = 1;
inline void RegisterPlayerEvent::clear_entity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = ::uint64_t{0u};
}
inline ::uint64_t RegisterPlayerEvent::entity() const {
  // @@protoc_insertion_point(field_get:RegisterPlayerEvent.entity)
  return _internal_entity();
}
inline void RegisterPlayerEvent::set_entity(::uint64_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:RegisterPlayerEvent.entity)
}
inline ::uint64_t RegisterPlayerEvent::_internal_entity() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.entity_;
}
inline void RegisterPlayerEvent::_internal_set_entity(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = value;
}

// -------------------------------------------------------------------

// PlayerUpgradeEvent

// uint64 entity = 1;
inline void PlayerUpgradeEvent::clear_entity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerUpgradeEvent::entity() const {
  // @@protoc_insertion_point(field_get:PlayerUpgradeEvent.entity)
  return _internal_entity();
}
inline void PlayerUpgradeEvent::set_entity(::uint64_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:PlayerUpgradeEvent.entity)
}
inline ::uint64_t PlayerUpgradeEvent::_internal_entity() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.entity_;
}
inline void PlayerUpgradeEvent::_internal_set_entity(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = value;
}

// uint32 new_level = 2;
inline void PlayerUpgradeEvent::clear_new_level() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.new_level_ = 0u;
}
inline ::uint32_t PlayerUpgradeEvent::new_level() const {
  // @@protoc_insertion_point(field_get:PlayerUpgradeEvent.new_level)
  return _internal_new_level();
}
inline void PlayerUpgradeEvent::set_new_level(::uint32_t value) {
  _internal_set_new_level(value);
  // @@protoc_insertion_point(field_set:PlayerUpgradeEvent.new_level)
}
inline ::uint32_t PlayerUpgradeEvent::_internal_new_level() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.new_level_;
}
inline void PlayerUpgradeEvent::_internal_set_new_level(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.new_level_ = value;
}

// -------------------------------------------------------------------

// InitializePlayerComponentsEvent

// uint64 entity = 1;
inline void InitializePlayerComponentsEvent::clear_entity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = ::uint64_t{0u};
}
inline ::uint64_t InitializePlayerComponentsEvent::entity() const {
  // @@protoc_insertion_point(field_get:InitializePlayerComponentsEvent.entity)
  return _internal_entity();
}
inline void InitializePlayerComponentsEvent::set_entity(::uint64_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:InitializePlayerComponentsEvent.entity)
}
inline ::uint64_t InitializePlayerComponentsEvent::_internal_entity() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.entity_;
}
inline void InitializePlayerComponentsEvent::_internal_set_entity(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.entity_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_logic_2fevent_2fplayer_5fevent_2eproto_2epb_2eh
