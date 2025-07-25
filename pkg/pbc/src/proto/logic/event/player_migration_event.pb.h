// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/player_migration_event.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto_2epb_2eh
#define proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto_2epb_2eh

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
#include "proto/logic/component/scene_comp.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto;
class PlayerMigrationPbEvent;
struct PlayerMigrationPbEventDefaultTypeInternal;
extern PlayerMigrationPbEventDefaultTypeInternal _PlayerMigrationPbEvent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class PlayerMigrationPbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerMigrationPbEvent) */ {
 public:
  inline PlayerMigrationPbEvent() : PlayerMigrationPbEvent(nullptr) {}
  ~PlayerMigrationPbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(PlayerMigrationPbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(PlayerMigrationPbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerMigrationPbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerMigrationPbEvent(const PlayerMigrationPbEvent& from) : PlayerMigrationPbEvent(nullptr, from) {}
  inline PlayerMigrationPbEvent(PlayerMigrationPbEvent&& from) noexcept
      : PlayerMigrationPbEvent(nullptr, std::move(from)) {}
  inline PlayerMigrationPbEvent& operator=(const PlayerMigrationPbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerMigrationPbEvent& operator=(PlayerMigrationPbEvent&& from) noexcept {
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
  static const PlayerMigrationPbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerMigrationPbEvent* internal_default_instance() {
    return reinterpret_cast<const PlayerMigrationPbEvent*>(
        &_PlayerMigrationPbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(PlayerMigrationPbEvent& a, PlayerMigrationPbEvent& b) { a.Swap(&b); }
  inline void Swap(PlayerMigrationPbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(PlayerMigrationPbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerMigrationPbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<PlayerMigrationPbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerMigrationPbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerMigrationPbEvent& from) { PlayerMigrationPbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerMigrationPbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "PlayerMigrationPbEvent"; }

 protected:
  explicit PlayerMigrationPbEvent(::google::protobuf::Arena* arena);
  PlayerMigrationPbEvent(::google::protobuf::Arena* arena, const PlayerMigrationPbEvent& from);
  PlayerMigrationPbEvent(::google::protobuf::Arena* arena, PlayerMigrationPbEvent&& from) noexcept
      : PlayerMigrationPbEvent(arena) {
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
    kSerializedPlayerDataFieldNumber = 4,
    kSceneInfoFieldNumber = 9,
    kPlayerIdFieldNumber = 1,
    kSourceSceneIdFieldNumber = 2,
    kTargetSceneIdFieldNumber = 3,
    kFromZoneFieldNumber = 5,
    kToZoneFieldNumber = 6,
    kTimestampFieldNumber = 7,
    kCentreNodeIdFieldNumber = 8,
  };
  // bytes serialized_player_data = 4;
  void clear_serialized_player_data() ;
  const std::string& serialized_player_data() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_serialized_player_data(Arg_&& arg, Args_... args);
  std::string* mutable_serialized_player_data();
  PROTOBUF_NODISCARD std::string* release_serialized_player_data();
  void set_allocated_serialized_player_data(std::string* value);

  private:
  const std::string& _internal_serialized_player_data() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_serialized_player_data(
      const std::string& value);
  std::string* _internal_mutable_serialized_player_data();

  public:
  // .ChangeSceneInfoPBComponent scene_info = 9;
  bool has_scene_info() const;
  void clear_scene_info() ;
  const ::ChangeSceneInfoPBComponent& scene_info() const;
  PROTOBUF_NODISCARD ::ChangeSceneInfoPBComponent* release_scene_info();
  ::ChangeSceneInfoPBComponent* mutable_scene_info();
  void set_allocated_scene_info(::ChangeSceneInfoPBComponent* value);
  void unsafe_arena_set_allocated_scene_info(::ChangeSceneInfoPBComponent* value);
  ::ChangeSceneInfoPBComponent* unsafe_arena_release_scene_info();

  private:
  const ::ChangeSceneInfoPBComponent& _internal_scene_info() const;
  ::ChangeSceneInfoPBComponent* _internal_mutable_scene_info();

  public:
  // uint64 player_id = 1;
  void clear_player_id() ;
  ::uint64_t player_id() const;
  void set_player_id(::uint64_t value);

  private:
  ::uint64_t _internal_player_id() const;
  void _internal_set_player_id(::uint64_t value);

  public:
  // uint64 source_scene_id = 2;
  void clear_source_scene_id() ;
  ::uint64_t source_scene_id() const;
  void set_source_scene_id(::uint64_t value);

  private:
  ::uint64_t _internal_source_scene_id() const;
  void _internal_set_source_scene_id(::uint64_t value);

  public:
  // uint64 target_scene_id = 3;
  void clear_target_scene_id() ;
  ::uint64_t target_scene_id() const;
  void set_target_scene_id(::uint64_t value);

  private:
  ::uint64_t _internal_target_scene_id() const;
  void _internal_set_target_scene_id(::uint64_t value);

  public:
  // uint32 from_zone = 5;
  void clear_from_zone() ;
  ::uint32_t from_zone() const;
  void set_from_zone(::uint32_t value);

  private:
  ::uint32_t _internal_from_zone() const;
  void _internal_set_from_zone(::uint32_t value);

  public:
  // uint32 to_zone = 6;
  void clear_to_zone() ;
  ::uint32_t to_zone() const;
  void set_to_zone(::uint32_t value);

  private:
  ::uint32_t _internal_to_zone() const;
  void _internal_set_to_zone(::uint32_t value);

  public:
  // int64 timestamp = 7;
  void clear_timestamp() ;
  ::int64_t timestamp() const;
  void set_timestamp(::int64_t value);

  private:
  ::int64_t _internal_timestamp() const;
  void _internal_set_timestamp(::int64_t value);

  public:
  // uint32 centre_node_id = 8;
  void clear_centre_node_id() ;
  ::uint32_t centre_node_id() const;
  void set_centre_node_id(::uint32_t value);

  private:
  ::uint32_t _internal_centre_node_id() const;
  void _internal_set_centre_node_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:PlayerMigrationPbEvent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      4, 9, 1,
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
                          const PlayerMigrationPbEvent& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::ArenaStringPtr serialized_player_data_;
    ::ChangeSceneInfoPBComponent* scene_info_;
    ::uint64_t player_id_;
    ::uint64_t source_scene_id_;
    ::uint64_t target_scene_id_;
    ::uint32_t from_zone_;
    ::uint32_t to_zone_;
    ::int64_t timestamp_;
    ::uint32_t centre_node_id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// PlayerMigrationPbEvent

// uint64 player_id = 1;
inline void PlayerMigrationPbEvent::clear_player_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.player_id_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerMigrationPbEvent::player_id() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.player_id)
  return _internal_player_id();
}
inline void PlayerMigrationPbEvent::set_player_id(::uint64_t value) {
  _internal_set_player_id(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.player_id)
}
inline ::uint64_t PlayerMigrationPbEvent::_internal_player_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.player_id_;
}
inline void PlayerMigrationPbEvent::_internal_set_player_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.player_id_ = value;
}

// uint64 source_scene_id = 2;
inline void PlayerMigrationPbEvent::clear_source_scene_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_scene_id_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerMigrationPbEvent::source_scene_id() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.source_scene_id)
  return _internal_source_scene_id();
}
inline void PlayerMigrationPbEvent::set_source_scene_id(::uint64_t value) {
  _internal_set_source_scene_id(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.source_scene_id)
}
inline ::uint64_t PlayerMigrationPbEvent::_internal_source_scene_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.source_scene_id_;
}
inline void PlayerMigrationPbEvent::_internal_set_source_scene_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.source_scene_id_ = value;
}

