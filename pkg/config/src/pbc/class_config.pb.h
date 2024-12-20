// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: class_config.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_class_5fconfig_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_class_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_class_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_class_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_class_5fconfig_2eproto;
class ClassTable;
struct ClassTableDefaultTypeInternal;
extern ClassTableDefaultTypeInternal _ClassTable_default_instance_;
class ClassTabledData;
struct ClassTabledDataDefaultTypeInternal;
extern ClassTabledDataDefaultTypeInternal _ClassTabledData_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class ClassTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ClassTable) */ {
 public:
  inline ClassTable() : ClassTable(nullptr) {}
  ~ClassTable() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ClassTable(
      ::google::protobuf::internal::ConstantInitialized);

  inline ClassTable(const ClassTable& from) : ClassTable(nullptr, from) {}
  inline ClassTable(ClassTable&& from) noexcept
      : ClassTable(nullptr, std::move(from)) {}
  inline ClassTable& operator=(const ClassTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline ClassTable& operator=(ClassTable&& from) noexcept {
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
  static const ClassTable& default_instance() {
    return *internal_default_instance();
  }
  static inline const ClassTable* internal_default_instance() {
    return reinterpret_cast<const ClassTable*>(
        &_ClassTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(ClassTable& a, ClassTable& b) { a.Swap(&b); }
  inline void Swap(ClassTable* other) {
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
  void UnsafeArenaSwap(ClassTable* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ClassTable* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<ClassTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ClassTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ClassTable& from) { ClassTable::MergeImpl(*this, from); }

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
  void InternalSwap(ClassTable* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "ClassTable"; }

 protected:
  explicit ClassTable(::google::protobuf::Arena* arena);
  ClassTable(::google::protobuf::Arena* arena, const ClassTable& from);
  ClassTable(::google::protobuf::Arena* arena, ClassTable&& from) noexcept
      : ClassTable(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSkillFieldNumber = 2,
    kIdFieldNumber = 1,
  };
  // repeated uint32 skill = 2;
  int skill_size() const;
  private:
  int _internal_skill_size() const;

  public:
  void clear_skill() ;
  ::uint32_t skill(int index) const;
  void set_skill(int index, ::uint32_t value);
  void add_skill(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& skill() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_skill();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_skill() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_skill();

  public:
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:ClassTable)
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
    ::google::protobuf::RepeatedField<::uint32_t> skill_;
    mutable ::google::protobuf::internal::CachedSize _skill_cached_byte_size_;
    ::uint32_t id_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_class_5fconfig_2eproto;
};
// -------------------------------------------------------------------

class ClassTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ClassTabledData) */ {
 public:
  inline ClassTabledData() : ClassTabledData(nullptr) {}
  ~ClassTabledData() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ClassTabledData(
      ::google::protobuf::internal::ConstantInitialized);

  inline ClassTabledData(const ClassTabledData& from) : ClassTabledData(nullptr, from) {}
  inline ClassTabledData(ClassTabledData&& from) noexcept
      : ClassTabledData(nullptr, std::move(from)) {}
  inline ClassTabledData& operator=(const ClassTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline ClassTabledData& operator=(ClassTabledData&& from) noexcept {
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
  static const ClassTabledData& default_instance() {
    return *internal_default_instance();
  }
  static inline const ClassTabledData* internal_default_instance() {
    return reinterpret_cast<const ClassTabledData*>(
        &_ClassTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(ClassTabledData& a, ClassTabledData& b) { a.Swap(&b); }
  inline void Swap(ClassTabledData* other) {
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
  void UnsafeArenaSwap(ClassTabledData* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ClassTabledData* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<ClassTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ClassTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ClassTabledData& from) { ClassTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(ClassTabledData* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "ClassTabledData"; }

 protected:
  explicit ClassTabledData(::google::protobuf::Arena* arena);
  ClassTabledData(::google::protobuf::Arena* arena, const ClassTabledData& from);
  ClassTabledData(::google::protobuf::Arena* arena, ClassTabledData&& from) noexcept
      : ClassTabledData(arena) {
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
  // repeated .ClassTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::ClassTable* mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::ClassTable>* mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::ClassTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::ClassTable>* _internal_mutable_data();
  public:
  const ::ClassTable& data(int index) const;
  ::ClassTable* add_data();
  const ::google::protobuf::RepeatedPtrField<::ClassTable>& data() const;
  // @@protoc_insertion_point(class_scope:ClassTabledData)
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
    ::google::protobuf::RepeatedPtrField< ::ClassTable > data_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_class_5fconfig_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// ClassTable

// uint32 id = 1;
inline void ClassTable::clear_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = 0u;
}
inline ::uint32_t ClassTable::id() const {
  // @@protoc_insertion_point(field_get:ClassTable.id)
  return _internal_id();
}
inline void ClassTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:ClassTable.id)
}
inline ::uint32_t ClassTable::_internal_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.id_;
}
inline void ClassTable::_internal_set_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = value;
}

// repeated uint32 skill = 2;
inline int ClassTable::_internal_skill_size() const {
  return _internal_skill().size();
}
inline int ClassTable::skill_size() const {
  return _internal_skill_size();
}
inline void ClassTable::clear_skill() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.skill_.Clear();
}
inline ::uint32_t ClassTable::skill(int index) const {
  // @@protoc_insertion_point(field_get:ClassTable.skill)
  return _internal_skill().Get(index);
}
inline void ClassTable::set_skill(int index, ::uint32_t value) {
  _internal_mutable_skill()->Set(index, value);
  // @@protoc_insertion_point(field_set:ClassTable.skill)
}
inline void ClassTable::add_skill(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _internal_mutable_skill()->Add(value);
  // @@protoc_insertion_point(field_add:ClassTable.skill)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& ClassTable::skill() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:ClassTable.skill)
  return _internal_skill();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* ClassTable::mutable_skill()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:ClassTable.skill)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_skill();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
ClassTable::_internal_skill() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.skill_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* ClassTable::_internal_mutable_skill() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.skill_;
}

// -------------------------------------------------------------------

// ClassTabledData

// repeated .ClassTable data = 1;
inline int ClassTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int ClassTabledData::data_size() const {
  return _internal_data_size();
}
inline void ClassTabledData::clear_data() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.data_.Clear();
}
inline ::ClassTable* ClassTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:ClassTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::ClassTable>* ClassTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:ClassTabledData.data)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_data();
}
inline const ::ClassTable& ClassTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:ClassTabledData.data)
  return _internal_data().Get(index);
}
inline ::ClassTable* ClassTabledData::add_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::ClassTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:ClassTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::ClassTable>& ClassTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:ClassTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::ClassTable>&
ClassTabledData::_internal_data() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::ClassTable>*
ClassTabledData::_internal_mutable_data() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_class_5fconfig_2eproto_2epb_2eh
