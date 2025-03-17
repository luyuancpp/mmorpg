// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: reward_config.proto
// Protobuf C++ Version: 6.31.0-dev

#ifndef reward_5fconfig_2eproto_2epb_2eh
#define reward_5fconfig_2eproto_2epb_2eh

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

#define PROTOBUF_INTERNAL_EXPORT_reward_5fconfig_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_reward_5fconfig_2eproto {
  static const ::uint32_t offsets[];
};
extern "C" {
extern const ::google::protobuf::internal::DescriptorTable descriptor_table_reward_5fconfig_2eproto;
}  // extern "C"
class RewardTable;
struct RewardTableDefaultTypeInternal;
extern RewardTableDefaultTypeInternal _RewardTable_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull RewardTable_class_data_;
class RewardTabledData;
struct RewardTabledDataDefaultTypeInternal;
extern RewardTabledDataDefaultTypeInternal _RewardTabledData_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull RewardTabledData_class_data_;
class Rewardreward;
struct RewardrewardDefaultTypeInternal;
extern RewardrewardDefaultTypeInternal _Rewardreward_default_instance_;
extern const ::google::protobuf::internal::ClassDataFull Rewardreward_class_data_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class Rewardreward final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:Rewardreward) */ {
 public:
  inline Rewardreward() : Rewardreward(nullptr) {}
  ~Rewardreward() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(Rewardreward* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(Rewardreward));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR Rewardreward(::google::protobuf::internal::ConstantInitialized);

  inline Rewardreward(const Rewardreward& from) : Rewardreward(nullptr, from) {}
  inline Rewardreward(Rewardreward&& from) noexcept
      : Rewardreward(nullptr, std::move(from)) {}
  inline Rewardreward& operator=(const Rewardreward& from) {
    CopyFrom(from);
    return *this;
  }
  inline Rewardreward& operator=(Rewardreward&& from) noexcept {
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
  static const Rewardreward& default_instance() {
    return *reinterpret_cast<const Rewardreward*>(
        &_Rewardreward_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(Rewardreward& a, Rewardreward& b) { a.Swap(&b); }
  inline void Swap(Rewardreward* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(Rewardreward* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  Rewardreward* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<Rewardreward>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const Rewardreward& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const Rewardreward& from) { Rewardreward::MergeImpl(*this, from); }

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
  void InternalSwap(Rewardreward* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "Rewardreward"; }

 protected:
  explicit Rewardreward(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  Rewardreward(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const Rewardreward& from);
  Rewardreward(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, Rewardreward&& from) noexcept
      : Rewardreward(arena) {
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
    kRewardItemFieldNumber = 1,
    kRewardCountFieldNumber = 2,
  };
  // uint32 reward_item = 1;
  void clear_reward_item() ;
  ::uint32_t reward_item() const;
  void set_reward_item(::uint32_t value);

  private:
  ::uint32_t _internal_reward_item() const;
  void _internal_set_reward_item(::uint32_t value);

  public:
  // uint32 reward_count = 2;
  void clear_reward_count() ;
  ::uint32_t reward_count() const;
  void set_reward_count(::uint32_t value);

  private:
  ::uint32_t _internal_reward_count() const;
  void _internal_set_reward_count(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:Rewardreward)
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
        const Rewardreward& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint32_t reward_item_;
    ::uint32_t reward_count_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_reward_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull Rewardreward_class_data_;
// -------------------------------------------------------------------

class RewardTable final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:RewardTable) */ {
 public:
  inline RewardTable() : RewardTable(nullptr) {}
  ~RewardTable() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(RewardTable* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(RewardTable));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR RewardTable(::google::protobuf::internal::ConstantInitialized);

  inline RewardTable(const RewardTable& from) : RewardTable(nullptr, from) {}
  inline RewardTable(RewardTable&& from) noexcept
      : RewardTable(nullptr, std::move(from)) {}
  inline RewardTable& operator=(const RewardTable& from) {
    CopyFrom(from);
    return *this;
  }
  inline RewardTable& operator=(RewardTable&& from) noexcept {
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
  static const RewardTable& default_instance() {
    return *reinterpret_cast<const RewardTable*>(
        &_RewardTable_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(RewardTable& a, RewardTable& b) { a.Swap(&b); }
  inline void Swap(RewardTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(RewardTable* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RewardTable* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<RewardTable>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const RewardTable& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const RewardTable& from) { RewardTable::MergeImpl(*this, from); }

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
  void InternalSwap(RewardTable* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "RewardTable"; }

 protected:
  explicit RewardTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  RewardTable(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const RewardTable& from);
  RewardTable(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, RewardTable&& from) noexcept
      : RewardTable(arena) {
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
    kRewardFieldNumber = 2,
    kIdFieldNumber = 1,
  };
  // repeated .Rewardreward reward = 2;
  int reward_size() const;
  private:
  int _internal_reward_size() const;

  public:
  void clear_reward() ;
  ::Rewardreward* PROTOBUF_NONNULL mutable_reward(int index);
  ::google::protobuf::RepeatedPtrField<::Rewardreward>* PROTOBUF_NONNULL mutable_reward();

  private:
  const ::google::protobuf::RepeatedPtrField<::Rewardreward>& _internal_reward() const;
  ::google::protobuf::RepeatedPtrField<::Rewardreward>* PROTOBUF_NONNULL _internal_mutable_reward();
  public:
  const ::Rewardreward& reward(int index) const;
  ::Rewardreward* PROTOBUF_NONNULL add_reward();
  const ::google::protobuf::RepeatedPtrField<::Rewardreward>& reward() const;
  // uint32 id = 1;
  void clear_id() ;
  ::uint32_t id() const;
  void set_id(::uint32_t value);

  private:
  ::uint32_t _internal_id() const;
  void _internal_set_id(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:RewardTable)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<1, 2,
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
        const RewardTable& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::Rewardreward > reward_;
    ::uint32_t id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_reward_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull RewardTable_class_data_;
// -------------------------------------------------------------------

class RewardTabledData final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:RewardTabledData) */ {
 public:
  inline RewardTabledData() : RewardTabledData(nullptr) {}
  ~RewardTabledData() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(RewardTabledData* PROTOBUF_NONNULL msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(RewardTabledData));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR RewardTabledData(::google::protobuf::internal::ConstantInitialized);

  inline RewardTabledData(const RewardTabledData& from) : RewardTabledData(nullptr, from) {}
  inline RewardTabledData(RewardTabledData&& from) noexcept
      : RewardTabledData(nullptr, std::move(from)) {}
  inline RewardTabledData& operator=(const RewardTabledData& from) {
    CopyFrom(from);
    return *this;
  }
  inline RewardTabledData& operator=(RewardTabledData&& from) noexcept {
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
  static const RewardTabledData& default_instance() {
    return *reinterpret_cast<const RewardTabledData*>(
        &_RewardTabledData_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(RewardTabledData& a, RewardTabledData& b) { a.Swap(&b); }
  inline void Swap(RewardTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(RewardTabledData* PROTOBUF_NONNULL other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RewardTabledData* PROTOBUF_NONNULL New(::google::protobuf::Arena* PROTOBUF_NULLABLE arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<RewardTabledData>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const RewardTabledData& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const RewardTabledData& from) { RewardTabledData::MergeImpl(*this, from); }

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
  void InternalSwap(RewardTabledData* PROTOBUF_NONNULL other);
 private:
  template <typename T>
  friend ::absl::string_view(::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "RewardTabledData"; }

 protected:
  explicit RewardTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena);
  RewardTabledData(::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const RewardTabledData& from);
  RewardTabledData(
      ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, RewardTabledData&& from) noexcept
      : RewardTabledData(arena) {
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
  // repeated .RewardTable data = 1;
  int data_size() const;
  private:
  int _internal_data_size() const;

  public:
  void clear_data() ;
  ::RewardTable* PROTOBUF_NONNULL mutable_data(int index);
  ::google::protobuf::RepeatedPtrField<::RewardTable>* PROTOBUF_NONNULL mutable_data();

  private:
  const ::google::protobuf::RepeatedPtrField<::RewardTable>& _internal_data() const;
  ::google::protobuf::RepeatedPtrField<::RewardTable>* PROTOBUF_NONNULL _internal_mutable_data();
  public:
  const ::RewardTable& data(int index) const;
  ::RewardTable* PROTOBUF_NONNULL add_data();
  const ::google::protobuf::RepeatedPtrField<::RewardTable>& data() const;
  // @@protoc_insertion_point(class_scope:RewardTabledData)
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
        const RewardTabledData& from_msg);
    ::google::protobuf::RepeatedPtrField< ::RewardTable > data_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_reward_5fconfig_2eproto;
};

extern const ::google::protobuf::internal::ClassDataFull RewardTabledData_class_data_;

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// Rewardreward

// uint32 reward_item = 1;
inline void Rewardreward::clear_reward_item() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.reward_item_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t Rewardreward::reward_item() const {
  // @@protoc_insertion_point(field_get:Rewardreward.reward_item)
  return _internal_reward_item();
}
inline void Rewardreward::set_reward_item(::uint32_t value) {
  _internal_set_reward_item(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:Rewardreward.reward_item)
}
inline ::uint32_t Rewardreward::_internal_reward_item() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.reward_item_;
}
inline void Rewardreward::_internal_set_reward_item(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.reward_item_ = value;
}

// uint32 reward_count = 2;
inline void Rewardreward::clear_reward_count() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.reward_count_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline ::uint32_t Rewardreward::reward_count() const {
  // @@protoc_insertion_point(field_get:Rewardreward.reward_count)
  return _internal_reward_count();
}
inline void Rewardreward::set_reward_count(::uint32_t value) {
  _internal_set_reward_count(value);
  _impl_._has_bits_[0] |= 0x00000002u;
  // @@protoc_insertion_point(field_set:Rewardreward.reward_count)
}
inline ::uint32_t Rewardreward::_internal_reward_count() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.reward_count_;
}
inline void Rewardreward::_internal_set_reward_count(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.reward_count_ = value;
}

// -------------------------------------------------------------------

// RewardTable

// uint32 id = 1;
inline void RewardTable::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0u;
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline ::uint32_t RewardTable::id() const {
  // @@protoc_insertion_point(field_get:RewardTable.id)
  return _internal_id();
}
inline void RewardTable::set_id(::uint32_t value) {
  _internal_set_id(value);
  _impl_._has_bits_[0] |= 0x00000001u;
  // @@protoc_insertion_point(field_set:RewardTable.id)
}
inline ::uint32_t RewardTable::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void RewardTable::_internal_set_id(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// repeated .Rewardreward reward = 2;
inline int RewardTable::_internal_reward_size() const {
  return _internal_reward().size();
}
inline int RewardTable::reward_size() const {
  return _internal_reward_size();
}
inline void RewardTable::clear_reward() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.reward_.Clear();
}
inline ::Rewardreward* PROTOBUF_NONNULL RewardTable::mutable_reward(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:RewardTable.reward)
  return _internal_mutable_reward()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Rewardreward>* PROTOBUF_NONNULL RewardTable::mutable_reward()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:RewardTable.reward)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_reward();
}
inline const ::Rewardreward& RewardTable::reward(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:RewardTable.reward)
  return _internal_reward().Get(index);
}
inline ::Rewardreward* PROTOBUF_NONNULL RewardTable::add_reward()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Rewardreward* _add = _internal_mutable_reward()->Add();
  // @@protoc_insertion_point(field_add:RewardTable.reward)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Rewardreward>& RewardTable::reward() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:RewardTable.reward)
  return _internal_reward();
}
inline const ::google::protobuf::RepeatedPtrField<::Rewardreward>&
RewardTable::_internal_reward() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.reward_;
}
inline ::google::protobuf::RepeatedPtrField<::Rewardreward>* PROTOBUF_NONNULL
RewardTable::_internal_mutable_reward() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.reward_;
}

// -------------------------------------------------------------------

// RewardTabledData

// repeated .RewardTable data = 1;
inline int RewardTabledData::_internal_data_size() const {
  return _internal_data().size();
}
inline int RewardTabledData::data_size() const {
  return _internal_data_size();
}
inline void RewardTabledData::clear_data() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_.Clear();
}
inline ::RewardTable* PROTOBUF_NONNULL RewardTabledData::mutable_data(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:RewardTabledData.data)
  return _internal_mutable_data()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::RewardTable>* PROTOBUF_NONNULL RewardTabledData::mutable_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:RewardTabledData.data)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_data();
}
inline const ::RewardTable& RewardTabledData::data(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:RewardTabledData.data)
  return _internal_data().Get(index);
}
inline ::RewardTable* PROTOBUF_NONNULL RewardTabledData::add_data()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::RewardTable* _add = _internal_mutable_data()->Add();
  // @@protoc_insertion_point(field_add:RewardTabledData.data)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::RewardTable>& RewardTabledData::data() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:RewardTabledData.data)
  return _internal_data();
}
inline const ::google::protobuf::RepeatedPtrField<::RewardTable>&
RewardTabledData::_internal_data() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_;
}
inline ::google::protobuf::RepeatedPtrField<::RewardTable>* PROTOBUF_NONNULL
RewardTabledData::_internal_mutable_data() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.data_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // reward_5fconfig_2eproto_2epb_2eh