// uint64 target_scene_id = 3;
inline void PlayerMigrationPbEvent::clear_target_scene_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_scene_id_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerMigrationPbEvent::target_scene_id() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.target_scene_id)
  return _internal_target_scene_id();
}
inline void PlayerMigrationPbEvent::set_target_scene_id(::uint64_t value) {
  _internal_set_target_scene_id(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.target_scene_id)
}
inline ::uint64_t PlayerMigrationPbEvent::_internal_target_scene_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.target_scene_id_;
}
inline void PlayerMigrationPbEvent::_internal_set_target_scene_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.target_scene_id_ = value;
}

// bytes serialized_player_data = 4;
inline void PlayerMigrationPbEvent::clear_serialized_player_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.serialized_player_data_.ClearToEmpty();
}
inline const std::string& PlayerMigrationPbEvent::serialized_player_data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.serialized_player_data)
  return _internal_serialized_player_data();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void PlayerMigrationPbEvent::set_serialized_player_data(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.serialized_player_data_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.serialized_player_data)
}
inline std::string* PlayerMigrationPbEvent::mutable_serialized_player_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_serialized_player_data();
  // @@protoc_insertion_point(field_mutable:PlayerMigrationPbEvent.serialized_player_data)
  return _s;
}
inline const std::string& PlayerMigrationPbEvent::_internal_serialized_player_data() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.serialized_player_data_.Get();
}
inline void PlayerMigrationPbEvent::_internal_set_serialized_player_data(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.serialized_player_data_.Set(value, GetArena());
}
inline std::string* PlayerMigrationPbEvent::_internal_mutable_serialized_player_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.serialized_player_data_.Mutable( GetArena());
}
inline std::string* PlayerMigrationPbEvent::release_serialized_player_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:PlayerMigrationPbEvent.serialized_player_data)
  return _impl_.serialized_player_data_.Release();
}
inline void PlayerMigrationPbEvent::set_allocated_serialized_player_data(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.serialized_player_data_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.serialized_player_data_.IsDefault()) {
    _impl_.serialized_player_data_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:PlayerMigrationPbEvent.serialized_player_data)
}

