// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/scene/game_player.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2fscene_2fgame_5fplayer_2eproto_2epb_2eh
#define proto_2fscene_2fgame_5fplayer_2eproto_2epb_2eh

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
#include "google/protobuf/generated_message_bases.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/service.h"
#include "google/protobuf/unknown_field_set.h"
#include "google/protobuf/empty.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2fscene_2fgame_5fplayer_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2fscene_2fgame_5fplayer_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2fscene_2fgame_5fplayer_2eproto;
class Centre2GsLoginRequest;
struct Centre2GsLoginRequestDefaultTypeInternal;
extern Centre2GsLoginRequestDefaultTypeInternal _Centre2GsLoginRequest_default_instance_;
class GameNodeExitGameRequest;
struct GameNodeExitGameRequestDefaultTypeInternal;
extern GameNodeExitGameRequestDefaultTypeInternal _GameNodeExitGameRequest_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class GameNodeExitGameRequest final
    : public ::google::protobuf::internal::ZeroFieldsBase
/* @@protoc_insertion_point(class_definition:GameNodeExitGameRequest) */ {
 public:
  inline GameNodeExitGameRequest() : GameNodeExitGameRequest(nullptr) {}

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(GameNodeExitGameRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(GameNodeExitGameRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR GameNodeExitGameRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline GameNodeExitGameRequest(const GameNodeExitGameRequest& from) : GameNodeExitGameRequest(nullptr, from) {}
  inline GameNodeExitGameRequest(GameNodeExitGameRequest&& from) noexcept
      : GameNodeExitGameRequest(nullptr, std::move(from)) {}
  inline GameNodeExitGameRequest& operator=(const GameNodeExitGameRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline GameNodeExitGameRequest& operator=(GameNodeExitGameRequest&& from) noexcept {
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
  static const GameNodeExitGameRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const GameNodeExitGameRequest* internal_default_instance() {
    return reinterpret_cast<const GameNodeExitGameRequest*>(
        &_GameNodeExitGameRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(GameNodeExitGameRequest& a, GameNodeExitGameRequest& b) { a.Swap(&b); }
  inline void Swap(GameNodeExitGameRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(GameNodeExitGameRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  GameNodeExitGameRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::internal::ZeroFieldsBase::DefaultConstruct<GameNodeExitGameRequest>(arena);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::CopyFrom;
  inline void CopyFrom(const GameNodeExitGameRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::CopyImpl(*this, from);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::MergeFrom;
  void MergeFrom(const GameNodeExitGameRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::MergeImpl(*this, from);
  }

  public:
  bool IsInitialized() const {
    return true;
  }
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "GameNodeExitGameRequest"; }

 protected:
  explicit GameNodeExitGameRequest(::google::protobuf::Arena* arena);
  GameNodeExitGameRequest(::google::protobuf::Arena* arena, const GameNodeExitGameRequest& from);
  GameNodeExitGameRequest(::google::protobuf::Arena* arena, GameNodeExitGameRequest&& from) noexcept
      : GameNodeExitGameRequest(arena) {
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
  // @@protoc_insertion_point(class_scope:GameNodeExitGameRequest)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 0, 0,
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
                          const GameNodeExitGameRequest& from_msg);
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  friend struct ::TableStruct_proto_2fscene_2fgame_5fplayer_2eproto;
};
// -------------------------------------------------------------------

class Centre2GsLoginRequest final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:Centre2GsLoginRequest) */ {
 public:
  inline Centre2GsLoginRequest() : Centre2GsLoginRequest(nullptr) {}
  ~Centre2GsLoginRequest() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(Centre2GsLoginRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(Centre2GsLoginRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR Centre2GsLoginRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline Centre2GsLoginRequest(const Centre2GsLoginRequest& from) : Centre2GsLoginRequest(nullptr, from) {}
  inline Centre2GsLoginRequest(Centre2GsLoginRequest&& from) noexcept
      : Centre2GsLoginRequest(nullptr, std::move(from)) {}
  inline Centre2GsLoginRequest& operator=(const Centre2GsLoginRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline Centre2GsLoginRequest& operator=(Centre2GsLoginRequest&& from) noexcept {
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
  static const Centre2GsLoginRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const Centre2GsLoginRequest* internal_default_instance() {
    return reinterpret_cast<const Centre2GsLoginRequest*>(
        &_Centre2GsLoginRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(Centre2GsLoginRequest& a, Centre2GsLoginRequest& b) { a.Swap(&b); }
  inline void Swap(Centre2GsLoginRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(Centre2GsLoginRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  Centre2GsLoginRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<Centre2GsLoginRequest>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const Centre2GsLoginRequest& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const Centre2GsLoginRequest& from) { Centre2GsLoginRequest::MergeImpl(*this, from); }

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
  void InternalSwap(Centre2GsLoginRequest* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "Centre2GsLoginRequest"; }

 protected:
  explicit Centre2GsLoginRequest(::google::protobuf::Arena* arena);
  Centre2GsLoginRequest(::google::protobuf::Arena* arena, const Centre2GsLoginRequest& from);
  Centre2GsLoginRequest(::google::protobuf::Arena* arena, Centre2GsLoginRequest&& from) noexcept
      : Centre2GsLoginRequest(arena) {
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
    kEnterGsTypeFieldNumber = 1,
  };
  // uint32 enter_gs_type = 1;
  void clear_enter_gs_type() ;
  ::uint32_t enter_gs_type() const;
  void set_enter_gs_type(::uint32_t value);

  private:
  ::uint32_t _internal_enter_gs_type() const;
  void _internal_set_enter_gs_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:Centre2GsLoginRequest)
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
                          const Centre2GsLoginRequest& from_msg);
    ::uint32_t enter_gs_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fscene_2fgame_5fplayer_2eproto;
};

// ===================================================================


// -------------------------------------------------------------------

class ScenePlayer_Stub;
class ScenePlayer : public ::google::protobuf::Service {
 protected:
  ScenePlayer() = default;

 public:
  using Stub = ScenePlayer_Stub;

  ScenePlayer(const ScenePlayer&) = delete;
  ScenePlayer& operator=(const ScenePlayer&) = delete;
  virtual ~ScenePlayer() = default;

  static const ::google::protobuf::ServiceDescriptor* descriptor();

  virtual void Centre2GsLogin(::google::protobuf::RpcController* controller,
                        const ::Centre2GsLoginRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void ExitGame(::google::protobuf::RpcController* controller,
                        const ::GameNodeExitGameRequest* request,
                        ::google::protobuf::Empty* response,
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

class ScenePlayer_Stub final : public ScenePlayer {
 public:
  ScenePlayer_Stub(::google::protobuf::RpcChannel* channel);
  ScenePlayer_Stub(::google::protobuf::RpcChannel* channel,
                   ::google::protobuf::Service::ChannelOwnership ownership);

  ScenePlayer_Stub(const ScenePlayer_Stub&) = delete;
  ScenePlayer_Stub& operator=(const ScenePlayer_Stub&) = delete;

  ~ScenePlayer_Stub() override;

  inline ::google::protobuf::RpcChannel* channel() { return channel_; }

  // implements ScenePlayer ------------------------------------------
  void Centre2GsLogin(::google::protobuf::RpcController* controller,
                        const ::Centre2GsLoginRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void ExitGame(::google::protobuf::RpcController* controller,
                        const ::GameNodeExitGameRequest* request,
                        ::google::protobuf::Empty* response,
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

// Centre2GsLoginRequest

// uint32 enter_gs_type = 1;
inline void Centre2GsLoginRequest::clear_enter_gs_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enter_gs_type_ = 0u;
}
inline ::uint32_t Centre2GsLoginRequest::enter_gs_type() const {
  // @@protoc_insertion_point(field_get:Centre2GsLoginRequest.enter_gs_type)
  return _internal_enter_gs_type();
}
inline void Centre2GsLoginRequest::set_enter_gs_type(::uint32_t value) {
  _internal_set_enter_gs_type(value);
  // @@protoc_insertion_point(field_set:Centre2GsLoginRequest.enter_gs_type)
}
inline ::uint32_t Centre2GsLoginRequest::_internal_enter_gs_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.enter_gs_type_;
}
inline void Centre2GsLoginRequest::_internal_set_enter_gs_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enter_gs_type_ = value;
}

// -------------------------------------------------------------------

// GameNodeExitGameRequest

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2fscene_2fgame_5fplayer_2eproto_2epb_2eh
