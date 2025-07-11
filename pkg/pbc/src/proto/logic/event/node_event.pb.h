// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/node_event.proto
// Protobuf C++ Version: 5.29.0

#ifndef proto_2flogic_2fevent_2fnode_5fevent_2eproto_2epb_2eh
#define proto_2flogic_2fevent_2fnode_5fevent_2eproto_2epb_2eh

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
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_proto_2flogic_2fevent_2fnode_5fevent_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_proto_2flogic_2fevent_2fnode_5fevent_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_proto_2flogic_2fevent_2fnode_5fevent_2eproto;
class ConnectToNodePbEvent;
struct ConnectToNodePbEventDefaultTypeInternal;
extern ConnectToNodePbEventDefaultTypeInternal _ConnectToNodePbEvent_default_instance_;
class OnNodeAddPbEvent;
struct OnNodeAddPbEventDefaultTypeInternal;
extern OnNodeAddPbEventDefaultTypeInternal _OnNodeAddPbEvent_default_instance_;
class OnNodeConnectedPbEvent;
struct OnNodeConnectedPbEventDefaultTypeInternal;
extern OnNodeConnectedPbEventDefaultTypeInternal _OnNodeConnectedPbEvent_default_instance_;
class OnNodeRemovePbEvent;
struct OnNodeRemovePbEventDefaultTypeInternal;
extern OnNodeRemovePbEventDefaultTypeInternal _OnNodeRemovePbEvent_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class OnNodeRemovePbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:OnNodeRemovePbEvent) */ {
 public:
  inline OnNodeRemovePbEvent() : OnNodeRemovePbEvent(nullptr) {}
  ~OnNodeRemovePbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(OnNodeRemovePbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(OnNodeRemovePbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OnNodeRemovePbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline OnNodeRemovePbEvent(const OnNodeRemovePbEvent& from) : OnNodeRemovePbEvent(nullptr, from) {}
  inline OnNodeRemovePbEvent(OnNodeRemovePbEvent&& from) noexcept
      : OnNodeRemovePbEvent(nullptr, std::move(from)) {}
  inline OnNodeRemovePbEvent& operator=(const OnNodeRemovePbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline OnNodeRemovePbEvent& operator=(OnNodeRemovePbEvent&& from) noexcept {
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
  static const OnNodeRemovePbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const OnNodeRemovePbEvent* internal_default_instance() {
    return reinterpret_cast<const OnNodeRemovePbEvent*>(
        &_OnNodeRemovePbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(OnNodeRemovePbEvent& a, OnNodeRemovePbEvent& b) { a.Swap(&b); }
  inline void Swap(OnNodeRemovePbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(OnNodeRemovePbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OnNodeRemovePbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<OnNodeRemovePbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OnNodeRemovePbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OnNodeRemovePbEvent& from) { OnNodeRemovePbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(OnNodeRemovePbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "OnNodeRemovePbEvent"; }

 protected:
  explicit OnNodeRemovePbEvent(::google::protobuf::Arena* arena);
  OnNodeRemovePbEvent(::google::protobuf::Arena* arena, const OnNodeRemovePbEvent& from);
  OnNodeRemovePbEvent(::google::protobuf::Arena* arena, OnNodeRemovePbEvent&& from) noexcept
      : OnNodeRemovePbEvent(arena) {
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
    kEntityFieldNumber = 1,
    kNodeTypeFieldNumber = 2,
    kSceneNodeTypeFieldNumber = 3,
  };
  // uint64 entity = 1;
  void clear_entity() ;
  ::uint64_t entity() const;
  void set_entity(::uint64_t value);

  private:
  ::uint64_t _internal_entity() const;
  void _internal_set_entity(::uint64_t value);

  public:
  // uint32 node_type = 2;
  void clear_node_type() ;
  ::uint32_t node_type() const;
  void set_node_type(::uint32_t value);

  private:
  ::uint32_t _internal_node_type() const;
  void _internal_set_node_type(::uint32_t value);

  public:
  // uint32 scene_node_type = 3;
  void clear_scene_node_type() ;
  ::uint32_t scene_node_type() const;
  void set_scene_node_type(::uint32_t value);

  private:
  ::uint32_t _internal_scene_node_type() const;
  void _internal_set_scene_node_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:OnNodeRemovePbEvent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
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
                          const OnNodeRemovePbEvent& from_msg);
    ::uint64_t entity_;
    ::uint32_t node_type_;
    ::uint32_t scene_node_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fnode_5fevent_2eproto;
};
// -------------------------------------------------------------------

class OnNodeConnectedPbEvent final
    : public ::google::protobuf::internal::ZeroFieldsBase
/* @@protoc_insertion_point(class_definition:OnNodeConnectedPbEvent) */ {
 public:
  inline OnNodeConnectedPbEvent() : OnNodeConnectedPbEvent(nullptr) {}

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(OnNodeConnectedPbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(OnNodeConnectedPbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OnNodeConnectedPbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline OnNodeConnectedPbEvent(const OnNodeConnectedPbEvent& from) : OnNodeConnectedPbEvent(nullptr, from) {}
  inline OnNodeConnectedPbEvent(OnNodeConnectedPbEvent&& from) noexcept
      : OnNodeConnectedPbEvent(nullptr, std::move(from)) {}
  inline OnNodeConnectedPbEvent& operator=(const OnNodeConnectedPbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline OnNodeConnectedPbEvent& operator=(OnNodeConnectedPbEvent&& from) noexcept {
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
  static const OnNodeConnectedPbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const OnNodeConnectedPbEvent* internal_default_instance() {
    return reinterpret_cast<const OnNodeConnectedPbEvent*>(
        &_OnNodeConnectedPbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(OnNodeConnectedPbEvent& a, OnNodeConnectedPbEvent& b) { a.Swap(&b); }
  inline void Swap(OnNodeConnectedPbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(OnNodeConnectedPbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OnNodeConnectedPbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::internal::ZeroFieldsBase::DefaultConstruct<OnNodeConnectedPbEvent>(arena);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::CopyFrom;
  inline void CopyFrom(const OnNodeConnectedPbEvent& from) {
    ::google::protobuf::internal::ZeroFieldsBase::CopyImpl(*this, from);
  }
  using ::google::protobuf::internal::ZeroFieldsBase::MergeFrom;
  void MergeFrom(const OnNodeConnectedPbEvent& from) {
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
  static ::absl::string_view FullMessageName() { return "OnNodeConnectedPbEvent"; }

 protected:
  explicit OnNodeConnectedPbEvent(::google::protobuf::Arena* arena);
  OnNodeConnectedPbEvent(::google::protobuf::Arena* arena, const OnNodeConnectedPbEvent& from);
  OnNodeConnectedPbEvent(::google::protobuf::Arena* arena, OnNodeConnectedPbEvent&& from) noexcept
      : OnNodeConnectedPbEvent(arena) {
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
  // @@protoc_insertion_point(class_scope:OnNodeConnectedPbEvent)
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
                          const OnNodeConnectedPbEvent& from_msg);
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fnode_5fevent_2eproto;
};
// -------------------------------------------------------------------

class OnNodeAddPbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:OnNodeAddPbEvent) */ {
 public:
  inline OnNodeAddPbEvent() : OnNodeAddPbEvent(nullptr) {}
  ~OnNodeAddPbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(OnNodeAddPbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(OnNodeAddPbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OnNodeAddPbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline OnNodeAddPbEvent(const OnNodeAddPbEvent& from) : OnNodeAddPbEvent(nullptr, from) {}
  inline OnNodeAddPbEvent(OnNodeAddPbEvent&& from) noexcept
      : OnNodeAddPbEvent(nullptr, std::move(from)) {}
  inline OnNodeAddPbEvent& operator=(const OnNodeAddPbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline OnNodeAddPbEvent& operator=(OnNodeAddPbEvent&& from) noexcept {
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
  static const OnNodeAddPbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const OnNodeAddPbEvent* internal_default_instance() {
    return reinterpret_cast<const OnNodeAddPbEvent*>(
        &_OnNodeAddPbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(OnNodeAddPbEvent& a, OnNodeAddPbEvent& b) { a.Swap(&b); }
  inline void Swap(OnNodeAddPbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(OnNodeAddPbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OnNodeAddPbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<OnNodeAddPbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OnNodeAddPbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OnNodeAddPbEvent& from) { OnNodeAddPbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(OnNodeAddPbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "OnNodeAddPbEvent"; }

 protected:
  explicit OnNodeAddPbEvent(::google::protobuf::Arena* arena);
  OnNodeAddPbEvent(::google::protobuf::Arena* arena, const OnNodeAddPbEvent& from);
  OnNodeAddPbEvent(::google::protobuf::Arena* arena, OnNodeAddPbEvent&& from) noexcept
      : OnNodeAddPbEvent(arena) {
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
    kEntityFieldNumber = 1,
    kNodeTypeFieldNumber = 2,
    kScenenodeTypeFieldNumber = 3,
  };
  // uint64 entity = 1;
  void clear_entity() ;
  ::uint64_t entity() const;
  void set_entity(::uint64_t value);

  private:
  ::uint64_t _internal_entity() const;
  void _internal_set_entity(::uint64_t value);

  public:
  // uint32 node_type = 2;
  void clear_node_type() ;
  ::uint32_t node_type() const;
  void set_node_type(::uint32_t value);

  private:
  ::uint32_t _internal_node_type() const;
  void _internal_set_node_type(::uint32_t value);

  public:
  // uint32 scenenode_type = 3;
  void clear_scenenode_type() ;
  ::uint32_t scenenode_type() const;
  void set_scenenode_type(::uint32_t value);

  private:
  ::uint32_t _internal_scenenode_type() const;
  void _internal_set_scenenode_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:OnNodeAddPbEvent)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
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
                          const OnNodeAddPbEvent& from_msg);
    ::uint64_t entity_;
    ::uint32_t node_type_;
    ::uint32_t scenenode_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fnode_5fevent_2eproto;
};
// -------------------------------------------------------------------

class ConnectToNodePbEvent final
    : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:ConnectToNodePbEvent) */ {
 public:
  inline ConnectToNodePbEvent() : ConnectToNodePbEvent(nullptr) {}
  ~ConnectToNodePbEvent() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(ConnectToNodePbEvent* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(ConnectToNodePbEvent));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ConnectToNodePbEvent(
      ::google::protobuf::internal::ConstantInitialized);

  inline ConnectToNodePbEvent(const ConnectToNodePbEvent& from) : ConnectToNodePbEvent(nullptr, from) {}
  inline ConnectToNodePbEvent(ConnectToNodePbEvent&& from) noexcept
      : ConnectToNodePbEvent(nullptr, std::move(from)) {}
  inline ConnectToNodePbEvent& operator=(const ConnectToNodePbEvent& from) {
    CopyFrom(from);
    return *this;
  }
  inline ConnectToNodePbEvent& operator=(ConnectToNodePbEvent&& from) noexcept {
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
  static const ConnectToNodePbEvent& default_instance() {
    return *internal_default_instance();
  }
  static inline const ConnectToNodePbEvent* internal_default_instance() {
    return reinterpret_cast<const ConnectToNodePbEvent*>(
        &_ConnectToNodePbEvent_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(ConnectToNodePbEvent& a, ConnectToNodePbEvent& b) { a.Swap(&b); }
  inline void Swap(ConnectToNodePbEvent* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ConnectToNodePbEvent* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ConnectToNodePbEvent* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<ConnectToNodePbEvent>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ConnectToNodePbEvent& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ConnectToNodePbEvent& from) { ConnectToNodePbEvent::MergeImpl(*this, from); }

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
  void InternalSwap(ConnectToNodePbEvent* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "ConnectToNodePbEvent"; }

 protected:
  explicit ConnectToNodePbEvent(::google::protobuf::Arena* arena);
  ConnectToNodePbEvent(::google::protobuf::Arena* arena, const ConnectToNodePbEvent& from);
  ConnectToNodePbEvent(::google::protobuf::Arena* arena, ConnectToNodePbEvent&& from) noexcept
      : ConnectToNodePbEvent(arena) {
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
    kEntityFieldNumber = 1,
    kNodeTypeFieldNumber = 2,
  };
  // uint32 entity = 1;
  void clear_entity() ;
  ::uint32_t entity() const;
  void set_entity(::uint32_t value);

  private:
  ::uint32_t _internal_entity() const;
  void _internal_set_entity(::uint32_t value);

  public:
  // uint32 node_type = 2;
  void clear_node_type() ;
  ::uint32_t node_type() const;
  void set_node_type(::uint32_t value);

  private:
  ::uint32_t _internal_node_type() const;
  void _internal_set_node_type(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:ConnectToNodePbEvent)
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
                          const ConnectToNodePbEvent& from_msg);
    ::uint32_t entity_;
    ::uint32_t node_type_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_proto_2flogic_2fevent_2fnode_5fevent_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// OnNodeAddPbEvent

// uint64 entity = 1;
inline void OnNodeAddPbEvent::clear_entity() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = ::uint64_t{0u};
}
inline ::uint64_t OnNodeAddPbEvent::entity() const {
  // @@protoc_insertion_point(field_get:OnNodeAddPbEvent.entity)
  return _internal_entity();
}
inline void OnNodeAddPbEvent::set_entity(::uint64_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:OnNodeAddPbEvent.entity)
}
inline ::uint64_t OnNodeAddPbEvent::_internal_entity() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.entity_;
}
inline void OnNodeAddPbEvent::_internal_set_entity(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = value;
}

// uint32 node_type = 2;
inline void OnNodeAddPbEvent::clear_node_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = 0u;
}
inline ::uint32_t OnNodeAddPbEvent::node_type() const {
  // @@protoc_insertion_point(field_get:OnNodeAddPbEvent.node_type)
  return _internal_node_type();
}
inline void OnNodeAddPbEvent::set_node_type(::uint32_t value) {
  _internal_set_node_type(value);
  // @@protoc_insertion_point(field_set:OnNodeAddPbEvent.node_type)
}
inline ::uint32_t OnNodeAddPbEvent::_internal_node_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.node_type_;
}
inline void OnNodeAddPbEvent::_internal_set_node_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = value;
}

// uint32 scenenode_type = 3;
inline void OnNodeAddPbEvent::clear_scenenode_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scenenode_type_ = 0u;
}
inline ::uint32_t OnNodeAddPbEvent::scenenode_type() const {
  // @@protoc_insertion_point(field_get:OnNodeAddPbEvent.scenenode_type)
  return _internal_scenenode_type();
}
inline void OnNodeAddPbEvent::set_scenenode_type(::uint32_t value) {
  _internal_set_scenenode_type(value);
  // @@protoc_insertion_point(field_set:OnNodeAddPbEvent.scenenode_type)
}
inline ::uint32_t OnNodeAddPbEvent::_internal_scenenode_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.scenenode_type_;
}
inline void OnNodeAddPbEvent::_internal_set_scenenode_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scenenode_type_ = value;
}

// -------------------------------------------------------------------

// OnNodeRemovePbEvent

// uint64 entity = 1;
inline void OnNodeRemovePbEvent::clear_entity() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = ::uint64_t{0u};
}
inline ::uint64_t OnNodeRemovePbEvent::entity() const {
  // @@protoc_insertion_point(field_get:OnNodeRemovePbEvent.entity)
  return _internal_entity();
}
inline void OnNodeRemovePbEvent::set_entity(::uint64_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:OnNodeRemovePbEvent.entity)
}
inline ::uint64_t OnNodeRemovePbEvent::_internal_entity() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.entity_;
}
inline void OnNodeRemovePbEvent::_internal_set_entity(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = value;
}

// uint32 node_type = 2;
inline void OnNodeRemovePbEvent::clear_node_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = 0u;
}
inline ::uint32_t OnNodeRemovePbEvent::node_type() const {
  // @@protoc_insertion_point(field_get:OnNodeRemovePbEvent.node_type)
  return _internal_node_type();
}
inline void OnNodeRemovePbEvent::set_node_type(::uint32_t value) {
  _internal_set_node_type(value);
  // @@protoc_insertion_point(field_set:OnNodeRemovePbEvent.node_type)
}
inline ::uint32_t OnNodeRemovePbEvent::_internal_node_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.node_type_;
}
inline void OnNodeRemovePbEvent::_internal_set_node_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = value;
}

// uint32 scene_node_type = 3;
inline void OnNodeRemovePbEvent::clear_scene_node_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scene_node_type_ = 0u;
}
inline ::uint32_t OnNodeRemovePbEvent::scene_node_type() const {
  // @@protoc_insertion_point(field_get:OnNodeRemovePbEvent.scene_node_type)
  return _internal_scene_node_type();
}
inline void OnNodeRemovePbEvent::set_scene_node_type(::uint32_t value) {
  _internal_set_scene_node_type(value);
  // @@protoc_insertion_point(field_set:OnNodeRemovePbEvent.scene_node_type)
}
inline ::uint32_t OnNodeRemovePbEvent::_internal_scene_node_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.scene_node_type_;
}
inline void OnNodeRemovePbEvent::_internal_set_scene_node_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scene_node_type_ = value;
}

// -------------------------------------------------------------------

// ConnectToNodePbEvent

// uint32 entity = 1;
inline void ConnectToNodePbEvent::clear_entity() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = 0u;
}
inline ::uint32_t ConnectToNodePbEvent::entity() const {
  // @@protoc_insertion_point(field_get:ConnectToNodePbEvent.entity)
  return _internal_entity();
}
inline void ConnectToNodePbEvent::set_entity(::uint32_t value) {
  _internal_set_entity(value);
  // @@protoc_insertion_point(field_set:ConnectToNodePbEvent.entity)
}
inline ::uint32_t ConnectToNodePbEvent::_internal_entity() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.entity_;
}
inline void ConnectToNodePbEvent::_internal_set_entity(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.entity_ = value;
}

// uint32 node_type = 2;
inline void ConnectToNodePbEvent::clear_node_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = 0u;
}
inline ::uint32_t ConnectToNodePbEvent::node_type() const {
  // @@protoc_insertion_point(field_get:ConnectToNodePbEvent.node_type)
  return _internal_node_type();
}
inline void ConnectToNodePbEvent::set_node_type(::uint32_t value) {
  _internal_set_node_type(value);
  // @@protoc_insertion_point(field_set:ConnectToNodePbEvent.node_type)
}
inline ::uint32_t ConnectToNodePbEvent::_internal_node_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.node_type_;
}
inline void ConnectToNodePbEvent::_internal_set_node_type(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.node_type_ = value;
}

// -------------------------------------------------------------------

// OnNodeConnectedPbEvent

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // proto_2flogic_2fevent_2fnode_5fevent_2eproto_2epb_2eh
