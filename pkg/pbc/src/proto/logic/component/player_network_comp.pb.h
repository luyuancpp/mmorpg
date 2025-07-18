// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/player_network_comp.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_2epb_2eh
#define proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_2epb_2eh

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
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto;
class PlayerSessionSnapshotPBComp;
struct PlayerSessionSnapshotPBCompDefaultTypeInternal;
extern PlayerSessionSnapshotPBCompDefaultTypeInternal _PlayerSessionSnapshotPBComp_default_instance_;
class PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse;
struct PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal;
extern PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal _PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          ::uint32_t, ::uint32_t,
          ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
          ::google::protobuf::internal::WireFormatLite::TYPE_UINT32> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      ::uint32_t, ::uint32_t,
      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>;
  PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse*>(
        &_PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse_default_instance_);
  }


 private:
  friend class ::google::protobuf::MessageLite;
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto;

  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      0, 2>
      _table_;

  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;
};
// -------------------------------------------------------------------

class PlayerSessionSnapshotPBComp final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerSessionSnapshotPBComp) */ {
 public:
  inline PlayerSessionSnapshotPBComp() : PlayerSessionSnapshotPBComp(nullptr) {}
  ~PlayerSessionSnapshotPBComp() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(PlayerSessionSnapshotPBComp* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(PlayerSessionSnapshotPBComp));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBComp(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerSessionSnapshotPBComp(const PlayerSessionSnapshotPBComp& from) : PlayerSessionSnapshotPBComp(nullptr, from) {}
  inline PlayerSessionSnapshotPBComp(PlayerSessionSnapshotPBComp&& from) noexcept
      : PlayerSessionSnapshotPBComp(nullptr, std::move(from)) {}
  inline PlayerSessionSnapshotPBComp& operator=(const PlayerSessionSnapshotPBComp& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerSessionSnapshotPBComp& operator=(PlayerSessionSnapshotPBComp&& from) noexcept {
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
  static const PlayerSessionSnapshotPBComp& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerSessionSnapshotPBComp* internal_default_instance() {
    return reinterpret_cast<const PlayerSessionSnapshotPBComp*>(
        &_PlayerSessionSnapshotPBComp_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(PlayerSessionSnapshotPBComp& a, PlayerSessionSnapshotPBComp& b) { a.Swap(&b); }
  inline void Swap(PlayerSessionSnapshotPBComp* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(PlayerSessionSnapshotPBComp* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerSessionSnapshotPBComp* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<PlayerSessionSnapshotPBComp>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerSessionSnapshotPBComp& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerSessionSnapshotPBComp& from) { PlayerSessionSnapshotPBComp::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerSessionSnapshotPBComp* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "PlayerSessionSnapshotPBComp"; }

 protected:
  explicit PlayerSessionSnapshotPBComp(::google::protobuf::Arena* arena);
  PlayerSessionSnapshotPBComp(::google::protobuf::Arena* arena, const PlayerSessionSnapshotPBComp& from);
  PlayerSessionSnapshotPBComp(::google::protobuf::Arena* arena, PlayerSessionSnapshotPBComp&& from) noexcept
      : PlayerSessionSnapshotPBComp(arena) {
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
    kNodeIdFieldNumber = 3,
    kLoginTokenFieldNumber = 4,
    kPlayerIdFieldNumber = 1,
    kGateSessionIdFieldNumber = 2,
  };
  // map<uint32, uint32> node_id = 3;
  int node_id_size() const;
  private:
  int _internal_node_id_size() const;

  public:
  void clear_node_id() ;
  const ::google::protobuf::Map<::uint32_t, ::uint32_t>& node_id() const;
  ::google::protobuf::Map<::uint32_t, ::uint32_t>* mutable_node_id();

  private:
  const ::google::protobuf::Map<::uint32_t, ::uint32_t>& _internal_node_id() const;
  ::google::protobuf::Map<::uint32_t, ::uint32_t>* _internal_mutable_node_id();

  public:
  // string login_token = 4;
  void clear_login_token() ;
  const std::string& login_token() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_login_token(Arg_&& arg, Args_... args);
  std::string* mutable_login_token();
  PROTOBUF_NODISCARD std::string* release_login_token();
  void set_allocated_login_token(std::string* value);

  private:
  const std::string& _internal_login_token() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_login_token(
      const std::string& value);
  std::string* _internal_mutable_login_token();

  public:
  // uint64 player_id = 1;
  void clear_player_id() ;
  ::uint64_t player_id() const;
  void set_player_id(::uint64_t value);

  private:
  ::uint64_t _internal_player_id() const;
  void _internal_set_player_id(::uint64_t value);

  public:
  // uint64 gate_session_id = 2;
  void clear_gate_session_id() ;
  ::uint64_t gate_session_id() const;
  void set_gate_session_id(::uint64_t value);

  private:
  ::uint64_t _internal_gate_session_id() const;
  void _internal_set_gate_session_id(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:PlayerSessionSnapshotPBComp)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 4, 1,
      47, 2>
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
                          const PlayerSessionSnapshotPBComp& from_msg);
    ::google::protobuf::internal::MapField<PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, ::uint32_t, ::uint32_t,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>
        node_id_;
    ::google::protobuf::internal::ArenaStringPtr login_token_;
    ::uint64_t player_id_;
    ::uint64_t gate_session_id_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// -------------------------------------------------------------------

// PlayerSessionSnapshotPBComp

// uint64 player_id = 1;
inline void PlayerSessionSnapshotPBComp::clear_player_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.player_id_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerSessionSnapshotPBComp::player_id() const {
  // @@protoc_insertion_point(field_get:PlayerSessionSnapshotPBComp.player_id)
  return _internal_player_id();
}
inline void PlayerSessionSnapshotPBComp::set_player_id(::uint64_t value) {
  _internal_set_player_id(value);
  // @@protoc_insertion_point(field_set:PlayerSessionSnapshotPBComp.player_id)
}
inline ::uint64_t PlayerSessionSnapshotPBComp::_internal_player_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.player_id_;
}
inline void PlayerSessionSnapshotPBComp::_internal_set_player_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.player_id_ = value;
}

// uint64 gate_session_id = 2;
inline void PlayerSessionSnapshotPBComp::clear_gate_session_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.gate_session_id_ = ::uint64_t{0u};
}
inline ::uint64_t PlayerSessionSnapshotPBComp::gate_session_id() const {
  // @@protoc_insertion_point(field_get:PlayerSessionSnapshotPBComp.gate_session_id)
  return _internal_gate_session_id();
}
inline void PlayerSessionSnapshotPBComp::set_gate_session_id(::uint64_t value) {
  _internal_set_gate_session_id(value);
  // @@protoc_insertion_point(field_set:PlayerSessionSnapshotPBComp.gate_session_id)
}
inline ::uint64_t PlayerSessionSnapshotPBComp::_internal_gate_session_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.gate_session_id_;
}
inline void PlayerSessionSnapshotPBComp::_internal_set_gate_session_id(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.gate_session_id_ = value;
}

// map<uint32, uint32> node_id = 3;
inline int PlayerSessionSnapshotPBComp::_internal_node_id_size() const {
  return _internal_node_id().size();
}
inline int PlayerSessionSnapshotPBComp::node_id_size() const {
  return _internal_node_id_size();
}
inline void PlayerSessionSnapshotPBComp::clear_node_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_id_.Clear();
}
inline const ::google::protobuf::Map<::uint32_t, ::uint32_t>& PlayerSessionSnapshotPBComp::_internal_node_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.node_id_.GetMap();
}
inline const ::google::protobuf::Map<::uint32_t, ::uint32_t>& PlayerSessionSnapshotPBComp::node_id() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:PlayerSessionSnapshotPBComp.node_id)
  return _internal_node_id();
}
inline ::google::protobuf::Map<::uint32_t, ::uint32_t>* PlayerSessionSnapshotPBComp::_internal_mutable_node_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.node_id_.MutableMap();
}
inline ::google::protobuf::Map<::uint32_t, ::uint32_t>* PlayerSessionSnapshotPBComp::mutable_node_id() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:PlayerSessionSnapshotPBComp.node_id)
  return _internal_mutable_node_id();
}