// uint32 from_zone = 5;
inline void PlayerMigrationPbEvent::clear_from_zone() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.from_zone_ = 0u;
}
inline ::uint32_t PlayerMigrationPbEvent::from_zone() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.from_zone)
  return _internal_from_zone();
}
inline void PlayerMigrationPbEvent::set_from_zone(::uint32_t value) {
  _internal_set_from_zone(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.from_zone)
}
inline ::uint32_t PlayerMigrationPbEvent::_internal_from_zone() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.from_zone_;
}
inline void PlayerMigrationPbEvent::_internal_set_from_zone(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.from_zone_ = value;
}

// uint32 to_zone = 6;
inline void PlayerMigrationPbEvent::clear_to_zone() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.to_zone_ = 0u;
}
inline ::uint32_t PlayerMigrationPbEvent::to_zone() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.to_zone)
  return _internal_to_zone();
}
inline void PlayerMigrationPbEvent::set_to_zone(::uint32_t value) {
  _internal_set_to_zone(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.to_zone)
}
inline ::uint32_t PlayerMigrationPbEvent::_internal_to_zone() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.to_zone_;
}
inline void PlayerMigrationPbEvent::_internal_set_to_zone(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.to_zone_ = value;
}

// int64 timestamp = 7;
inline void PlayerMigrationPbEvent::clear_timestamp() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.timestamp_ = ::int64_t{0};
}
inline ::int64_t PlayerMigrationPbEvent::timestamp() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.timestamp)
  return _internal_timestamp();
}
inline void PlayerMigrationPbEvent::set_timestamp(::int64_t value) {
  _internal_set_timestamp(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.timestamp)
}
inline ::int64_t PlayerMigrationPbEvent::_internal_timestamp() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.timestamp_;
}
inline void PlayerMigrationPbEvent::_internal_set_timestamp(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.timestamp_ = value;
}

// uint32 centre_node_id = 8;
inline void PlayerMigrationPbEvent::clear_centre_node_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.centre_node_id_ = 0u;
}
inline ::uint32_t PlayerMigrationPbEvent::centre_node_id() const {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.centre_node_id)
  return _internal_centre_node_id();
}
inline void PlayerMigrationPbEvent::set_centre_node_id(::uint32_t value) {
  _internal_set_centre_node_id(value);
  // @@protoc_insertion_point(field_set:PlayerMigrationPbEvent.centre_node_id)
}
inline ::uint32_t PlayerMigrationPbEvent::_internal_centre_node_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.centre_node_id_;
}
inline void PlayerMigrationPbEvent::_internal_set_centre_node_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.centre_node_id_ = value;
}

// .ChangeSceneInfoPBComponent scene_info = 9;
inline bool PlayerMigrationPbEvent::has_scene_info() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.scene_info_ != nullptr);
  return value;
}
inline const ::ChangeSceneInfoPBComponent& PlayerMigrationPbEvent::_internal_scene_info() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::ChangeSceneInfoPBComponent* p = _impl_.scene_info_;
  return p != nullptr ? *p : reinterpret_cast<const ::ChangeSceneInfoPBComponent&>(::_ChangeSceneInfoPBComponent_default_instance_);
}
inline const ::ChangeSceneInfoPBComponent& PlayerMigrationPbEvent::scene_info() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:PlayerMigrationPbEvent.scene_info)
  return _internal_scene_info();
}
inline void PlayerMigrationPbEvent::unsafe_arena_set_allocated_scene_info(::ChangeSceneInfoPBComponent* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_);
  }
  _impl_.scene_info_ = reinterpret_cast<::ChangeSceneInfoPBComponent*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:PlayerMigrationPbEvent.scene_info)
}
inline ::ChangeSceneInfoPBComponent* PlayerMigrationPbEvent::release_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::ChangeSceneInfoPBComponent* released = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
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
inline ::ChangeSceneInfoPBComponent* PlayerMigrationPbEvent::unsafe_arena_release_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:PlayerMigrationPbEvent.scene_info)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::ChangeSceneInfoPBComponent* temp = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
  return temp;
}
inline ::ChangeSceneInfoPBComponent* PlayerMigrationPbEvent::_internal_mutable_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.scene_info_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::ChangeSceneInfoPBComponent>(GetArena());
    _impl_.scene_info_ = reinterpret_cast<::ChangeSceneInfoPBComponent*>(p);
  }
  return _impl_.scene_info_;
}
inline ::ChangeSceneInfoPBComponent* PlayerMigrationPbEvent::mutable_scene_info() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::ChangeSceneInfoPBComponent* _msg = _internal_mutable_scene_info();
  // @@protoc_insertion_point(field_mutable:PlayerMigrationPbEvent.scene_info)
  return _msg;
}
inline void PlayerMigrationPbEvent::set_allocated_scene_info(::ChangeSceneInfoPBComponent* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_);
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

  _impl_.scene_info_ = reinterpret_cast<::ChangeSceneInfoPBComponent*>(value);
  // @@protoc_insertion_point(field_set_allocated:PlayerMigrationPbEvent.scene_info)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fevent_2fplayer_5fmigration_5fevent_2eproto_2epb_2eh
