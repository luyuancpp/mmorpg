// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: game_rpc.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef game_5frpc_2eproto_2epb_2eh
#define game_5frpc_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 6031000
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
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_game_5frpc_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_game_5frpc_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_game_5frpc_2eproto;
}  // extern "C"
enum GameErrorCode : int;
extern const uint32_t GameErrorCode_internal_data_[];
enum GameMessageType : int;
extern const uint32_t GameMessageType_internal_data_[];
class GameRpcMessage;
struct GameRpcMessageDefaultTypeInternal;
extern GameRpcMessageDefaultTypeInternal _GameRpcMessage_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull GameRpcMessage_class_data_;
namespace google {
namespace protobuf {
template <>
internal::EnumTraitsT<::GameErrorCode_internal_data_>
    internal::EnumTraitsImpl::value<::GameErrorCode>;
template <>
internal::EnumTraitsT<::GameMessageType_internal_data_>
    internal::EnumTraitsImpl::value<::GameMessageType>;
}  // namespace protobuf
}  // namespace google

enum GameMessageType : int {
  REQUEST = 0,
  RESPONSE = 1,
  RPC_CLIENT_REQUEST = 2,
  NODE_ROUTE = 3,
  RPC_ERROR = 4,
  GameMessageType_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  GameMessageType_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

extern const uint32_t GameMessageType_internal_data_[];
inline constexpr GameMessageType GameMessageType_MIN =
    static_cast<GameMessageType>(0);
inline constexpr GameMessageType GameMessageType_MAX =
    static_cast<GameMessageType>(4);
inline bool GameMessageType_IsValid(int value) {
  return 0 <= value && value <= 4;
}
inline constexpr int GameMessageType_ARRAYSIZE = 4 + 1;
const ::google::protobuf::EnumDescriptor* PROTOBUF_NONNULL GameMessageType_descriptor();
template <typename T>
const std::string& GameMessageType_Name(T value) {
  static_assert(std::is_same<T, GameMessageType>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to GameMessageType_Name().");
  return GameMessageType_Name(static_cast<GameMessageType>(value));
}
template <>
inline const std::string& GameMessageType_Name(GameMessageType value) {
  return ::google::protobuf::internal::NameOfDenseEnum<GameMessageType_descriptor, 0, 4>(
      static_cast<int>(value));
}
inline bool GameMessageType_Parse(
    absl::string_view name, GameMessageType* PROTOBUF_NONNULL value) {
  return ::google::protobuf::internal::ParseNamedEnum<GameMessageType>(GameMessageType_descriptor(), name,
                                           value);
}
enum GameErrorCode : int {
  RPC_NO_ERROR = 0,
  WRONG_PROTO = 1,
  NO_SERVICE = 2,
  NO_METHOD = 3,
  INVALID_REQUEST = 4,
  INVALID_RESPONSE = 5,
  TIMEOUT = 6,
  GameErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  GameErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

extern const uint32_t GameErrorCode_internal_data_[];
inline constexpr GameErrorCode GameErrorCode_MIN =
    static_cast<GameErrorCode>(0);
inline constexpr GameErrorCode GameErrorCode_MAX =
    static_cast<GameErrorCode>(6);
inline bool GameErrorCode_IsValid(int value) {
  return 0 <= value && value <= 6;
}
inline constexpr int GameErrorCode_ARRAYSIZE = 6 + 1;
const ::google::protobuf::EnumDescriptor* PROTOBUF_NONNULL GameErrorCode_descriptor();
template <typename T>
const std::string& GameErrorCode_Name(T value) {
  static_assert(std::is_same<T, GameErrorCode>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to GameErrorCode_Name().");
  return GameErrorCode_Name(static_cast<GameErrorCode>(value));
}
template <>
inline const std::string& GameErrorCode_Name(GameErrorCode value) {
  return ::google::protobuf::internal::NameOfDenseEnum<GameErrorCode_descriptor, 0, 6>(
      static_cast<int>(value));
}
inline bool GameErrorCode_Parse(
    absl::string_view name, GameErrorCode* PROTOBUF_NONNULL value) {
  return ::google::protobuf::internal::ParseNamedEnum<GameErrorCode>(GameErrorCode_descriptor(), name,
                                           value);
}

// ===================================================================


// -------------------------------------------------------------------

class GameRpcMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:GameRpcMessage) */ {
 public:
  inline GameRpcMessage() : GameRpcMessage(nullptr) {}
  ~GameRpcMessage() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(GameRpcMessage* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(GameRpcMessage));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR GameRpcMessage(::google::protobuf::internal::ConstantInitialized);

  inline GameRpcMessage(const GameRpcMessage& from) : GameRpcMessage(nullptr, from) {}
  inline GameRpcMessage(GameRpcMessage&& from) noexcept
      : GameRpcMessage(nullptr, std::move(from)) {}
  inline GameRpcMessage& operator=(const GameRpcMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline GameRpcMessage& operator=(GameRpcMessage&& from) noexcept {
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
  inline ::google::protobuf::UnknownFieldSet* PROTOBUF_NONNULL mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* PROTOBUF_NONNULL GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* PROTOBUF_NONNULL GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const GameRpcMessage& default_instance() {
    return *reinterpret_cast<const GameRpcMessage*>(
        &_GameRpcMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(GameRpcMessage& a, GameRpcMessage& b) { a.Swap(&b); }
  inline void Swap(GameRpcMessage* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(GameRpcMessage* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  GameRpcMessage* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<GameRpcMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const GameRpcMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const GameRpcMessage& from) { GameRpcMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(::google::protobuf::MessageLite& to_msg,
                        const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      const ::google::protobuf::MessageLite& msg, ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* PROTOBUF_NONNULL _InternalSerialize(
      ::uint8_t* PROTOBUF_NONNULL target,
      ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(GameRpcMessage* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "GameRpcMessage"; }

 protected:
  explicit GameRpcMessage(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  GameRpcMessage(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const GameRpcMessage& from);
  GameRpcMessage(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, GameRpcMessage&& from) noexcept
      : GameRpcMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GetClassData() const PROTOBUF_FINAL;
  static void* PROTOBUF_NONNULL PlacementNew_(
      const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  static constexpr auto InternalNewImpl_();

 public:
  static constexpr auto InternalGenerateClassData_();

  ::google::protobuf::Metadata GetMetadata() const;
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
  std::string* PROTOBUF_NONNULL mutable_request();
  [[nodiscard]] std::string* PROTOBUF_NULLABLE release_request();
  void set_allocated_request(std::string* PROTOBUF_NULLABLE value);

  private:
  const std::string& _internal_request() const;
  PROTOBUF_ALWAYS_INLINE void _internal_set_request(const std::string& value);
  std::string* PROTOBUF_NONNULL _internal_mutable_request();

  public:
  // bytes response = 3;
  void clear_response() ;
  const std::string& response() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_response(Arg_&& arg, Args_... args);
  std::string* PROTOBUF_NONNULL mutable_response();
  [[nodiscard]] std::string* PROTOBUF_NULLABLE release_response();
  void set_allocated_response(std::string* PROTOBUF_NULLABLE value);

  private:
  const std::string& _internal_response() const;
  PROTOBUF_ALWAYS_INLINE void _internal_set_response(const std::string& value);
  std::string* PROTOBUF_NONNULL _internal_mutable_response();

  public:
  // .GameMessageType type = 1;
  void clear_type() ;
  ::GameMessageType type() const;
  void set_type(::GameMessageType value);

  private:
  ::GameMessageType _internal_type() const;
  void _internal_set_type(::GameMessageType value);

  public:
  // .GameErrorCode error = 4;
  void clear_error() ;
  ::GameErrorCode error() const;
  void set_error(::GameErrorCode value);

  private:
  ::GameErrorCode _internal_error() const;
  void _internal_set_error(::GameErrorCode value);

  public:
  // uint32 message_id = 5;
  void clear_message_id() ;
  ::uint32_t message_id() const;
  void set_message_id(::uint32_t value);

  private:
  ::uint32_t _internal_message_id() const;
  void _internal_set_message_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:GameRpcMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<3, 5,
                                   0, 0,
                                   2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
    inline explicit Impl_(
        ::google::protobuf::internal::InternalVisibility visibility,
        ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const Impl_& from,
        const GameRpcMessage& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::ArenaStringPtr request_;
    ::google::protobuf::internal::ArenaStringPtr response_;
    int type_;
    int error_;
    ::uint32_t message_id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_game_5frpc_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull GameRpcMessage_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// GameRpcMessage

// .GameMessageType type = 1;
inline void GameRpcMessage::clear_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = 0;
  _impl_._has_bits_[0] &= ~0x00000004u;
}
inline ::GameMessageType GameRpcMessage::type() const {
  // @@protoc_insertion_point(field_get:GameRpcMessage.type)
  return _internal_type();
}
inline void GameRpcMessage::set_type(::GameMessageType value) {
  _internal_set_type(value);
  _impl_._has_bits_[0] |= 0x00000004u;
  // @@protoc_insertion_point(field_set:GameRpcMessage.type)
}
inline ::GameMessageType GameRpcMessage::_internal_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::GameMessageType>(_impl_.type_);
}
inline void GameRpcMessage::_internal_set_type(::GameMessageType value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = value;
}

// bytes request = 2;
inline void GameRpcMessage::clear_request() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.request_.ClearToEmpty();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const std::string& GameRpcMessage::request() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:GameRpcMessage.request)
  return _internal_request();
}
template <typename Arg_, typename... Args_>
PROTOBUF_ALWAYS_INLINE void GameRpcMessage::set_request(Arg_&& arg, Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.request_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:GameRpcMessage.request)
}
inline std::string* PROTOBUF_NONNULL GameRpcMessage::mutable_request()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_request();
  // @@protoc_insertion_point(field_mutable:GameRpcMessage.request)
  return _s;
}
inline const std::string& GameRpcMessage::_internal_request() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.request_.Get();
}
inline void GameRpcMessage::_internal_set_request(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.request_.Set(value, GetArena());
}
inline std::string* PROTOBUF_NONNULL GameRpcMessage::_internal_mutable_request() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  return _impl_.request_.Mutable( GetArena());
}
inline std::string* PROTOBUF_NULLABLE GameRpcMessage::release_request() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:GameRpcMessage.request)
  if ((_impl_._has_bits_[0] & 0x00000001u) == 0) {
    return nullptr;
  }
  _impl_._has_bits_[0] &= ~0x00000001u;
  auto* released = _impl_.request_.Release();
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString()) {
    _impl_.request_.Set("", GetArena());
  }
  return released;
}
inline void GameRpcMessage::set_allocated_request(std::string* PROTOBUF_NULLABLE value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  _impl_.request_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.request_.IsDefault()) {
    _impl_.request_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:GameRpcMessage.request)
}

// bytes response = 3;
inline void GameRpcMessage::clear_response() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.response_.ClearToEmpty();
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline const std::string& GameRpcMessage::response() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:GameRpcMessage.response)
  return _internal_response();
}
template <typename Arg_, typename... Args_>
PROTOBUF_ALWAYS_INLINE void GameRpcMessage::set_response(Arg_&& arg, Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000002u;
  _impl_.response_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:GameRpcMessage.response)
}
inline std::string* PROTOBUF_NONNULL GameRpcMessage::mutable_response()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_response();
  // @@protoc_insertion_point(field_mutable:GameRpcMessage.response)
  return _s;
}
inline const std::string& GameRpcMessage::_internal_response() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.response_.Get();
}
inline void GameRpcMessage::_internal_set_response(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000002u;
  _impl_.response_.Set(value, GetArena());
}
inline std::string* PROTOBUF_NONNULL GameRpcMessage::_internal_mutable_response() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000002u;
  return _impl_.response_.Mutable( GetArena());
}
inline std::string* PROTOBUF_NULLABLE GameRpcMessage::release_response() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:GameRpcMessage.response)
  if ((_impl_._has_bits_[0] & 0x00000002u) == 0) {
    return nullptr;
  }
  _impl_._has_bits_[0] &= ~0x00000002u;
  auto* released = _impl_.response_.Release();
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString()) {
    _impl_.response_.Set("", GetArena());
  }
  return released;
}
inline void GameRpcMessage::set_allocated_response(std::string* PROTOBUF_NULLABLE value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }
  _impl_.response_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.response_.IsDefault()) {
    _impl_.response_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:GameRpcMessage.response)
}

