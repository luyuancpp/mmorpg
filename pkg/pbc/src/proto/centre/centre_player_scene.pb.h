// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/centre/centre_player_scene.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto_2epb_2eh
#define proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto_2epb_2eh

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
#include "google/protobuf/generated_message_bases.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/service.h"
#include "google/protobuf/unknown_field_set.h"
#include "google/protobuf/empty.pb.h"
#include "proto/common/common.pb.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto;
class CentreEnterSceneRequest;
struct CentreEnterSceneRequestDefaultTypeInternal;
extern CentreEnterSceneRequestDefaultTypeInternal _CentreEnterSceneRequest_default_instance_;
class CentreLeaveSceneAsyncSavePlayerCompleteRequest;
struct CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal;
extern CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal _CentreLeaveSceneAsyncSavePlayerCompleteRequest_default_instance_;
class CentreLeaveSceneRequest;
struct CentreLeaveSceneRequestDefaultTypeInternal;
extern CentreLeaveSceneRequestDefaultTypeInternal _CentreLeaveSceneRequest_default_instance_;
class CentreSceneInfoRequest;
struct CentreSceneInfoRequestDefaultTypeInternal;
extern CentreSceneInfoRequestDefaultTypeInternal _CentreSceneInfoRequest_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class CentreSceneInfoRequest final
    : public ::google::protobuf::internal::ZeroFieldsBase
