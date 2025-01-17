// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: messagelimiter_config.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_messagelimiter_5fconfig_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_messagelimiter_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_messagelimiter_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_messagelimiter_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_messagelimiter_5fconfig_2eproto;
class MessageLimiterTable;
struct MessageLimiterTableDefaultTypeInternal;
extern MessageLimiterTableDefaultTypeInternal _MessageLimiterTable_default_instance_;
class MessageLimiterTabledData;
struct MessageLimiterTabledDataDefaultTypeInternal;
extern MessageLimiterTabledDataDefaultTypeInternal _MessageLimiterTabledData_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class MessageLimiterTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:MessageLimiterTable) */ {
 public:
  inline MessageLimiterTable() : MessageLimiterTable(nullptr) {}
  ~MessageLimiterTable() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MessageLimiterTable(
      ::google::protobuf::internal::ConstantInitialized);

  inline MessageLimiterTable(const MessageLimiterTable& from) : MessageLimiterTable(nullptr, from) {}
  inline MessageLimiterTable(MessageLimiterTable&& from) noexcept
      : MessageLimiterTable(nullptr, std::move(from)) {}
  inline MessageLimiterTable& operator=(const MessageLimiterTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline MessageLimiterTable& operator=(MessageLimiterTable&& from) noexcept {
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
  static const MessageLimiterTable& default_instance() {
    return *internal_default_instance();
  }
  static inline const MessageLimiterTable* internal_default_instance() {
    return reinterpret_cast<const MessageLimiterTable*>(
        &_MessageLimiterTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(MessageLimiterTable& a, MessageLimiterTable& b) { a.Swap(&b); }
  inline void Swap(MessageLimiterTable* other) {
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
  void UnsafeArenaSwap(MessageLimiterTable* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  MessageLimiterTable* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<MessageLimiterTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const MessageLimiterTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const MessageLimiterTable& from) { MessageLimiterTable::MergeImpl(*this, from); }

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
  void InternalSwap(MessageLimiterTable* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "MessageLimiterTable"; }

 protected:
  explicit MessageLimiterTable(::google::protobuf::Arena* arena);
  MessageLimiterTable(::google::protobuf::Arena* arena, const MessageLimiterTable& from);
  MessageLimiterTable(::google::protobuf::Arena* arena, MessageLimiterTable&& from) noexcept
      : MessageLimiterTable(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kIdFieldNumber = 1,
    kMaxRequestsFieldNumber = 2,
    kTimeWindowFieldNumber = 3,
    kTipMessageFieldNumber = 4,
  };
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // uint32 MaxRequests = 2;
  void clear_maxrequests() ;
  ::uint32_t maxrequests() const;
  void set_maxrequests(::uint32_t value);

  private:
  ::uint32_t _internal_maxrequests() const;
  void _internal_set_maxrequests(::uint32_t value);

  public:
  // uint32 TimeWindow = 3;
  void clear_timewindow() ;
  ::uint32_t timewindow() const;
  void set_timewindow(::uint32_t value);

  private:
  ::uint32_t _internal_timewindow() const;
  void _internal_set_timewindow(::uint32_t value);

  public:
  // uint32 TipMessage = 4;
  void clear_tipmessage() ;
  ::uint32_t tipmessage() const;
  void set_tipmessage(::uint32_t value);

  private:
  ::uint32_t _internal_tipmessage() const;
  void _internal_set_tipmessage(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:MessageLimiterTable)
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
    ::uint32_t id_;
    ::uint32_t maxrequests_;
    ::uint32_t timewindow_;
    ::uint32_t tipmessage_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_messagelimiter_5fconfig_2eproto;
};
// -------------------------------------------------------------------

class MessageLimiterTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:MessageLimiterTabledData) */ {
 public:
  inline MessageLimiterTabledData() : MessageLimiterTabledData(nullptr) {}
  ~MessageLimiterTabledData() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MessageLimiterTabledData(
      ::google::protobuf::internal::ConstantInitialized);

  inline MessageLimiterTabledData(const MessageLimiterTabledData& from) : MessageLimiterTabledData(nullptr, from) {}
  inline MessageLimiterTabledData(MessageLimiterTabledData&& from) noexcept
      : MessageLimiterTabledData(nullptr, std::move(from)) {}
  inline MessageLimiterTabledData& operator=(const MessageLimiterTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline MessageLimiterTabledData& operator=(MessageLimiterTabledData&& from) noexcept {
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
  static const MessageLimiterTabledData& default_instance() {
    return *internal_default_instance();
  }
  static inline const MessageLimiterTabledData* internal_default_instance() {
    return reinterpret_cast<const MessageLimiterTabledData*>(
        &_MessageLimiterTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(MessageLimiterTabledData& a, MessageLimiterTabledData& b) { a.Swap(&b); }
  inline void Swap(MessageLimiterTabledData* other) {
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
  void UnsafeArenaSwap(MessageLimiterTabledData* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  MessageLimiterTabledData* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<MessageLimiterTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const MessageLimiterTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const MessageLimiterTabledData& from) { MessageLimiterTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(MessageLimiterTabledData* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "MessageLimiterTabledData"; }

 protected:
  explicit MessageLimiterTabledData(::google::protobuf::Arena* arena);
  MessageLimiterTabledData(::google::protobuf::Arena* arena, const MessageLimiterTabledData& from);
  MessageLimiterTabledData(::google::protobuf::Arena* arena, MessageLimiterTabledData&& from) noexcept
      : MessageLimiterTabledData(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kDataFieldNumber = 1,
  };
  // repeated .MessageLimiterTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::MessageLimiterTable* mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>* mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>* _internal_mutable_data();
  public:
  const ::MessageLimiterTable& data(int index) const;
  ::MessageLimiterTable* add_data();
  const ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>& data() const;
  // @@protoc_insertion_point(class_scope:MessageLimiterTabledData)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 1,
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
    ::google::protobuf::RepeatedPtrField< ::MessageLimiterTable > data_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_messagelimiter_5fconfig_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// MessageLimiterTable

// uint32 id = 1;
inline void MessageLimiterTable::clear_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = 0u;
}
inline ::uint32_t MessageLimiterTable::id() const {
  // @@protoc_insertion_point(field_get:MessageLimiterTable.id)
  return _internal_id();
}
inline void MessageLimiterTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:MessageLimiterTable.id)
}
inline ::uint32_t MessageLimiterTable::_internal_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.id_;
}
inline void MessageLimiterTable::_internal_set_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = value;
}

// uint32 MaxRequests = 2;
inline void MessageLimiterTable::clear_maxrequests() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.maxrequests_ = 0u;
}
inline ::uint32_t MessageLimiterTable::maxrequests() const {
  // @@protoc_insertion_point(field_get:MessageLimiterTable.MaxRequests)
  return _internal_maxrequests();
}
inline void MessageLimiterTable::set_maxrequests(::uint32_t value) {
  _internal_set_maxrequests(value);
  // @@protoc_insertion_point(field_set:MessageLimiterTable.MaxRequests)
}
inline ::uint32_t MessageLimiterTable::_internal_maxrequests() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.maxrequests_;
}
inline void MessageLimiterTable::_internal_set_maxrequests(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.maxrequests_ = value;
}

// uint32 TimeWindow = 3;
inline void MessageLimiterTable::clear_timewindow() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.timewindow_ = 0u;
}
inline ::uint32_t MessageLimiterTable::timewindow() const {
  // @@protoc_insertion_point(field_get:MessageLimiterTable.TimeWindow)
  return _internal_timewindow();
}
inline void MessageLimiterTable::set_timewindow(::uint32_t value) {
  _internal_set_timewindow(value);
  // @@protoc_insertion_point(field_set:MessageLimiterTable.TimeWindow)
}
inline ::uint32_t MessageLimiterTable::_internal_timewindow() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.timewindow_;
}
inline void MessageLimiterTable::_internal_set_timewindow(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.timewindow_ = value;
}

// uint32 TipMessage = 4;
inline void MessageLimiterTable::clear_tipmessage() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tipmessage_ = 0u;
}
inline ::uint32_t MessageLimiterTable::tipmessage() const {
  // @@protoc_insertion_point(field_get:MessageLimiterTable.TipMessage)
  return _internal_tipmessage();
}
inline void MessageLimiterTable::set_tipmessage(::uint32_t value) {
  _internal_set_tipmessage(value);
  // @@protoc_insertion_point(field_set:MessageLimiterTable.TipMessage)
}
inline ::uint32_t MessageLimiterTable::_internal_tipmessage() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.tipmessage_;
}
inline void MessageLimiterTable::_internal_set_tipmessage(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tipmessage_ = value;
}

// -------------------------------------------------------------------

// MessageLimiterTabledData

// repeated .MessageLimiterTable data = 1;
inline int MessageLimiterTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int MessageLimiterTabledData::data_size() const {
  return _internal_data_size();
}
inline void MessageLimiterTabledData::clear_data() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.data_.Clear();
}
inline ::MessageLimiterTable* MessageLimiterTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:MessageLimiterTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>* MessageLimiterTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:MessageLimiterTabledData.data)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_data();
}
inline const ::MessageLimiterTable& MessageLimiterTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:MessageLimiterTabledData.data)
  return _internal_data().Get(index);
}
inline ::MessageLimiterTable* MessageLimiterTabledData::add_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::MessageLimiterTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:MessageLimiterTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>& MessageLimiterTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:MessageLimiterTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>&
MessageLimiterTabledData::_internal_data() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::MessageLimiterTable>*
MessageLimiterTabledData::_internal_mutable_data() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_messagelimiter_5fconfig_2eproto_2epb_2eh