// .GameErrorCode error = 4;
inline void GameRpcMessage::clear_error() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.error_ = 0;
  _impl_._has_bits_[0] &= ~0x00000008u;
}
inline ::GameErrorCode GameRpcMessage::error() const {
  // @@protoc_insertion_point(field_get:GameRpcMessage.error)
  return _internal_error();
}
inline void GameRpcMessage::set_error(::GameErrorCode value) {
  _internal_set_error(value);
  _impl_._has_bits_[0] |= 0x00000008u;
  // @@protoc_insertion_point(field_set:GameRpcMessage.error)
}
inline ::GameErrorCode GameRpcMessage::_internal_error() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::GameErrorCode>(_impl_.error_);
}
inline void GameRpcMessage::_internal_set_error(::GameErrorCode value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.error_ = value;
}

// uint32 message_id = 5;
inline void GameRpcMessage::clear_message_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.message_id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000010u;
}
inline ::uint32_t GameRpcMessage::message_id() const {
  // @@protoc_insertion_point(field_get:GameRpcMessage.message_id)
  return _internal_message_id();
}
inline void GameRpcMessage::set_message_id(::uint32_t value) {
  _internal_set_message_id(value);
  _impl_._has_bits_[0] |= 0x00000010u;
  // @@protoc_insertion_point(field_set:GameRpcMessage.message_id)
}
inline ::uint32_t GameRpcMessage::_internal_message_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.message_id_;
}
inline void GameRpcMessage::_internal_set_message_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.message_id_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::GameMessageType> : std::true_type {};
template <>
inline const EnumDescriptor* PROTOBUF_NONNULL GetEnumDescriptor<::GameMessageType>() {
  return ::GameMessageType_descriptor();
}
template <>
struct is_proto_enum<::GameErrorCode> : std::true_type {};
template <>
inline const EnumDescriptor* PROTOBUF_NONNULL GetEnumDescriptor<::GameErrorCode>() {
  return ::GameErrorCode_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // game_5frpc_2eproto_2epb_2eh