/* @@protoc_insertion_point(class_definition:CentreSceneInfoRequest) */ {
 public:
  inline CentreSceneInfoRequest() : CentreSceneInfoRequest(nullptr) {}

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CentreSceneInfoRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CentreSceneInfoRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CentreSceneInfoRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline CentreSceneInfoRequest(const CentreSceneInfoRequest& from) : CentreSceneInfoRequest(nullptr, from) {}
  inline CentreSceneInfoRequest(CentreSceneInfoRequest&& from) noexcept
      : CentreSceneInfoRequest(nullptr, std::move(from)) {}
  inline CentreSceneInfoRequest& operator=(const CentreSceneInfoRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline CentreSceneInfoRequest& operator=(CentreSceneInfoRequest&& from) noexcept {
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
  static const CentreSceneInfoRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const CentreSceneInfoRequest* internal_default_instance() {
    return reinterpret_cast<const CentreSceneInfoRequest*>(
        &_CentreSceneInfoRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(CentreSceneInfoRequest& a, CentreSceneInfoRequest& b) { a.Swap(&b); }
  inline void Swap(CentreSceneInfoRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CentreSceneInfoRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CentreSceneInfoRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::internal::ZeroFieldsBase::DefaultConstruct<CentreSceneInfoRequest>(arena);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::CopyFrom;
  inline void CopyFrom(const CentreSceneInfoRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::CopyImpl(*this, from);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::MergeFrom;
  void MergeFrom(const CentreSceneInfoRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::MergeImpl(*this, from);
  }

  public:
  bool IsInitialized() const {
    return true;
  }
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CentreSceneInfoRequest"; }

 protected:
  explicit CentreSceneInfoRequest(::google::protobuf::Arena* arena);
  CentreSceneInfoRequest(::google::protobuf::Arena* arena, const CentreSceneInfoRequest& from);
  CentreSceneInfoRequest(::google::protobuf::Arena* arena, CentreSceneInfoRequest&& from) noexcept
      : CentreSceneInfoRequest(arena) {
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
  // @@protoc_insertion_point(class_scope:CentreSceneInfoRequest)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 0, 0,
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
                          const CentreSceneInfoRequest& from_msg);
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  friend struct ::TableStruct_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto;
};
// -------------------------------------------------------------------

class CentreLeaveSceneRequest final
    : public ::google::protobuf::internal::ZeroFieldsBase
/* @@protoc_insertion_point(class_definition:CentreLeaveSceneRequest) */ {
 public:
  inline CentreLeaveSceneRequest() : CentreLeaveSceneRequest(nullptr) {}

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CentreLeaveSceneRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CentreLeaveSceneRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CentreLeaveSceneRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline CentreLeaveSceneRequest(const CentreLeaveSceneRequest& from) : CentreLeaveSceneRequest(nullptr, from) {}
  inline CentreLeaveSceneRequest(CentreLeaveSceneRequest&& from) noexcept
      : CentreLeaveSceneRequest(nullptr, std::move(from)) {}
  inline CentreLeaveSceneRequest& operator=(const CentreLeaveSceneRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline CentreLeaveSceneRequest& operator=(CentreLeaveSceneRequest&& from) noexcept {
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
  static const CentreLeaveSceneRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const CentreLeaveSceneRequest* internal_default_instance() {
    return reinterpret_cast<const CentreLeaveSceneRequest*>(
        &_CentreLeaveSceneRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(CentreLeaveSceneRequest& a, CentreLeaveSceneRequest& b) { a.Swap(&b); }
  inline void Swap(CentreLeaveSceneRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CentreLeaveSceneRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CentreLeaveSceneRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::internal::ZeroFieldsBase::DefaultConstruct<CentreLeaveSceneRequest>(arena);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::CopyFrom;
  inline void CopyFrom(const CentreLeaveSceneRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::CopyImpl(*this, from);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::MergeFrom;
  void MergeFrom(const CentreLeaveSceneRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::MergeImpl(*this, from);
  }

  public:
  bool IsInitialized() const {
    return true;
  }
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CentreLeaveSceneRequest"; }

 protected:
  explicit CentreLeaveSceneRequest(::google::protobuf::Arena* arena);
  CentreLeaveSceneRequest(::google::protobuf::Arena* arena, const CentreLeaveSceneRequest& from);
  CentreLeaveSceneRequest(::google::protobuf::Arena* arena, CentreLeaveSceneRequest&& from) noexcept
      : CentreLeaveSceneRequest(arena) {
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
  // @@protoc_insertion_point(class_scope:CentreLeaveSceneRequest)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 0, 0,
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
                          const CentreLeaveSceneRequest& from_msg);
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  friend struct ::TableStruct_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto;
};
// -------------------------------------------------------------------

class CentreLeaveSceneAsyncSavePlayerCompleteRequest final
    : public ::google::protobuf::internal::ZeroFieldsBase
/* @@protoc_insertion_point(class_definition:CentreLeaveSceneAsyncSavePlayerCompleteRequest) */ {
 public:
  inline CentreLeaveSceneAsyncSavePlayerCompleteRequest() : CentreLeaveSceneAsyncSavePlayerCompleteRequest(nullptr) {}

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CentreLeaveSceneAsyncSavePlayerCompleteRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CentreLeaveSceneAsyncSavePlayerCompleteRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CentreLeaveSceneAsyncSavePlayerCompleteRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline CentreLeaveSceneAsyncSavePlayerCompleteRequest(const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from) : CentreLeaveSceneAsyncSavePlayerCompleteRequest(nullptr, from) {}
  inline CentreLeaveSceneAsyncSavePlayerCompleteRequest(CentreLeaveSceneAsyncSavePlayerCompleteRequest&& from) noexcept
      : CentreLeaveSceneAsyncSavePlayerCompleteRequest(nullptr, std::move(from)) {}
  inline CentreLeaveSceneAsyncSavePlayerCompleteRequest& operator=(const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline CentreLeaveSceneAsyncSavePlayerCompleteRequest& operator=(CentreLeaveSceneAsyncSavePlayerCompleteRequest&& from) noexcept {
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
  static const CentreLeaveSceneAsyncSavePlayerCompleteRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const CentreLeaveSceneAsyncSavePlayerCompleteRequest* internal_default_instance() {
    return reinterpret_cast<const CentreLeaveSceneAsyncSavePlayerCompleteRequest*>(
        &_CentreLeaveSceneAsyncSavePlayerCompleteRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(CentreLeaveSceneAsyncSavePlayerCompleteRequest& a, CentreLeaveSceneAsyncSavePlayerCompleteRequest& b) { a.Swap(&b); }
  inline void Swap(CentreLeaveSceneAsyncSavePlayerCompleteRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CentreLeaveSceneAsyncSavePlayerCompleteRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CentreLeaveSceneAsyncSavePlayerCompleteRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::internal::ZeroFieldsBase::DefaultConstruct<CentreLeaveSceneAsyncSavePlayerCompleteRequest>(arena);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::CopyFrom;
  inline void CopyFrom(const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::CopyImpl(*this, from);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::MergeFrom;
  void MergeFrom(const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from) {
    ::google::protobuf::internal::ZeroFieldsBase::MergeImpl(*this, from);
  }

  public:
  bool IsInitialized() const {
    return true;
  }
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CentreLeaveSceneAsyncSavePlayerCompleteRequest"; }

 protected:
  explicit CentreLeaveSceneAsyncSavePlayerCompleteRequest(::google::protobuf::Arena* arena);
  CentreLeaveSceneAsyncSavePlayerCompleteRequest(::google::protobuf::Arena* arena, const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from);
  CentreLeaveSceneAsyncSavePlayerCompleteRequest(::google::protobuf::Arena* arena, CentreLeaveSceneAsyncSavePlayerCompleteRequest&& from) noexcept
      : CentreLeaveSceneAsyncSavePlayerCompleteRequest(arena) {
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
  // @@protoc_insertion_point(class_scope:CentreLeaveSceneAsyncSavePlayerCompleteRequest)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 0, 0,
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
                          const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from_msg);
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  friend struct ::TableStruct_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto;
};
// -------------------------------------------------------------------

class CentreEnterSceneRequest final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:CentreEnterSceneRequest) */ {
 public:
  inline CentreEnterSceneRequest() : CentreEnterSceneRequest(nullptr) {}
  ~CentreEnterSceneRequest() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(CentreEnterSceneRequest* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(CentreEnterSceneRequest));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CentreEnterSceneRequest(
      ::google::protobuf::internal::ConstantInitialized);

  inline CentreEnterSceneRequest(const CentreEnterSceneRequest& from) : CentreEnterSceneRequest(nullptr, from) {}
  inline CentreEnterSceneRequest(CentreEnterSceneRequest&& from) noexcept
      : CentreEnterSceneRequest(nullptr, std::move(from)) {}
  inline CentreEnterSceneRequest& operator=(const CentreEnterSceneRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline CentreEnterSceneRequest& operator=(CentreEnterSceneRequest&& from) noexcept {
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
  static const CentreEnterSceneRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const CentreEnterSceneRequest* internal_default_instance() {
    return reinterpret_cast<const CentreEnterSceneRequest*>(
        &_CentreEnterSceneRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(CentreEnterSceneRequest& a, CentreEnterSceneRequest& b) { a.Swap(&b); }
  inline void Swap(CentreEnterSceneRequest* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(CentreEnterSceneRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CentreEnterSceneRequest* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<CentreEnterSceneRequest>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const CentreEnterSceneRequest& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const CentreEnterSceneRequest& from) { CentreEnterSceneRequest::MergeImpl(*this, from); }

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
  void InternalSwap(CentreEnterSceneRequest* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "CentreEnterSceneRequest"; }

 protected:
  explicit CentreEnterSceneRequest(::google::protobuf::Arena* arena);
  CentreEnterSceneRequest(::google::protobuf::Arena* arena, const CentreEnterSceneRequest& from);
  CentreEnterSceneRequest(::google::protobuf::Arena* arena, CentreEnterSceneRequest&& from) noexcept
      : CentreEnterSceneRequest(arena) {
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
    kSceneInfoFieldNumber = 1,
  };
  // .SceneInfoPBComponent scene_info = 1;
  bool has_scene_info() const;
  void clear_scene_info() ;
  const ::SceneInfoPBComponent& scene_info() const;
  PROTOBUF_NODISCARD ::SceneInfoPBComponent* release_scene_info();
  ::SceneInfoPBComponent* mutable_scene_info();
  void set_allocated_scene_info(::SceneInfoPBComponent* value);
  void unsafe_arena_set_allocated_scene_info(::SceneInfoPBComponent* value);
  ::SceneInfoPBComponent* unsafe_arena_release_scene_info();

  private:
  const ::SceneInfoPBComponent& _internal_scene_info() const;
  ::SceneInfoPBComponent* _internal_mutable_scene_info();

  public:
  // @@protoc_insertion_point(class_scope:CentreEnterSceneRequest)
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
                          const CentreEnterSceneRequest& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::SceneInfoPBComponent* scene_info_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto;
};

// ===================================================================


// -------------------------------------------------------------------

class CentrePlayerScene_Stub;
class CentrePlayerScene : public ::google::protobuf::Service {
 protected:
  CentrePlayerScene() = default;

 public:
  using Stub = CentrePlayerScene_Stub;

  CentrePlayerScene(const CentrePlayerScene&) = delete;
  CentrePlayerScene& operator=(const CentrePlayerScene&) = delete;
  virtual ~CentrePlayerScene() = default;

  static const ::google::protobuf::ServiceDescriptor* descriptor();

  virtual void EnterScene(::google::protobuf::RpcController* controller,
                        const ::CentreEnterSceneRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void LeaveScene(::google::protobuf::RpcController* controller,
                        const ::CentreLeaveSceneRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void LeaveSceneAsyncSavePlayerComplete(::google::protobuf::RpcController* controller,
                        const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done);
  virtual void SceneInfoC2S(::google::protobuf::RpcController* controller,
                        const ::CentreSceneInfoRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done);

  // implements Service ----------------------------------------------
  const ::google::protobuf::ServiceDescriptor* GetDescriptor() override;

  void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                  ::google::protobuf::RpcController* controller,
                  const ::google::protobuf::Message* request,
                  ::google::protobuf::Message* response,
                  ::google::protobuf::Closure* done) override;

  const ::google::protobuf::Message& GetRequestPrototype(
      const ::google::protobuf::MethodDescriptor* method) const override;

  const ::google::protobuf::Message& GetResponsePrototype(
      const ::google::protobuf::MethodDescriptor* method) const override;
};

class CentrePlayerScene_Stub final : public CentrePlayerScene {
 public:
  CentrePlayerScene_Stub(::google::protobuf::RpcChannel* channel);
  CentrePlayerScene_Stub(::google::protobuf::RpcChannel* channel,
                   ::google::protobuf::Service::ChannelOwnership ownership);

  CentrePlayerScene_Stub(const CentrePlayerScene_Stub&) = delete;
  CentrePlayerScene_Stub& operator=(const CentrePlayerScene_Stub&) = delete;

  ~CentrePlayerScene_Stub() override;

  inline ::google::protobuf::RpcChannel* channel() { return channel_; }

  // implements CentrePlayerScene ------------------------------------------
  void EnterScene(::google::protobuf::RpcController* controller,
                        const ::CentreEnterSceneRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void LeaveScene(::google::protobuf::RpcController* controller,
                        const ::CentreLeaveSceneRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void LeaveSceneAsyncSavePlayerComplete(::google::protobuf::RpcController* controller,
                        const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done) override;
  void SceneInfoC2S(::google::protobuf::RpcController* controller,
                        const ::CentreSceneInfoRequest* request,
                        ::google::protobuf::Empty* response,
                        ::google::protobuf::Closure* done) override;

 private:
  ::google::protobuf::RpcChannel* channel_;
  bool owns_channel_;
};
// ===================================================================



// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// CentreEnterSceneRequest

// .SceneInfoPBComponent scene_info = 1;
inline bool CentreEnterSceneRequest::has_scene_info() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.scene_info_ != nullptr);
  return value;
}
inline const ::SceneInfoPBComponent& CentreEnterSceneRequest::_internal_scene_info() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::SceneInfoPBComponent* p = _impl_.scene_info_;
  return p != nullptr ? *p : reinterpret_cast<const ::SceneInfoPBComponent&>(::_SceneInfoPBComponent_default_instance_);
}
inline const ::SceneInfoPBComponent& CentreEnterSceneRequest::scene_info() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:CentreEnterSceneRequest.scene_info)
  return _internal_scene_info();
}
inline void CentreEnterSceneRequest::unsafe_arena_set_allocated_scene_info(::SceneInfoPBComponent* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_);
  }
  _impl_.scene_info_ = reinterpret_cast<::SceneInfoPBComponent*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:CentreEnterSceneRequest.scene_info)
}
inline ::SceneInfoPBComponent* CentreEnterSceneRequest::release_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::SceneInfoPBComponent* released = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
  if (::google::protobuf::internal::DebugHardenForceCopyInRelease()) {
    auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
    if (GetArena() == nullptr) {
      delete old;
    }
  } else {
    if (GetArena() != nullptr) {
      released = ::google::protobuf::internal::DuplicateIfNonNull(released);
    }
  }
  return released;
}
inline ::SceneInfoPBComponent* CentreEnterSceneRequest::unsafe_arena_release_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:CentreEnterSceneRequest.scene_info)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::SceneInfoPBComponent* temp = _impl_.scene_info_;
  _impl_.scene_info_ = nullptr;
  return temp;
}
inline ::SceneInfoPBComponent* CentreEnterSceneRequest::_internal_mutable_scene_info() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.scene_info_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::SceneInfoPBComponent>(GetArena());
    _impl_.scene_info_ = reinterpret_cast<::SceneInfoPBComponent*>(p);
  }
  return _impl_.scene_info_;
}
inline ::SceneInfoPBComponent* CentreEnterSceneRequest::mutable_scene_info() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::SceneInfoPBComponent* _msg = _internal_mutable_scene_info();
  // @@protoc_insertion_point(field_mutable:CentreEnterSceneRequest.scene_info)
  return _msg;
}
inline void CentreEnterSceneRequest::set_allocated_scene_info(::SceneInfoPBComponent* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.scene_info_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = reinterpret_cast<::google::protobuf::MessageLite*>(value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }

  _impl_.scene_info_ = reinterpret_cast<::SceneInfoPBComponent*>(value);
  // @@protoc_insertion_point(field_set_allocated:CentreEnterSceneRequest.scene_info)
}

// -------------------------------------------------------------------

// CentreLeaveSceneRequest

// -------------------------------------------------------------------

// CentreLeaveSceneAsyncSavePlayerCompleteRequest

// -------------------------------------------------------------------

// CentreSceneInfoRequest

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2fcentre_2fcentre_5fplayer_5fscene_2eproto_2epb_2eh
