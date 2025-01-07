// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/common/gate_service.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fgate_5fservice_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fgate_5fservice_2eproto_2epb_2eh

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
#include "proto/common/empty.pb.h"
#include "proto/common/common.pb.h"
#include "proto/common/common_message.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2fcommon_2fgate_5fservice_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2fcommon_2fgate_5fservice_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto;
class BroadcastToPlayersRequest;
struct BroadcastToPlayersRequestDefaultTypeInternal;
extern BroadcastToPlayersRequestDefaultTypeInternal _BroadcastToPlayersRequest_default_instance_;
class KickSessionRequest;
struct KickSessionRequestDefaultTypeInternal;
extern KickSessionRequestDefaultTypeInternal _KickSessionRequest_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class KickSessionRequest final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:KickSessionRequest) */ {
 public:
  inline KickSessionRequest() : KickSessionRequest(nullptr) {}
  ~KickSessionRequest() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR KickSessionRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline KickSessionRequest(const KickSessionRequest& from) : KickSessionRequest(nullptr, from) {}
  inline KickSessionRequest(KickSessionRequest&& from) noexcept
      : KickSessionRequest(nullptr, std::move(from)) {}
  inline KickSessionRequest& operator=(const KickSessionRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline KickSessionRequest& operator=(KickSessionRequest&& from) noexcept {
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
  static const KickSessionRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const KickSessionRequest* internal_default_instance() {
    return reinterpret_cast<const KickSessionRequest*>(
        &_KickSessionRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(KickSessionRequest& a, KickSessionRequest& b) { a.Swap(&b); }
  inline void Swap(KickSessionRequest* other) {
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
  void UnsafeArenaSwap(KickSessionRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  KickSessionRequest* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<KickSessionRequest>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const KickSessionRequest& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const KickSessionRequest& from) { KickSessionRequest::MergeImpl(*this, from); }

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
  void InternalSwap(KickSessionRequest* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "KickSessionRequest"; }

 protected:
  explicit KickSessionRequest(::google::protobuf::Arena* arena);
  KickSessionRequest(::google::protobuf::Arena* arena, const KickSessionRequest& from);
  KickSessionRequest(::google::protobuf::Arena* arena, KickSessionRequest&& from) noexcept
      : KickSessionRequest(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSessionIdFieldNumber = 1,
  };
  // uint64 session_id = 1;
  void clear_session_id() ;
  ::uint64_t session_id() const;
  void set_session_id(::uint64_t value);

  private:
  ::uint64_t _internal_session_id() const;
  void _internal_set_session_id(::uint64_t value);

  public:
  // @@protoc_insertion_point(class_scope:KickSessionRequest)
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
    ::uint64_t session_id_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fcommon_2fgate_5fservice_2eproto;
};
// -------------------------------------------------------------------

class BroadcastToPlayersRequest final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:BroadcastToPlayersRequest) */ {
 public:
  inline BroadcastToPlayersRequest() : BroadcastToPlayersRequest(nullptr) {}
  ~BroadcastToPlayersRequest() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR BroadcastToPlayersRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline BroadcastToPlayersRequest(const BroadcastToPlayersRequest& from) : BroadcastToPlayersRequest(nullptr, from) {}
  inline BroadcastToPlayersRequest(BroadcastToPlayersRequest&& from) noexcept
      : BroadcastToPlayersRequest(nullptr, std::move(from)) {}
  inline BroadcastToPlayersRequest& operator=(const BroadcastToPlayersRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline BroadcastToPlayersRequest& operator=(BroadcastToPlayersRequest&& from) noexcept {
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
  static const BroadcastToPlayersRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const BroadcastToPlayersRequest* internal_default_instance() {
    return reinterpret_cast<const BroadcastToPlayersRequest*>(
        &_BroadcastToPlayersRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(BroadcastToPlayersRequest& a, BroadcastToPlayersRequest& b) { a.Swap(&b); }
  inline void Swap(BroadcastToPlayersRequest* other) {
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
  void UnsafeArenaSwap(BroadcastToPlayersRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  BroadcastToPlayersRequest* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<BroadcastToPlayersRequest>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const BroadcastToPlayersRequest& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const BroadcastToPlayersRequest& from) { BroadcastToPlayersRequest::MergeImpl(*this, from); }

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
  void InternalSwap(BroadcastToPlayersRequest* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "BroadcastToPlayersRequest"; }

 protected:
  explicit BroadcastToPlayersRequest(::google::protobuf::Arena* arena);
  BroadcastToPlayersRequest(::google::protobuf::Arena* arena, const BroadcastToPlayersRequest& from);
  BroadcastToPlayersRequest(::google::protobuf::Arena* arena, BroadcastToPlayersRequest&& from) noexcept
      : BroadcastToPlayersRequest(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSessionListFieldNumber = 1,
    kBodyFieldNumber = 2,
  };
  // repeated uint64 session_list = 1;
  int session_list_size() const;
  private:
  int _internal_session_list_size() const;

  public:
  void clear_session_list() ;
  ::uint64_t session_list(int index) const;
  void set_session_list(int index, ::uint64_t value);
  void add_session_list(::uint64_t value);
  const ::google::protobuf::RepeatedField<::uint64_t>& session_list() const;
  ::google::protobuf::RepeatedField<::uint64_t>* mutable_session_list();

  private:
  const ::google::protobuf::RepeatedField<::uint64_t>& _internal_session_list() const;
  ::google::protobuf::RepeatedField<::uint64_t>* _internal_mutable_session_list();

  public:
  // .MessagePayload body = 2;
  bool has_body() const;
  void clear_body() ;
  const ::MessagePayload& body() const;
  PROTOBUF_NODISCARD ::MessagePayload* release_body();
  ::MessagePayload* mutable_body();
  void set_allocated_body(::MessagePayload* value);
  void unsafe_arena_set_allocated_body(::MessagePayload* value);
  ::MessagePayload* unsafe_arena_release_body();

  private:
  const ::MessagePayload& _internal_body() const;
  ::MessagePayload* _internal_mutable_body();

  public:
  // @@protoc_insertion_point(class_scope:BroadcastToPlayersRequest)
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
    ::google::protobuf::RepeatedField<::uint64_t> session_list_;
    mutable ::google::protobuf::internal::CachedSize _session_list_cached_byte_size_;
    ::MessagePayload* body_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fcommon_2fgate_5fservice_2eproto;
};

// ===================================================================


// -------------------------------------------------------------------

class GateService_Stub;
class GateService : public ::google::protobuf::Service {
 protected:
  GateService() = default;

 public:
  using Stub = GateService_Stub;

  GateService(const GateService&) = delete;
  GateService& operator=(const GateService&) = delete;
  virtual ~GateService() = default;

  static const ::google::protobuf::ServiceDescriptor* descriptor();

  virtual void RegisterGame(::google::protobuf::RpcController* controller,
                        const ::RegisterGameNodeRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void UnRegisterGame(::google::protobuf::RpcController* controller,
                        const ::UnregisterGameNodeRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void PlayerEnterGameNode(::google::protobuf::RpcController* controller,
                        const ::RegisterGameNodeSessionRequest* request,
                        ::RegisterGameNodeSessionResponse* response,
                        ::google::protobuf::Closure* done);
  virtual void SendMessageToPlayer(::google::protobuf::RpcController* controller,
                        const ::NodeRouteMessageRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void KickSessionByCentre(::google::protobuf::RpcController* controller,
                        const ::KickSessionRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void RouteNodeMessage(::google::protobuf::RpcController* controller,
                        const ::RouteMessageRequest* request,
                        ::RouteMessageResponse* response,
                        ::google::protobuf::Closure* done);
  virtual void RoutePlayerMessage(::google::protobuf::RpcController* controller,
                        const ::RoutePlayerMessageRequest* request,
                        ::RoutePlayerMessageResponse* response,
                        ::google::protobuf::Closure* done);
  virtual void BroadcastToPlayers(::google::protobuf::RpcController* controller,
                        const ::BroadcastToPlayersRequest* request,
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

class GateService_Stub final : public GateService {
 public:
  GateService_Stub(::google::protobuf::RpcChannel* channel);
  GateService_Stub(::google::protobuf::RpcChannel* channel,
                   ::google::protobuf::Service::ChannelOwnership ownership);

  GateService_Stub(const GateService_Stub&) = delete;
  GateService_Stub& operator=(const GateService_Stub&) = delete;

  ~GateService_Stub() override;

  inline ::google::protobuf::RpcChannel* channel() { return channel_; }

  // implements GateService ------------------------------------------
  void RegisterGame(::google::protobuf::RpcController* controller,
                        const ::RegisterGameNodeRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void UnRegisterGame(::google::protobuf::RpcController* controller,
                        const ::UnregisterGameNodeRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void PlayerEnterGameNode(::google::protobuf::RpcController* controller,
                        const ::RegisterGameNodeSessionRequest* request,
                        ::RegisterGameNodeSessionResponse* response,
                        ::google::protobuf::Closure* done) override;
  void SendMessageToPlayer(::google::protobuf::RpcController* controller,
                        const ::NodeRouteMessageRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void KickSessionByCentre(::google::protobuf::RpcController* controller,
                        const ::KickSessionRequest* request,
                        ::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void RouteNodeMessage(::google::protobuf::RpcController* controller,
                        const ::RouteMessageRequest* request,
                        ::RouteMessageResponse* response,
                        ::google::protobuf::Closure* done) override;
  void RoutePlayerMessage(::google::protobuf::RpcController* controller,
                        const ::RoutePlayerMessageRequest* request,
                        ::RoutePlayerMessageResponse* response,
                        ::google::protobuf::Closure* done) override;
  void BroadcastToPlayers(::google::protobuf::RpcController* controller,
                        const ::BroadcastToPlayersRequest* request,
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

// KickSessionRequest

// uint64 session_id = 1;
inline void KickSessionRequest::clear_session_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.session_id_ = ::uint64_t{0u};
}
inline ::uint64_t KickSessionRequest::session_id() const {
  // @@protoc_insertion_point(field_get:KickSessionRequest.session_id)
  return _internal_session_id();
}
inline void KickSessionRequest::set_session_id(::uint64_t value) {
  _internal_set_session_id(value);
  // @@protoc_insertion_point(field_set:KickSessionRequest.session_id)
}
inline ::uint64_t KickSessionRequest::_internal_session_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.session_id_;
}
inline void KickSessionRequest::_internal_set_session_id(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.session_id_ = value;
}

// -------------------------------------------------------------------

// BroadcastToPlayersRequest

// repeated uint64 session_list = 1;
inline int BroadcastToPlayersRequest::_internal_session_list_size() const {
  return _internal_session_list().size();
}
inline int BroadcastToPlayersRequest::session_list_size() const {
  return _internal_session_list_size();
}
inline void BroadcastToPlayersRequest::clear_session_list() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.session_list_.Clear();
}
inline ::uint64_t BroadcastToPlayersRequest::session_list(int index) const {
  // @@protoc_insertion_point(field_get:BroadcastToPlayersRequest.session_list)
  return _internal_session_list().Get(index);
}
inline void BroadcastToPlayersRequest::set_session_list(int index, ::uint64_t value) {
  _internal_mutable_session_list()->Set(index, value);
  // @@protoc_insertion_point(field_set:BroadcastToPlayersRequest.session_list)
}
inline void BroadcastToPlayersRequest::add_session_list(::uint64_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _internal_mutable_session_list()->Add(value);
  // @@protoc_insertion_point(field_add:BroadcastToPlayersRequest.session_list)
}
inline const ::google::protobuf::RepeatedField<::uint64_t>& BroadcastToPlayersRequest::session_list() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:BroadcastToPlayersRequest.session_list)
  return _internal_session_list();
}
inline ::google::protobuf::RepeatedField<::uint64_t>* BroadcastToPlayersRequest::mutable_session_list()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:BroadcastToPlayersRequest.session_list)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_session_list();
}
inline const ::google::protobuf::RepeatedField<::uint64_t>&
BroadcastToPlayersRequest::_internal_session_list() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.session_list_;
}
inline ::google::protobuf::RepeatedField<::uint64_t>* BroadcastToPlayersRequest::_internal_mutable_session_list() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.session_list_;
}

// .MessagePayload body = 2;
inline bool BroadcastToPlayersRequest::has_body() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.body_ != nullptr);
  return value;
}
inline const ::MessagePayload& BroadcastToPlayersRequest::_internal_body() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  const ::MessagePayload* p = _impl_.body_;
  return p != nullptr ? *p : reinterpret_cast<const ::MessagePayload&>(::_MessagePayload_default_instance_);
}
inline const ::MessagePayload& BroadcastToPlayersRequest::body() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:BroadcastToPlayersRequest.body)
  return _internal_body();
}
inline void BroadcastToPlayersRequest::unsafe_arena_set_allocated_body(::MessagePayload* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.body_);
  }
  _impl_.body_ = reinterpret_cast<::MessagePayload*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:BroadcastToPlayersRequest.body)
}
inline ::MessagePayload* BroadcastToPlayersRequest::release_body() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::MessagePayload* released = _impl_.body_;
  _impl_.body_ = nullptr;
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
inline ::MessagePayload* BroadcastToPlayersRequest::unsafe_arena_release_body() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:BroadcastToPlayersRequest.body)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::MessagePayload* temp = _impl_.body_;
  _impl_.body_ = nullptr;
  return temp;
}
inline ::MessagePayload* BroadcastToPlayersRequest::_internal_mutable_body() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.body_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::MessagePayload>(GetArena());
    _impl_.body_ = reinterpret_cast<::MessagePayload*>(p);
  }
  return _impl_.body_;
}
inline ::MessagePayload* BroadcastToPlayersRequest::mutable_body() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::MessagePayload* _msg = _internal_mutable_body();
  // @@protoc_insertion_point(field_mutable:BroadcastToPlayersRequest.body)
  return _msg;
}
inline void BroadcastToPlayersRequest::set_allocated_body(::MessagePayload* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (message_arena == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.body_);
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

  _impl_.body_ = reinterpret_cast<::MessagePayload*>(value);
  // @@protoc_insertion_point(field_set_allocated:BroadcastToPlayersRequest.body)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fgate_5fservice_2eproto_2epb_2eh
