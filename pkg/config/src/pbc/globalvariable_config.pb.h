// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: globalvariable_config.proto
// Protobuf C++ Version: 5.26.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_globalvariable_5fconfig_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_globalvariable_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_globalvariable_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_globalvariable_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_globalvariable_5fconfig_2eproto;
class GlobalVariableTable;
struct GlobalVariableTableDefaultTypeInternal;
extern GlobalVariableTableDefaultTypeInternal _GlobalVariableTable_default_instance_;
class GlobalVariableTabledData;
struct GlobalVariableTabledDataDefaultTypeInternal;
extern GlobalVariableTabledDataDefaultTypeInternal _GlobalVariableTabledData_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class GlobalVariableTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:GlobalVariableTable) */ {
 public:
  inline GlobalVariableTable() : GlobalVariableTable(nullptr) {}
  ~GlobalVariableTable() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR GlobalVariableTable(
      ::google::protobuf::internal::ConstantInitialized);

  inline GlobalVariableTable(const GlobalVariableTable& from) : GlobalVariableTable(nullptr, from) {}
  inline GlobalVariableTable(GlobalVariableTable&& from) noexcept
      : GlobalVariableTable(nullptr, std::move(from)) {}
  inline GlobalVariableTable& operator=(const GlobalVariableTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline GlobalVariableTable& operator=(GlobalVariableTable&& from) noexcept {
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
  static const GlobalVariableTable& default_instance() {
    return *internal_default_instance();
  }
  static inline const GlobalVariableTable* internal_default_instance() {
    return reinterpret_cast<const GlobalVariableTable*>(
        &_GlobalVariableTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(GlobalVariableTable& a, GlobalVariableTable& b) { a.Swap(&b); }
  inline void Swap(GlobalVariableTable* other) {
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
  void UnsafeArenaSwap(GlobalVariableTable* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  GlobalVariableTable* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<GlobalVariableTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const GlobalVariableTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const GlobalVariableTable& from) { GlobalVariableTable::MergeImpl(*this, from); }

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
  void InternalSwap(GlobalVariableTable* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "GlobalVariableTable"; }

 protected:
  explicit GlobalVariableTable(::google::protobuf::Arena* arena);
  GlobalVariableTable(::google::protobuf::Arena* arena, const GlobalVariableTable& from);
  GlobalVariableTable(::google::protobuf::Arena* arena, GlobalVariableTable&& from) noexcept
      : GlobalVariableTable(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData()
      const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const final;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kTostringFieldNumber = 4,
    kIdFieldNumber = 1,
    kTouint32FieldNumber = 2,
    kToint32FieldNumber = 3,
    kTofloatFieldNumber = 5,
    kTodoubleFieldNumber = 6,
  };
  // string tostring = 4;
  void clear_tostring() ;
  const std::string& tostring() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_tostring(Arg_&& arg, Args_... args);
  std::string* mutable_tostring();
  PROTOBUF_NODISCARD std::string* release_tostring();
  void set_allocated_tostring(std::string* value);

  private:
  const std::string& _internal_tostring() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_tostring(
      const std::string& value);
  std::string* _internal_mutable_tostring();

  public:
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // uint32 touint32 = 2;
  void clear_touint32() ;
  ::uint32_t touint32() const;
  void set_touint32(::uint32_t value);

  private:
  ::uint32_t _internal_touint32() const;
  void _internal_set_touint32(::uint32_t value);

  public:
  // int32 toint32 = 3;
  void clear_toint32() ;
  ::int32_t toint32() const;
  void set_toint32(::int32_t value);

  private:
  ::int32_t _internal_toint32() const;
  void _internal_set_toint32(::int32_t value);

  public:
  // float tofloat = 5;
  void clear_tofloat() ;
  float tofloat() const;
  void set_tofloat(float value);

  private:
  float _internal_tofloat() const;
  void _internal_set_tofloat(float value);

  public:
  // double todouble = 6;
  void clear_todouble() ;
  double todouble() const;
  void set_todouble(double value);

  private:
  double _internal_todouble() const;
  void _internal_set_todouble(double value);

  public:
  // @@protoc_insertion_point(class_scope:GlobalVariableTable)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 6, 0,
      36, 2>
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
    ::google::protobuf::internal::ArenaStringPtr tostring_;
    ::uint32_t id_;
    ::uint32_t touint32_;
    ::int32_t toint32_;
    float tofloat_;
    double todouble_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_globalvariable_5fconfig_2eproto;
};
// -------------------------------------------------------------------

class GlobalVariableTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:GlobalVariableTabledData) */ {
 public:
  inline GlobalVariableTabledData() : GlobalVariableTabledData(nullptr) {}
  ~GlobalVariableTabledData() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR GlobalVariableTabledData(
      ::google::protobuf::internal::ConstantInitialized);

  inline GlobalVariableTabledData(const GlobalVariableTabledData& from) : GlobalVariableTabledData(nullptr, from) {}
  inline GlobalVariableTabledData(GlobalVariableTabledData&& from) noexcept
      : GlobalVariableTabledData(nullptr, std::move(from)) {}
  inline GlobalVariableTabledData& operator=(const GlobalVariableTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline GlobalVariableTabledData& operator=(GlobalVariableTabledData&& from) noexcept {
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
  static const GlobalVariableTabledData& default_instance() {
    return *internal_default_instance();
  }
  static inline const GlobalVariableTabledData* internal_default_instance() {
    return reinterpret_cast<const GlobalVariableTabledData*>(
        &_GlobalVariableTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(GlobalVariableTabledData& a, GlobalVariableTabledData& b) { a.Swap(&b); }
  inline void Swap(GlobalVariableTabledData* other) {
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
  void UnsafeArenaSwap(GlobalVariableTabledData* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  GlobalVariableTabledData* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<GlobalVariableTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const GlobalVariableTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const GlobalVariableTabledData& from) { GlobalVariableTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(GlobalVariableTabledData* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "GlobalVariableTabledData"; }

 protected:
  explicit GlobalVariableTabledData(::google::protobuf::Arena* arena);
  GlobalVariableTabledData(::google::protobuf::Arena* arena, const GlobalVariableTabledData& from);
  GlobalVariableTabledData(::google::protobuf::Arena* arena, GlobalVariableTabledData&& from) noexcept
      : GlobalVariableTabledData(arena) {
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
  // repeated .GlobalVariableTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::GlobalVariableTable* mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>* mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>* _internal_mutable_data();
  public:
  const ::GlobalVariableTable& data(int index) const;
  ::GlobalVariableTable* add_data();
  const ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>& data() const;
  // @@protoc_insertion_point(class_scope:GlobalVariableTabledData)
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
    ::google::protobuf::RepeatedPtrField< ::GlobalVariableTable > data_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_globalvariable_5fconfig_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// GlobalVariableTable

// uint32 id = 1;
inline void GlobalVariableTable::clear_id() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = 0u;
}
inline ::uint32_t GlobalVariableTable::id() const {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.id)
  return _internal_id();
}
inline void GlobalVariableTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:GlobalVariableTable.id)
}
inline ::uint32_t GlobalVariableTable::_internal_id() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.id_;
}
inline void GlobalVariableTable::_internal_set_id(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.id_ = value;
}

// uint32 touint32 = 2;
inline void GlobalVariableTable::clear_touint32() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.touint32_ = 0u;
}
inline ::uint32_t GlobalVariableTable::touint32() const {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.touint32)
  return _internal_touint32();
}
inline void GlobalVariableTable::set_touint32(::uint32_t value) {
  _internal_set_touint32(value);
  // @@protoc_insertion_point(field_set:GlobalVariableTable.touint32)
}
inline ::uint32_t GlobalVariableTable::_internal_touint32() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.touint32_;
}
inline void GlobalVariableTable::_internal_set_touint32(::uint32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.touint32_ = value;
}

// int32 toint32 = 3;
inline void GlobalVariableTable::clear_toint32() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.toint32_ = 0;
}
inline ::int32_t GlobalVariableTable::toint32() const {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.toint32)
  return _internal_toint32();
}
inline void GlobalVariableTable::set_toint32(::int32_t value) {
  _internal_set_toint32(value);
  // @@protoc_insertion_point(field_set:GlobalVariableTable.toint32)
}
inline ::int32_t GlobalVariableTable::_internal_toint32() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.toint32_;
}
inline void GlobalVariableTable::_internal_set_toint32(::int32_t value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.toint32_ = value;
}

// string tostring = 4;
inline void GlobalVariableTable::clear_tostring() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tostring_.ClearToEmpty();
}
inline const std::string& GlobalVariableTable::tostring() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.tostring)
  return _internal_tostring();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void GlobalVariableTable::set_tostring(Arg_&& arg,
                                                     Args_... args) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tostring_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:GlobalVariableTable.tostring)
}
inline std::string* GlobalVariableTable::mutable_tostring() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_tostring();
  // @@protoc_insertion_point(field_mutable:GlobalVariableTable.tostring)
  return _s;
}
inline const std::string& GlobalVariableTable::_internal_tostring() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.tostring_.Get();
}
inline void GlobalVariableTable::_internal_set_tostring(const std::string& value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tostring_.Set(value, GetArena());
}
inline std::string* GlobalVariableTable::_internal_mutable_tostring() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _impl_.tostring_.Mutable( GetArena());
}
inline std::string* GlobalVariableTable::release_tostring() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:GlobalVariableTable.tostring)
  return _impl_.tostring_.Release();
}
inline void GlobalVariableTable::set_allocated_tostring(std::string* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tostring_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.tostring_.IsDefault()) {
          _impl_.tostring_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:GlobalVariableTable.tostring)
}