// string login_token = 4;
inline void PlayerSessionSnapshotPBComp::clear_login_token() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.login_token_.ClearToEmpty();
}
inline const std::string& PlayerSessionSnapshotPBComp::login_token() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:PlayerSessionSnapshotPBComp.login_token)
  return _internal_login_token();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void PlayerSessionSnapshotPBComp::set_login_token(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.login_token_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:PlayerSessionSnapshotPBComp.login_token)
}
inline std::string* PlayerSessionSnapshotPBComp::mutable_login_token() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_login_token();
  // @@protoc_insertion_point(field_mutable:PlayerSessionSnapshotPBComp.login_token)
  return _s;
}
inline const std::string& PlayerSessionSnapshotPBComp::_internal_login_token() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.login_token_.Get();
}
inline void PlayerSessionSnapshotPBComp::_internal_set_login_token(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.login_token_.Set(value, GetArena());
}
inline std::string* PlayerSessionSnapshotPBComp::_internal_mutable_login_token() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.login_token_.Mutable( GetArena());
}
inline std::string* PlayerSessionSnapshotPBComp::release_login_token() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:PlayerSessionSnapshotPBComp.login_token)
  return _impl_.login_token_.Release();
}
inline void PlayerSessionSnapshotPBComp::set_allocated_login_token(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.login_token_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.login_token_.IsDefault()) {
    _impl_.login_token_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:PlayerSessionSnapshotPBComp.login_token)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_2epb_2eh
