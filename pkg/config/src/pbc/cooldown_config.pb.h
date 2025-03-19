// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: cooldown_config.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_cooldown_5fconfig_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_cooldown_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_cooldown_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_cooldown_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_cooldown_5fconfig_2eproto;
class CooldownTable;
struct CooldownTableDefaultTypeInternal;
extern CooldownTableDefaultTypeInternal _CooldownTable_default_instance_;
class CooldownTabledData;
struct CooldownTabledDataDefaultTypeInternal;
extern CooldownTabledDataDefaultTypeInternal _CooldownTabledData_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class CooldownTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CooldownTable) */ {
 public:
  inline CooldownTable() : CooldownTable(nullptr) {}
  ~CooldownTable() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CooldownTable(
      ::google::protobuf::internal::ConstantInitialized);

  inline CooldownTable(const CooldownTable& from) : CooldownTable(nullptr, from) {}
  inline CooldownTable(CooldownTable&& from) noexcept
      : CooldownTable(nullptr, std::move(from)) {}
  inline CooldownTable& operator=(const CooldownTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline CooldownTable& operator=(CooldownTable&& from) noexcept {
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
  static const CooldownTable& default_instance() {
    return *internal_default_instance();
  }
  static inline const CooldownTable* internal_default_instance() {
    return reinterpret_cast<const CooldownTable*>(
        &_CooldownTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(CooldownTable& a, CooldownTable& b) { a.Swap(&b); }
  inline void Swap(CooldownTable* other) {
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
  void UnsafeArenaSwap(CooldownTable* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CooldownTable* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<CooldownTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CooldownTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CooldownTable& from) { CooldownTable::MergeImpl(*this, from); }

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
  void InternalSwap(CooldownTable* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "CooldownTable"; }

 protected:
  explicit CooldownTable(::google::protobuf::Arena* arena);
  CooldownTable(::google::protobuf::Arena* arena, const CooldownTable& from);
  CooldownTable(::google::protobuf::Arena* arena, CooldownTable&& from) noexcept
      : CooldownTable(arena) {
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
    kDurationFieldNumber = 2,
  };
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // uint32 duration = 2;
  void clear_duration() ;
  ::uint32_t duration() const;
  void set_duration(::uint32_t value);

  private:
  ::uint32_t _internal_duration() const;
  void _internal_set_duration(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:CooldownTable)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
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
    ::uint32_t duration_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_cooldown_5fconfig_2eproto;
};
// -------------------------------------------------------------------

class CooldownTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CooldownTabledData) */ {
 public:
  inline CooldownTabledData() : CooldownTabledData(nullptr) {}
  ~CooldownTabledData() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CooldownTabledData(
      ::google::protobuf::internal::ConstantInitialized);

  inline CooldownTabledData(const CooldownTabledData& from) : CooldownTabledData(nullptr, from) {}
  inline CooldownTabledData(CooldownTabledData&& from) noexcept
      : CooldownTabledData(nullptr, std::move(from)) {}
  inline CooldownTabledData& operator=(const CooldownTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline CooldownTabledData& operator=(CooldownTabledData&& from) noexcept {
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
  static const CooldownTabledData& default_instance() {
    return *internal_default_instance();
  }
  static inline const CooldownTabledData* internal_default_instance() {
    return reinterpret_cast<const CooldownTabledData*>(
        &_CooldownTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(CooldownTabledData& a, CooldownTabledData& b) { a.Swap(&b); }
  inline void Swap(CooldownTabledData* other) {
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
  void UnsafeArenaSwap(CooldownTabledData* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CooldownTabledData* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<CooldownTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CooldownTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CooldownTabledData& from) { CooldownTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(CooldownTabledData* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "CooldownTabledData"; }

 protected:
  explicit CooldownTabledData(::google::protobuf::Arena* arena);
  CooldownTabledData(::google::protobuf::Arena* arena, const CooldownTabledData& from);
  CooldownTabledData(::google::protobuf::Arena* arena, CooldownTabledData&& from) noexcept
      : CooldownTabledData(arena) {
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
  // repeated .CooldownTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::CooldownTable* mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::CooldownTable>* mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::CooldownTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::CooldownTable>* _internal_mutable_data();
  public:
  const ::CooldownTable& data(int index) const;
  ::CooldownTable* add_data();
  const ::google::protobuf::RepeatedPtrField<::CooldownTable>& data() const;
  // @@protoc_insertion_point(class_scope:CooldownTabledData)
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
    ::google::protobuf::RepeatedPtrField< ::CooldownTable > data_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_cooldown_5fconfig_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// CooldownTable

// uint32 id = 1;
inline void CooldownTable::clear_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = 0u;
}
inline ::uint32_t CooldownTable::id() const {
  // @@protoc_insertion_point(field_get:CooldownTable.id)
  return _internal_id();
}
inline void CooldownTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:CooldownTable.id)
}
inline ::uint32_t CooldownTable::_internal_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.id_;
}
inline void CooldownTable::_internal_set_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = value;
}

// uint32 duration = 2;
inline void CooldownTable::clear_duration() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.duration_ = 0u;
}
inline ::uint32_t CooldownTable::duration() const {
  // @@protoc_insertion_point(field_get:CooldownTable.duration)
  return _internal_duration();
}
inline void CooldownTable::set_duration(::uint32_t value) {
  _internal_set_duration(value);
  // @@protoc_insertion_point(field_set:CooldownTable.duration)
}
inline ::uint32_t CooldownTable::_internal_duration() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.duration_;
}
inline void CooldownTable::_internal_set_duration(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.duration_ = value;
}

// -------------------------------------------------------------------

// CooldownTabledData

// repeated .CooldownTable data = 1;
inline int CooldownTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int CooldownTabledData::data_size() const {
  return _internal_data_size();
}
inline void CooldownTabledData::clear_data() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.data_.Clear();
}
inline ::CooldownTable* CooldownTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:CooldownTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::CooldownTable>* CooldownTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:CooldownTabledData.data)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_data();
}
inline const ::CooldownTable& CooldownTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:CooldownTabledData.data)
  return _internal_data().Get(index);
}
inline ::CooldownTable* CooldownTabledData::add_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::CooldownTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:CooldownTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::CooldownTable>& CooldownTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:CooldownTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::CooldownTable>&
CooldownTabledData::_internal_data() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::CooldownTable>*
CooldownTabledData::_internal_mutable_data() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_cooldown_5fconfig_2eproto_2epb_2eh
