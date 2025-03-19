// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/component/player_async_comp.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto;
class PlayerGameNodeEnteryInfoPBComponent;
struct PlayerGameNodeEnteryInfoPBComponentDefaultTypeInternal;
extern PlayerGameNodeEnteryInfoPBComponentDefaultTypeInternal _PlayerGameNodeEnteryInfoPBComponent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class PlayerGameNodeEnteryInfoPBComponent final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:PlayerGameNodeEnteryInfoPBComponent) */ {
 public:
  inline PlayerGameNodeEnteryInfoPBComponent() : PlayerGameNodeEnteryInfoPBComponent(nullptr) {}
  ~PlayerGameNodeEnteryInfoPBComponent() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlayerGameNodeEnteryInfoPBComponent(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlayerGameNodeEnteryInfoPBComponent(const PlayerGameNodeEnteryInfoPBComponent& from) : PlayerGameNodeEnteryInfoPBComponent(nullptr, from) {}
  inline PlayerGameNodeEnteryInfoPBComponent(PlayerGameNodeEnteryInfoPBComponent&& from) noexcept
      : PlayerGameNodeEnteryInfoPBComponent(nullptr, std::move(from)) {}
  inline PlayerGameNodeEnteryInfoPBComponent& operator=(const PlayerGameNodeEnteryInfoPBComponent& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlayerGameNodeEnteryInfoPBComponent& operator=(PlayerGameNodeEnteryInfoPBComponent&& from) noexcept {
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
  static const PlayerGameNodeEnteryInfoPBComponent& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlayerGameNodeEnteryInfoPBComponent* internal_default_instance() {
    return reinterpret_cast<const PlayerGameNodeEnteryInfoPBComponent*>(
        &_PlayerGameNodeEnteryInfoPBComponent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(PlayerGameNodeEnteryInfoPBComponent& a, PlayerGameNodeEnteryInfoPBComponent& b) { a.Swap(&b); }
  inline void Swap(PlayerGameNodeEnteryInfoPBComponent* other) {
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
  void UnsafeArenaSwap(PlayerGameNodeEnteryInfoPBComponent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlayerGameNodeEnteryInfoPBComponent* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<PlayerGameNodeEnteryInfoPBComponent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlayerGameNodeEnteryInfoPBComponent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlayerGameNodeEnteryInfoPBComponent& from) { PlayerGameNodeEnteryInfoPBComponent::MergeImpl(*this, from); }

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
  void InternalSwap(PlayerGameNodeEnteryInfoPBComponent* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "PlayerGameNodeEnteryInfoPBComponent"; }

 protected:
  explicit PlayerGameNodeEnteryInfoPBComponent(::google::protobuf::Arena* arena);
  PlayerGameNodeEnteryInfoPBComponent(::google::protobuf::Arena* arena, const PlayerGameNodeEnteryInfoPBComponent& from);
  PlayerGameNodeEnteryInfoPBComponent(::google::protobuf::Arena* arena, PlayerGameNodeEnteryInfoPBComponent&& from) noexcept
      : PlayerGameNodeEnteryInfoPBComponent(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kCentreNodeIdFieldNumber = 1,
  };
  // uint32 centre_node_id = 1;
  void clear_centre_node_id() ;
  ::uint32_t centre_node_id() const;
  void set_centre_node_id(::uint32_t value);

  private:
  ::uint32_t _internal_centre_node_id() const;
  void _internal_set_centre_node_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:PlayerGameNodeEnteryInfoPBComponent)
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
    ::uint32_t centre_node_id_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// PlayerGameNodeEnteryInfoPBComponent

// uint32 centre_node_id = 1;
inline void PlayerGameNodeEnteryInfoPBComponent::clear_centre_node_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.centre_node_id_ = 0u;
}
inline ::uint32_t PlayerGameNodeEnteryInfoPBComponent::centre_node_id() const {
  // @@protoc_insertion_point(field_get:PlayerGameNodeEnteryInfoPBComponent.centre_node_id)
  return _internal_centre_node_id();
}
inline void PlayerGameNodeEnteryInfoPBComponent::set_centre_node_id(::uint32_t value) {
  _internal_set_centre_node_id(value);
  // @@protoc_insertion_point(field_set:PlayerGameNodeEnteryInfoPBComponent.centre_node_id)
}
inline ::uint32_t PlayerGameNodeEnteryInfoPBComponent::_internal_centre_node_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.centre_node_id_;
}
inline void PlayerGameNodeEnteryInfoPBComponent::_internal_set_centre_node_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.centre_node_id_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_proto_2flogic_2fcomponent_2fplayer_5fasync_5fcomp_2eproto_2epb_2eh
