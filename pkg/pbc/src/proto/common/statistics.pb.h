// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/common/statistics.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fstatistics_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fstatistics_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_proto_2fcommon_2fstatistics_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2fcommon_2fstatistics_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2fcommon_2fstatistics_2eproto;
class MessageStatistics;
struct MessageStatisticsDefaultTypeInternal;
extern MessageStatisticsDefaultTypeInternal _MessageStatistics_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class MessageStatistics final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:MessageStatistics) */ {
 public:
  inline MessageStatistics() : MessageStatistics(nullptr) {}
  ~MessageStatistics() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MessageStatistics(
      ::google::protobuf::internal::ConstantInitialized);

  inline MessageStatistics(const MessageStatistics& from) : MessageStatistics(nullptr, from) {}
  inline MessageStatistics(MessageStatistics&& from) noexcept
      : MessageStatistics(nullptr, std::move(from)) {}
  inline MessageStatistics& operator=(const MessageStatistics& from) {
    CopyFrom(from);
    return *this;
  }
  inline MessageStatistics& operator=(MessageStatistics&& from) noexcept {
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
  static const MessageStatistics& default_instance() {
    return *internal_default_instance();
  }
  static inline const MessageStatistics* internal_default_instance() {
    return reinterpret_cast<const MessageStatistics*>(
        &_MessageStatistics_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(MessageStatistics& a, MessageStatistics& b) { a.Swap(&b); }
  inline void Swap(MessageStatistics* other) {
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
  void UnsafeArenaSwap(MessageStatistics* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  MessageStatistics* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<MessageStatistics>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const MessageStatistics& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const MessageStatistics& from) { MessageStatistics::MergeImpl(*this, from); }

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
  void InternalSwap(MessageStatistics* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "MessageStatistics"; }

 protected:
  explicit MessageStatistics(::google::protobuf::Arena* arena);
  MessageStatistics(::google::protobuf::Arena* arena, const MessageStatistics& from);
  MessageStatistics(::google::protobuf::Arena* arena, MessageStatistics&& from) noexcept
      : MessageStatistics(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kCountFieldNumber = 1,
    kFlowRateSecondFieldNumber = 2,
    kFlowRateTotalFieldNumber = 3,
    kSecondFieldNumber = 4,
  };
  // uint32 count = 1;
  void clear_count() ;
  ::uint32_t count() const;
  void set_count(::uint32_t value);

  private:
  ::uint32_t _internal_count() const;
  void _internal_set_count(::uint32_t value);

  public:
  // uint32 flow_rate_second = 2;
  void clear_flow_rate_second() ;
  ::uint32_t flow_rate_second() const;
  void set_flow_rate_second(::uint32_t value);

  private:
  ::uint32_t _internal_flow_rate_second() const;
  void _internal_set_flow_rate_second(::uint32_t value);

  public:
  // uint32 flow_rate_total = 3;
  void clear_flow_rate_total() ;
  ::uint32_t flow_rate_total() const;
  void set_flow_rate_total(::uint32_t value);

  private:
  ::uint32_t _internal_flow_rate_total() const;
  void _internal_set_flow_rate_total(::uint32_t value);

  public:
  // uint32 second = 4;
  void clear_second() ;
  ::uint32_t second() const;
  void set_second(::uint32_t value);

  private:
  ::uint32_t _internal_second() const;
  void _internal_set_second(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:MessageStatistics)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 4, 0,
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
    ::uint32_t count_;
    ::uint32_t flow_rate_second_;
    ::uint32_t flow_rate_total_;
    ::uint32_t second_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fcommon_2fstatistics_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// MessageStatistics

// uint32 count = 1;
inline void MessageStatistics::clear_count() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.count_ = 0u;
}
inline ::uint32_t MessageStatistics::count() const {
  // @@protoc_insertion_point(field_get:MessageStatistics.count)
  return _internal_count();
}
inline void MessageStatistics::set_count(::uint32_t value) {
  _internal_set_count(value);
  // @@protoc_insertion_point(field_set:MessageStatistics.count)
}
inline ::uint32_t MessageStatistics::_internal_count() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.count_;
}
inline void MessageStatistics::_internal_set_count(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.count_ = value;
}

// uint32 flow_rate_second = 2;
inline void MessageStatistics::clear_flow_rate_second() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.flow_rate_second_ = 0u;
}
inline ::uint32_t MessageStatistics::flow_rate_second() const {
  // @@protoc_insertion_point(field_get:MessageStatistics.flow_rate_second)
  return _internal_flow_rate_second();
}
inline void MessageStatistics::set_flow_rate_second(::uint32_t value) {
  _internal_set_flow_rate_second(value);
  // @@protoc_insertion_point(field_set:MessageStatistics.flow_rate_second)
}
inline ::uint32_t MessageStatistics::_internal_flow_rate_second() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.flow_rate_second_;
}
inline void MessageStatistics::_internal_set_flow_rate_second(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.flow_rate_second_ = value;
}

// uint32 flow_rate_total = 3;
inline void MessageStatistics::clear_flow_rate_total() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.flow_rate_total_ = 0u;
}
inline ::uint32_t MessageStatistics::flow_rate_total() const {
  // @@protoc_insertion_point(field_get:MessageStatistics.flow_rate_total)
  return _internal_flow_rate_total();
}
inline void MessageStatistics::set_flow_rate_total(::uint32_t value) {
  _internal_set_flow_rate_total(value);
  // @@protoc_insertion_point(field_set:MessageStatistics.flow_rate_total)
}
inline ::uint32_t MessageStatistics::_internal_flow_rate_total() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.flow_rate_total_;
}
inline void MessageStatistics::_internal_set_flow_rate_total(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.flow_rate_total_ = value;
}

// uint32 second = 4;
inline void MessageStatistics::clear_second() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.second_ = 0u;
}
inline ::uint32_t MessageStatistics::second() const {
  // @@protoc_insertion_point(field_get:MessageStatistics.second)
  return _internal_second();
}
inline void MessageStatistics::set_second(::uint32_t value) {
  _internal_set_second(value);
  // @@protoc_insertion_point(field_set:MessageStatistics.second)
}
inline ::uint32_t MessageStatistics::_internal_second() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.second_;
}
inline void MessageStatistics::_internal_set_second(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.second_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_proto_2fcommon_2fstatistics_2eproto_2epb_2eh
