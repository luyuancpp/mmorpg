// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: cooldown_config.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef cooldown_5fconfig_2eproto_2epb_2eh
#define cooldown_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_cooldown_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_cooldown_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_cooldown_5fconfig_2eproto;
}  // extern "C"
class CooldownTable;
struct CooldownTableDefaultTypeInternal;
extern CooldownTableDefaultTypeInternal _CooldownTable_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CooldownTable_class_data_;
class CooldownTabledData;
struct CooldownTabledDataDefaultTypeInternal;
extern CooldownTabledDataDefaultTypeInternal _CooldownTabledData_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull CooldownTabledData_class_data_;
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
  ~CooldownTable() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CooldownTable* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CooldownTable));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CooldownTable(::google::protobuf::internal::ConstantInitialized);

  inline CooldownTable(const CooldownTable& from) : CooldownTable(nullptr, from) {}
  inline CooldownTable(CooldownTable&& from) noexcept
      : CooldownTable(nullptr, std::move(from)) {}
  inline CooldownTable& operator=(const CooldownTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline CooldownTable& operator=(CooldownTable&& from) noexcept {
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
  static const CooldownTable& default_instance() {
    return *reinterpret_cast<const CooldownTable*>(
        &_CooldownTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(CooldownTable& a, CooldownTable& b) { a.Swap(&b); }
  inline void Swap(CooldownTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CooldownTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CooldownTable* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CooldownTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CooldownTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CooldownTable& from) { CooldownTable::MergeImpl(*this, from); }

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
  void InternalSwap(CooldownTable* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CooldownTable"; }

 protected:
  explicit CooldownTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  CooldownTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const CooldownTable& from);
  CooldownTable(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, CooldownTable&& from) noexcept
      : CooldownTable(arena) {
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
  static const ::google::protobuf::internal::TcParseTable<1, 2,
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
        const CooldownTable& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint32_t id_;
    ::uint32_t duration_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_cooldown_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull CooldownTable_class_data_;
// -------------------------------------------------------------------

class CooldownTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CooldownTabledData) */ {
 public:
  inline CooldownTabledData() : CooldownTabledData(nullptr) {}
  ~CooldownTabledData() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CooldownTabledData* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CooldownTabledData));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CooldownTabledData(::google::protobuf::internal::ConstantInitialized);

  inline CooldownTabledData(const CooldownTabledData& from) : CooldownTabledData(nullptr, from) {}
  inline CooldownTabledData(CooldownTabledData&& from) noexcept
      : CooldownTabledData(nullptr, std::move(from)) {}
  inline CooldownTabledData& operator=(const CooldownTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline CooldownTabledData& operator=(CooldownTabledData&& from) noexcept {
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
  static const CooldownTabledData& default_instance() {
    return *reinterpret_cast<const CooldownTabledData*>(
        &_CooldownTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(CooldownTabledData& a, CooldownTabledData& b) { a.Swap(&b); }
  inline void Swap(CooldownTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CooldownTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CooldownTabledData* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CooldownTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CooldownTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CooldownTabledData& from) { CooldownTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(CooldownTabledData* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CooldownTabledData"; }

 protected:
  explicit CooldownTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  CooldownTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const CooldownTabledData& from);
  CooldownTabledData(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, CooldownTabledData&& from) noexcept
      : CooldownTabledData(arena) {
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
    kDataFieldNumber = 1,
  };
  // repeated .CooldownTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::CooldownTable* PROTOBUF_NONNULL mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::CooldownTable>* PROTOBUF_NONNULL mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::CooldownTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::CooldownTable>* PROTOBUF_NONNULL _internal_mutable_data();
  public:
  const ::CooldownTable& data(int index) const;
  ::CooldownTable* PROTOBUF_NONNULL add_data();
  const ::google::protobuf::RepeatedPtrField<::CooldownTable>& data() const;
  // @@protoc_insertion_point(class_scope:CooldownTabledData)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<0, 1,
                                   1, 0,
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
        const CooldownTabledData& from_msg);
    ::google::protobuf::RepeatedPtrField< ::CooldownTable > data_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_cooldown_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull CooldownTabledData_class_data_;

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
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t CooldownTable::id() const {
  // @@protoc_insertion_point(field_get:CooldownTable.id)
  return _internal_id();
}
inline void CooldownTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:CooldownTable.id)
}
inline ::uint32_t CooldownTable::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void CooldownTable::_internal_set_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// uint32 duration = 2;
inline void CooldownTable::clear_duration() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.duration_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline ::uint32_t CooldownTable::duration() const {
  // @@protoc_insertion_point(field_get:CooldownTable.duration)
  return _internal_duration();
}
inline void CooldownTable::set_duration(::uint32_t value) {
  _internal_set_duration(value);
  _impl_._has_bits_[0] |= 0x00000002u;
  // @@protoc_insertion_point(field_set:CooldownTable.duration)
}
inline ::uint32_t CooldownTable::_internal_duration() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.duration_;
}
inline void CooldownTable::_internal_set_duration(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
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
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Clear();
}
inline ::CooldownTable* PROTOBUF_NONNULL CooldownTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:CooldownTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::CooldownTable>* PROTOBUF_NONNULL CooldownTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:CooldownTabledData.data)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_data();
}
inline const ::CooldownTable& CooldownTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:CooldownTabledData.data)
  return _internal_data().Get(index);
}
inline ::CooldownTable* PROTOBUF_NONNULL CooldownTabledData::add_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
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
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::CooldownTable>* PROTOBUF_NONNULL
CooldownTabledData::_internal_mutable_data() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // cooldown_5fconfig_2eproto_2epb_2eh
