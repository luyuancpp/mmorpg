// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: game_rpc.proto
// Protobuf C++ Version: 4.26.0-dev

#ifndef GOOGLE_PROTOBUF_INCLUDED_game_5frpc_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_game_5frpc_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/port_def.inc"
#if PROTOBUF_VERSION != 4026000
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
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_game_5frpc_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_game_5frpc_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_game_5frpc_2eproto;
namespace muduo {
namespace net {
class RpcMessage;
struct RpcMessageDefaultTypeInternal;
extern RpcMessageDefaultTypeInternal _RpcMessage_default_instance_;
}  // namespace net
}  // namespace muduo
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace muduo {
namespace net {
enum MessageType : int {
  REQUEST = 0,
  RESPONSE = 1,
  S2C_REQUEST = 2,
  NODE_ROUTE = 3,
  RPC_ERROR = 4,
  MessageType_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  MessageType_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool MessageType_IsValid(int value);
extern const uint32_t MessageType_internal_data_[];
constexpr MessageType MessageType_MIN = static_cast<MessageType>(0);
constexpr MessageType MessageType_MAX = static_cast<MessageType>(4);
constexpr int MessageType_ARRAYSIZE = 4 + 1;
const ::google::protobuf::EnumDescriptor*
MessageType_descriptor();
template <typename T>
const std::string& MessageType_Name(T value) {
  static_assert(std::is_same<T, MessageType>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to MessageType_Name().");
  return MessageType_Name(static_cast<MessageType>(value));
}
template <>
inline const std::string& MessageType_Name(MessageType value) {
  return ::google::protobuf::internal::NameOfDenseEnum<MessageType_descriptor,
                                                 0, 4>(
      static_cast<int>(value));
}
inline bool MessageType_Parse(absl::string_view name, MessageType* value) {
  return ::google::protobuf::internal::ParseNamedEnum<MessageType>(
      MessageType_descriptor(), name, value);
}
enum ErrorCode : int {
  RPC_NO_ERROR = 0,
  WRONG_PROTO = 1,
  NO_SERVICE = 2,
  NO_METHOD = 3,
  INVALID_REQUEST = 4,
  INVALID_RESPONSE = 5,
  TIMEOUT = 6,
  ErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  ErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool ErrorCode_IsValid(int value);
extern const uint32_t ErrorCode_internal_data_[];
constexpr ErrorCode ErrorCode_MIN = static_cast<ErrorCode>(0);
constexpr ErrorCode ErrorCode_MAX = static_cast<ErrorCode>(6);
constexpr int ErrorCode_ARRAYSIZE = 6 + 1;
const ::google::protobuf::EnumDescriptor*
ErrorCode_descriptor();
template <typename T>
const std::string& ErrorCode_Name(T value) {
  static_assert(std::is_same<T, ErrorCode>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to ErrorCode_Name().");
  return ErrorCode_Name(static_cast<ErrorCode>(value));
}
template <>
inline const std::string& ErrorCode_Name(ErrorCode value) {
  return ::google::protobuf::internal::NameOfDenseEnum<ErrorCode_descriptor,
                                                 0, 6>(
      static_cast<int>(value));
}
inline bool ErrorCode_Parse(absl::string_view name, ErrorCode* value) {
  return ::google::protobuf::internal::ParseNamedEnum<ErrorCode>(
      ErrorCode_descriptor(), name, value);
}

// ===================================================================


// -------------------------------------------------------------------

class RpcMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:muduo.net.RpcMessage) */ {
 public:
  inline RpcMessage() : RpcMessage(nullptr) {}
  ~RpcMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR RpcMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline RpcMessage(const RpcMessage& from) : RpcMessage(nullptr, from) {}
  inline RpcMessage(RpcMessage&& from) noexcept
      : RpcMessage(nullptr, std::move(from)) {}
  inline RpcMessage& operator=(const RpcMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline RpcMessage& operator=(RpcMessage&& from) noexcept {
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
  static const RpcMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const RpcMessage* internal_default_instance() {
    return reinterpret_cast<const RpcMessage*>(
        &_RpcMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(RpcMessage& a, RpcMessage& b) { a.Swap(&b); }
  inline void Swap(RpcMessage* other) {
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
  void UnsafeArenaSwap(RpcMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RpcMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<RpcMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const RpcMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const RpcMessage& from) { RpcMessage::MergeImpl(*this, from); }

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
  void InternalSwap(RpcMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "muduo.net.RpcMessage"; }

 protected:
  explicit RpcMessage(::google::protobuf::Arena* arena);
  RpcMessage(::google::protobuf::Arena* arena, const RpcMessage& from);
  RpcMessage(::google::protobuf::Arena* arena, RpcMessage&& from) noexcept
      : RpcMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kRequestFieldNumber = 2,
    kResponseFieldNumber = 3,
    kTypeFieldNumber = 1,
    kErrorFieldNumber = 4,
    kMessageIdFieldNumber = 5,
  };
  // bytes request = 2;
  void clear_request() ;
  const std::string& request() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_request(Arg_&& arg, Args_... args);
  std::string* mutable_request();
  PROTOBUF_NODISCARD std::string* release_request();
  void set_allocated_request(std::string* value);

  private:
  const std::string& _internal_request() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_request(
      const std::string& value);
  std::string* _internal_mutable_request();

  public:
  // bytes response = 3;
  void clear_response() ;
  const std::string& response() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_response(Arg_&& arg, Args_... args);
  std::string* mutable_response();
  PROTOBUF_NODISCARD std::string* release_response();
  void set_allocated_response(std::string* value);

  private:
  const std::string& _internal_response() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_response(
      const std::string& value);
  std::string* _internal_mutable_response();

  public:
  // .muduo.net.MessageType type = 1;
  void clear_type() ;
  ::muduo::net::MessageType type() const;
  void set_type(::muduo::net::MessageType value);

  private:
  ::muduo::net::MessageType _internal_type() const;
  void _internal_set_type(::muduo::net::MessageType value);

  public:
  // .muduo.net.ErrorCode error = 4;
  void clear_error() ;
  ::muduo::net::ErrorCode error() const;
  void set_error(::muduo::net::ErrorCode value);

  private:
  ::muduo::net::ErrorCode _internal_error() const;
  void _internal_set_error(::muduo::net::ErrorCode value);

  public:
  // uint32 message_id = 5;
  void clear_message_id() ;
  ::uint32_t message_id() const;
  void set_message_id(::uint32_t value);

  private:
  ::uint32_t _internal_message_id() const;
  void _internal_set_message_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:muduo.net.RpcMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 5, 0,
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
    ::google::protobuf::internal::ArenaStringPtr request_;
    ::google::protobuf::internal::ArenaStringPtr response_;
    int type_;
    int error_;
    ::uint32_t message_id_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_game_5frpc_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// RpcMessage

// .muduo.net.MessageType type = 1;
inline void RpcMessage::clear_type() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.type_ = 0;
}
inline ::muduo::net::MessageType RpcMessage::type() const {
  // @@protoc_insertion_point(field_get:muduo.net.RpcMessage.type)
  return _internal_type();
}
inline void RpcMessage::set_type(::muduo::net::MessageType value) {
  _internal_set_type(value);
  // @@protoc_insertion_point(field_set:muduo.net.RpcMessage.type)
}
inline ::muduo::net::MessageType RpcMessage::_internal_type() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return static_cast<::muduo::net::MessageType>(_impl_.type_);
}
inline void RpcMessage::_internal_set_type(::muduo::net::MessageType value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.type_ = value;
}

// bytes request = 2;
inline void RpcMessage::clear_request() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.request_.ClearToEmpty();
}
inline const std::string& RpcMessage::request() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:muduo.net.RpcMessage.request)
  return _internal_request();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void RpcMessage::set_request(Arg_&& arg,
                                                     Args_... args) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.request_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:muduo.net.RpcMessage.request)
}
inline std::string* RpcMessage::mutable_request() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_request();
  // @@protoc_insertion_point(field_mutable:muduo.net.RpcMessage.request)
  return _s;
}
inline const std::string& RpcMessage::_internal_request() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.request_.Get();
}
inline void RpcMessage::_internal_set_request(const std::string& value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.request_.Set(value, GetArena());
}
inline std::string* RpcMessage::_internal_mutable_request() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _impl_.request_.Mutable( GetArena());
}
inline std::string* RpcMessage::release_request() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:muduo.net.RpcMessage.request)
  return _impl_.request_.Release();
}
inline void RpcMessage::set_allocated_request(std::string* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.request_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.request_.IsDefault()) {
          _impl_.request_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:muduo.net.RpcMessage.request)
}