// float tofloat = 5;
inline void GlobalVariableTable::clear_tofloat() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tofloat_ = 0;
}
inline float GlobalVariableTable::tofloat() const {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.tofloat)
  return _internal_tofloat();
}
inline void GlobalVariableTable::set_tofloat(float value) {
  _internal_set_tofloat(value);
  // @@protoc_insertion_point(field_set:GlobalVariableTable.tofloat)
}
inline float GlobalVariableTable::_internal_tofloat() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.tofloat_;
}
inline void GlobalVariableTable::_internal_set_tofloat(float value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.tofloat_ = value;
}

// double todouble = 6;
inline void GlobalVariableTable::clear_todouble() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.todouble_ = 0;
}
inline double GlobalVariableTable::todouble() const {
  // @@protoc_insertion_point(field_get:GlobalVariableTable.todouble)
  return _internal_todouble();
}
inline void GlobalVariableTable::set_todouble(double value) {
  _internal_set_todouble(value);
  // @@protoc_insertion_point(field_set:GlobalVariableTable.todouble)
}
inline double GlobalVariableTable::_internal_todouble() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.todouble_;
}
inline void GlobalVariableTable::_internal_set_todouble(double value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.todouble_ = value;
}

// -------------------------------------------------------------------

// GlobalVariableTabledData

// repeated .GlobalVariableTable data = 1;
inline int GlobalVariableTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int GlobalVariableTabledData::data_size() const {
  return _internal_data_size();
}
inline void GlobalVariableTabledData::clear_data() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.data_.Clear();
}
inline ::GlobalVariableTable* GlobalVariableTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:GlobalVariableTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>* GlobalVariableTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:GlobalVariableTabledData.data)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  return _internal_mutable_data();
}
inline const ::GlobalVariableTable& GlobalVariableTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:GlobalVariableTabledData.data)
  return _internal_data().Get(index);
}
inline ::GlobalVariableTable* GlobalVariableTabledData::add_data() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::GlobalVariableTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:GlobalVariableTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>& GlobalVariableTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:GlobalVariableTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>&
GlobalVariableTabledData::_internal_data() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::GlobalVariableTable>*
GlobalVariableTabledData::_internal_mutable_data() {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_globalvariable_5fconfig_2eproto_2epb_2eh
