// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: skillpermission_config.proto
// Protobuf C++ Version: 5.29.0

#ifndef skillpermission_5fconfig_2eproto_2epb_2eh
#define skillpermission_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_skillpermission_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_skillpermission_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_skillpermission_5fconfig_2eproto;
class SkillPermissionTable;
struct SkillPermissionTableDefaultTypeInternal;
extern SkillPermissionTableDefaultTypeInternal _SkillPermissionTable_default_instance_;
class SkillPermissionTabledData;
struct SkillPermissionTabledDataDefaultTypeInternal;
extern SkillPermissionTabledDataDefaultTypeInternal _SkillPermissionTabledData_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class SkillPermissionTable final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SkillPermissionTable) */ {
 public:
  inline SkillPermissionTable() : SkillPermissionTable(nullptr) {}
  ~SkillPermissionTable() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(SkillPermissionTable* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(SkillPermissionTable));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillPermissionTable(
      ::google::protobuf::internal::ConstantInitialized);

  inline SkillPermissionTable(const SkillPermissionTable& from) : SkillPermissionTable(nullptr, from) {}
  inline SkillPermissionTable(SkillPermissionTable&& from) noexcept
      : SkillPermissionTable(nullptr, std::move(from)) {}
  inline SkillPermissionTable& operator=(const SkillPermissionTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline SkillPermissionTable& operator=(SkillPermissionTable&& from) noexcept {
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
  static const SkillPermissionTable& default_instance() {
    return *internal_default_instance();
  }
  static inline const SkillPermissionTable* internal_default_instance() {
    return reinterpret_cast<const SkillPermissionTable*>(
        &_SkillPermissionTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(SkillPermissionTable& a, SkillPermissionTable& b) { a.Swap(&b); }
  inline void Swap(SkillPermissionTable* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SkillPermissionTable* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SkillPermissionTable* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<SkillPermissionTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SkillPermissionTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SkillPermissionTable& from) { SkillPermissionTable::MergeImpl(*this, from); }

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
  void InternalSwap(SkillPermissionTable* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "SkillPermissionTable"; }

 protected:
  explicit SkillPermissionTable(::google::protobuf::Arena* arena);
  SkillPermissionTable(::google::protobuf::Arena* arena, const SkillPermissionTable& from);
  SkillPermissionTable(::google::protobuf::Arena* arena, SkillPermissionTable&& from) noexcept
      : SkillPermissionTable(arena) {
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
    kSkilltypeFieldNumber = 2,
    kIdFieldNumber = 1,
  };
  // repeated uint32 skilltype = 2;
  int skilltype_size() const;
  private:
  int _internal_skilltype_size() const;

  public:
  void clear_skilltype() ;
  ::uint32_t skilltype(int index) const;
  void set_skilltype(int index, ::uint32_t value);
  void add_skilltype(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& skilltype() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_skilltype();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_skilltype() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_skilltype();

  public:
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:SkillPermissionTable)
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
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const SkillPermissionTable& from_msg);
    ::google::protobuf::RepeatedField<::uint32_t> skilltype_;
    ::google::protobuf::internal::CachedSize _skilltype_cached_byte_size_;
    ::uint32_t id_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_skillpermission_5fconfig_2eproto;
};
// -------------------------------------------------------------------

class SkillPermissionTabledData final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:SkillPermissionTabledData) */ {
 public:
  inline SkillPermissionTabledData() : SkillPermissionTabledData(nullptr) {}
  ~SkillPermissionTabledData() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(SkillPermissionTabledData* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(SkillPermissionTabledData));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR SkillPermissionTabledData(
      ::google::protobuf::internal::ConstantInitialized);

  inline SkillPermissionTabledData(const SkillPermissionTabledData& from) : SkillPermissionTabledData(nullptr, from) {}
  inline SkillPermissionTabledData(SkillPermissionTabledData&& from) noexcept
      : SkillPermissionTabledData(nullptr, std::move(from)) {}
  inline SkillPermissionTabledData& operator=(const SkillPermissionTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline SkillPermissionTabledData& operator=(SkillPermissionTabledData&& from) noexcept {
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
  static const SkillPermissionTabledData& default_instance() {
    return *internal_default_instance();
  }
  static inline const SkillPermissionTabledData* internal_default_instance() {
    return reinterpret_cast<const SkillPermissionTabledData*>(
        &_SkillPermissionTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(SkillPermissionTabledData& a, SkillPermissionTabledData& b) { a.Swap(&b); }
  inline void Swap(SkillPermissionTabledData* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(SkillPermissionTabledData* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  SkillPermissionTabledData* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<SkillPermissionTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const SkillPermissionTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const SkillPermissionTabledData& from) { SkillPermissionTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(SkillPermissionTabledData* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "SkillPermissionTabledData"; }

 protected:
  explicit SkillPermissionTabledData(::google::protobuf::Arena* arena);
  SkillPermissionTabledData(::google::protobuf::Arena* arena, const SkillPermissionTabledData& from);
  SkillPermissionTabledData(::google::protobuf::Arena* arena, SkillPermissionTabledData&& from) noexcept
      : SkillPermissionTabledData(arena) {
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
    kDataFieldNumber = 1,
  };
  // repeated .SkillPermissionTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::SkillPermissionTable* mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>* mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>* _internal_mutable_data();
  public:
  const ::SkillPermissionTable& data(int index) const;
  ::SkillPermissionTable* add_data();
  const ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>& data() const;
  // @@protoc_insertion_point(class_scope:SkillPermissionTabledData)
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
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const SkillPermissionTabledData& from_msg);
    ::google::protobuf::RepeatedPtrField< ::SkillPermissionTable > data_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_skillpermission_5fconfig_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// SkillPermissionTable

// uint32 id = 1;
inline void SkillPermissionTable::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0u;
}
inline ::uint32_t SkillPermissionTable::id() const {
  // @@protoc_insertion_point(field_get:SkillPermissionTable.id)
  return _internal_id();
}
inline void SkillPermissionTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:SkillPermissionTable.id)
}
inline ::uint32_t SkillPermissionTable::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void SkillPermissionTable::_internal_set_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// repeated uint32 skilltype = 2;
inline int SkillPermissionTable::_internal_skilltype_size() const {
  return _internal_skilltype().size();
}
inline int SkillPermissionTable::skilltype_size() const {
  return _internal_skilltype_size();
}
inline void SkillPermissionTable::clear_skilltype() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.skilltype_.Clear();
}
inline ::uint32_t SkillPermissionTable::skilltype(int index) const {
  // @@protoc_insertion_point(field_get:SkillPermissionTable.skilltype)
  return _internal_skilltype().Get(index);
}
inline void SkillPermissionTable::set_skilltype(int index, ::uint32_t value) {
  _internal_mutable_skilltype()->Set(index, value);
  // @@protoc_insertion_point(field_set:SkillPermissionTable.skilltype)
}
inline void SkillPermissionTable::add_skilltype(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_skilltype()->Add(value);
  // @@protoc_insertion_point(field_add:SkillPermissionTable.skilltype)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& SkillPermissionTable::skilltype() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:SkillPermissionTable.skilltype)
  return _internal_skilltype();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* SkillPermissionTable::mutable_skilltype()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:SkillPermissionTable.skilltype)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_skilltype();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
SkillPermissionTable::_internal_skilltype() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.skilltype_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* SkillPermissionTable::_internal_mutable_skilltype() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.skilltype_;
}

// -------------------------------------------------------------------

// SkillPermissionTabledData

// repeated .SkillPermissionTable data = 1;
inline int SkillPermissionTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int SkillPermissionTabledData::data_size() const {
  return _internal_data_size();
}
inline void SkillPermissionTabledData::clear_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Clear();
}
inline ::SkillPermissionTable* SkillPermissionTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:SkillPermissionTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>* SkillPermissionTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:SkillPermissionTabledData.data)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_data();
}
inline const ::SkillPermissionTable& SkillPermissionTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:SkillPermissionTabledData.data)
  return _internal_data().Get(index);
}
inline ::SkillPermissionTable* SkillPermissionTabledData::add_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::SkillPermissionTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:SkillPermissionTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>& SkillPermissionTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:SkillPermissionTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>&
SkillPermissionTabledData::_internal_data() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::SkillPermissionTable>*
SkillPermissionTabledData::_internal_mutable_data() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // skillpermission_5fconfig_2eproto_2epb_2eh
