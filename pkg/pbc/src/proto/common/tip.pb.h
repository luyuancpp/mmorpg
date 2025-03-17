// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/common/tip.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef proto_2fcommon_2ftip_2eproto_2epb_2eh
#define proto_2fcommon_2ftip_2eproto_2epb_2eh

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
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2fcommon_2ftip_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2fcommon_2ftip_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_proto_2fcommon_2ftip_2eproto;
}  // extern "C"
class TipInfoMessage;
struct TipInfoMessageDefaultTypeInternal;
extern TipInfoMessageDefaultTypeInternal _TipInfoMessage_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull TipInfoMessage_class_data_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class TipInfoMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:TipInfoMessage) */ {
 public:
  inline TipInfoMessage() : TipInfoMessage(nullptr) {}
  ~TipInfoMessage() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(TipInfoMessage* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(TipInfoMessage));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR TipInfoMessage(::google::protobuf::internal::ConstantInitialized);

  inline TipInfoMessage(const TipInfoMessage& from) : TipInfoMessage(nullptr, from) {}
  inline TipInfoMessage(TipInfoMessage&& from) noexcept
      : TipInfoMessage(nullptr, std::move(from)) {}
  inline TipInfoMessage& operator=(const TipInfoMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline TipInfoMessage& operator=(TipInfoMessage&& from) noexcept {
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
  static const TipInfoMessage& default_instance() {
    return *reinterpret_cast<const TipInfoMessage*>(
        &_TipInfoMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(TipInfoMessage& a, TipInfoMessage& b) { a.Swap(&b); }
  inline void Swap(TipInfoMessage* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(TipInfoMessage* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  TipInfoMessage* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<TipInfoMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const TipInfoMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const TipInfoMessage& from) { TipInfoMessage::MergeImpl(*this, from); }

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
  void InternalSwap(TipInfoMessage* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "TipInfoMessage"; }

 protected:
  explicit TipInfoMessage(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  TipInfoMessage(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const TipInfoMessage& from);
  TipInfoMessage(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, TipInfoMessage&& from) noexcept
      : TipInfoMessage(arena) {
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
    kParametersFieldNumber = 2,
    kIdFieldNumber = 1,
  };
  // repeated string parameters = 2;
  int parameters_size() const;
  private:
  int _internal_parameters_size() const;

  public:
  void clear_parameters() ;
  const std::string& parameters(int index) const;
  std::string* PROTOBUF_NONNULL mutable_parameters(int index);
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_parameters(int index, Arg_&& value, Args_... args);
  std::string* PROTOBUF_NONNULL add_parameters();
  template <typename Arg_ = const std::string&, typename... Args_>
  void add_parameters(Arg_&& value, Args_... args);
  const ::google::protobuf::RepeatedPtrField<std::string>& parameters() const;
  ::google::protobuf::RepeatedPtrField<std::string>* PROTOBUF_NONNULL mutable_parameters();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_parameters() const;
  ::google::protobuf::RepeatedPtrField<std::string>* PROTOBUF_NONNULL _internal_mutable_parameters();

  public:
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:TipInfoMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
                                   0, 33,
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
        const TipInfoMessage& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField<std::string> parameters_;
    ::uint32_t id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fcommon_2ftip_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull TipInfoMessage_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// TipInfoMessage

// uint32 id = 1;
inline void TipInfoMessage::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t TipInfoMessage::id() const {
  // @@protoc_insertion_point(field_get:TipInfoMessage.id)
  return _internal_id();
}
inline void TipInfoMessage::set_id(::uint32_t value) {
  _internal_set_id(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:TipInfoMessage.id)
}
inline ::uint32_t TipInfoMessage::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void TipInfoMessage::_internal_set_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// repeated string parameters = 2;
inline int TipInfoMessage::_internal_parameters_size() const {
  return _internal_parameters().size();
}
inline int TipInfoMessage::parameters_size() const {
  return _internal_parameters_size();
}
inline void TipInfoMessage::clear_parameters() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.parameters_.Clear();
}
inline std::string* PROTOBUF_NONNULL TipInfoMessage::add_parameters()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_parameters()->Add();
  // @@protoc_insertion_point(field_add_mutable:TipInfoMessage.parameters)
  return _s;
}
inline const std::string& TipInfoMessage::parameters(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:TipInfoMessage.parameters)
  return _internal_parameters().Get(index);
}
inline std::string* PROTOBUF_NONNULL TipInfoMessage::mutable_parameters(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:TipInfoMessage.parameters)
  return _internal_mutable_parameters()->Mutable(index);
}
template <typename Arg_, typename... Args_>
inline void TipInfoMessage::set_parameters(int index, Arg_&& value, Args_... args) {
  ::google::protobuf::internal::AssignToString(*_internal_mutable_parameters()->Mutable(index), std::forward<Arg_>(value),
                        args... );
  // @@protoc_insertion_point(field_set:TipInfoMessage.parameters)
}
template <typename Arg_, typename... Args_>
inline void TipInfoMessage::add_parameters(Arg_&& value, Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::google::protobuf::internal::AddToRepeatedPtrField(*_internal_mutable_parameters(),
                               std::forward<Arg_>(value),
                               args... );
  // @@protoc_insertion_point(field_add:TipInfoMessage.parameters)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>& TipInfoMessage::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:TipInfoMessage.parameters)
  return _internal_parameters();
}
inline ::google::protobuf::RepeatedPtrField<std::string>* PROTOBUF_NONNULL
TipInfoMessage::mutable_parameters() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:TipInfoMessage.parameters)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_parameters();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
TipInfoMessage::_internal_parameters() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.parameters_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>* PROTOBUF_NONNULL
TipInfoMessage::_internal_mutable_parameters() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.parameters_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2fcommon_2ftip_2eproto_2epb_2eh