// bytes response = 3;
inline void RpcMessage::clear_response() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.response_.ClearToEmpty();
}
inline const std::string& RpcMessage::response() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:muduo.net.RpcMessage.response)
  return _internal_response();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void RpcMessage::set_response(Arg_&& arg,
                                                     Args_... args) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.response_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:muduo.net.RpcMessage.response)
}
inline std::string* RpcMessage::mutable_response() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_response();
  // @@protoc_insertion_point(field_mutable:muduo.net.RpcMessage.response)
  return _s;
}
inline const std::string& RpcMessage::_internal_response() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.response_.Get();
}
inline void RpcMessage::_internal_set_response(const std::string& value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.response_.Set(value, GetArena());
}
inline std::string* RpcMessage::_internal_mutable_response() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _impl_.response_.Mutable( GetArena());
}
inline std::string* RpcMessage::release_response() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:muduo.net.RpcMessage.response)
  return _impl_.response_.Release();
}
inline void RpcMessage::set_allocated_response(std::string* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.response_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.response_.IsDefault()) {
          _impl_.response_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:muduo.net.RpcMessage.response)
}

// .muduo.net.ErrorCode error = 4;
inline void RpcMessage::clear_error() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.error_ = 0;
}
inline ::muduo::net::ErrorCode RpcMessage::error() const {
  // @@protoc_insertion_point(field_get:muduo.net.RpcMessage.error)
  return _internal_error();
}
inline void RpcMessage::set_error(::muduo::net::ErrorCode value) {
  _internal_set_error(value);
  // @@protoc_insertion_point(field_set:muduo.net.RpcMessage.error)
}
inline ::muduo::net::ErrorCode RpcMessage::_internal_error() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return static_cast<::muduo::net::ErrorCode>(_impl_.error_);
}
inline void RpcMessage::_internal_set_error(::muduo::net::ErrorCode value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.error_ = value;
}

// uint32 message_id = 5;
inline void RpcMessage::clear_message_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.message_id_ = 0u;
}
inline ::uint32_t RpcMessage::message_id() const {
  // @@protoc_insertion_point(field_get:muduo.net.RpcMessage.message_id)
  return _internal_message_id();
}
inline void RpcMessage::set_message_id(::uint32_t value) {
  _internal_set_message_id(value);
  // @@protoc_insertion_point(field_set:muduo.net.RpcMessage.message_id)
}
inline ::uint32_t RpcMessage::_internal_message_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.message_id_;
}
inline void RpcMessage::_internal_set_message_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.message_id_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace net
}  // namespace muduo


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::muduo::net::MessageType> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::muduo::net::MessageType>() {
  return ::muduo::net::MessageType_descriptor();
}
template <>
struct is_proto_enum<::muduo::net::ErrorCode> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::muduo::net::ErrorCode>() {
  return ::muduo::net::ErrorCode_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_game_5frpc_2eproto_2epb_2eh
