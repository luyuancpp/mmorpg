// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: reward_config.proto
// Protobuf C++ Version: 5.29.0

#include "reward_config.pb.h"

#include <algorithm>
#include <type_traits>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/generated_message_tctable_impl.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;

inline constexpr Rewardreward::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : reward_item_{0u},
        reward_count_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR Rewardreward::Rewardreward(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct RewardrewardDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RewardrewardDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RewardrewardDefaultTypeInternal() {}
  union {
    Rewardreward _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RewardrewardDefaultTypeInternal _Rewardreward_default_instance_;

inline constexpr RewardTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : reward_{},
        id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR RewardTable::RewardTable(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct RewardTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RewardTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RewardTableDefaultTypeInternal() {}
  union {
    RewardTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RewardTableDefaultTypeInternal _RewardTable_default_instance_;

inline constexpr RewardTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR RewardTabledData::RewardTabledData(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct RewardTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RewardTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RewardTabledDataDefaultTypeInternal() {}
  union {
    RewardTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RewardTabledDataDefaultTypeInternal _RewardTabledData_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_reward_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_reward_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_reward_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::Rewardreward, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::Rewardreward, _impl_.reward_item_),
        PROTOBUF_FIELD_OFFSET(::Rewardreward, _impl_.reward_count_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::RewardTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::RewardTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::RewardTable, _impl_.reward_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::RewardTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::RewardTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::Rewardreward)},
        {10, -1, -1, sizeof(::RewardTable)},
        {20, -1, -1, sizeof(::RewardTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_Rewardreward_default_instance_._instance,
    &::_RewardTable_default_instance_._instance,
    &::_RewardTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_reward_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\023reward_config.proto\"9\n\014Rewardreward\022\023\n"
    "\013reward_item\030\001 \001(\r\022\024\n\014reward_count\030\002 \001(\r"
    "\"8\n\013RewardTable\022\n\n\002id\030\001 \001(\r\022\035\n\006reward\030\002 "
    "\003(\0132\r.Rewardreward\".\n\020RewardTabledData\022\032"
    "\n\004data\030\001 \003(\0132\014.RewardTableB\tZ\007pb/gameb\006p"
    "roto3"
};
static ::absl::once_flag descriptor_table_reward_5fconfig_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_reward_5fconfig_2eproto = {
    false,
    false,
    205,
    descriptor_table_protodef_reward_5fconfig_2eproto,
    "reward_config.proto",
    &descriptor_table_reward_5fconfig_2eproto_once,
    nullptr,
    0,
    3,
    schemas,
    file_default_instances,
    TableStruct_reward_5fconfig_2eproto::offsets,
    file_level_enum_descriptors_reward_5fconfig_2eproto,
    file_level_service_descriptors_reward_5fconfig_2eproto,
};
// ===================================================================

class Rewardreward::_Internal {
 public:
};

Rewardreward::Rewardreward(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Rewardreward)
}
Rewardreward::Rewardreward(
    ::google::protobuf::Arena* arena, const Rewardreward& from)
    : Rewardreward(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE Rewardreward::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void Rewardreward::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, reward_item_),
           0,
           offsetof(Impl_, reward_count_) -
               offsetof(Impl_, reward_item_) +
               sizeof(Impl_::reward_count_));
}
Rewardreward::~Rewardreward() {
  // @@protoc_insertion_point(destructor:Rewardreward)
  SharedDtor(*this);
}
inline void Rewardreward::SharedDtor(MessageLite& self) {
  Rewardreward& this_ = static_cast<Rewardreward&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* Rewardreward::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) Rewardreward(arena);
}
constexpr auto Rewardreward::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(Rewardreward),
                                            alignof(Rewardreward));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull Rewardreward::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_Rewardreward_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &Rewardreward::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<Rewardreward>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &Rewardreward::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<Rewardreward>(), &Rewardreward::ByteSizeLong,
            &Rewardreward::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_._cached_size_),
        false,
    },
    &Rewardreward::kDescriptorMethods,
    &descriptor_table_reward_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* Rewardreward::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> Rewardreward::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Rewardreward>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 reward_count = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Rewardreward, _impl_.reward_count_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_count_)}},
    // uint32 reward_item = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Rewardreward, _impl_.reward_item_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_item_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 reward_item = 1;
    {PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_item_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 reward_count = 2;
    {PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_count_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void Rewardreward::Clear() {
// @@protoc_insertion_point(message_clear_start:Rewardreward)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.reward_item_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.reward_count_) -
      reinterpret_cast<char*>(&_impl_.reward_item_)) + sizeof(_impl_.reward_count_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* Rewardreward::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const Rewardreward& this_ = static_cast<const Rewardreward&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* Rewardreward::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const Rewardreward& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:Rewardreward)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 reward_item = 1;
          if (this_._internal_reward_item() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_reward_item(), target);
          }

          // uint32 reward_count = 2;
          if (this_._internal_reward_count() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                2, this_._internal_reward_count(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:Rewardreward)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t Rewardreward::ByteSizeLong(const MessageLite& base) {
          const Rewardreward& this_ = static_cast<const Rewardreward&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t Rewardreward::ByteSizeLong() const {
          const Rewardreward& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:Rewardreward)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // uint32 reward_item = 1;
            if (this_._internal_reward_item() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_reward_item());
            }
            // uint32 reward_count = 2;
            if (this_._internal_reward_count() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_reward_count());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void Rewardreward::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<Rewardreward*>(&to_msg);
  auto& from = static_cast<const Rewardreward&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:Rewardreward)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_reward_item() != 0) {
    _this->_impl_.reward_item_ = from._impl_.reward_item_;
  }
  if (from._internal_reward_count() != 0) {
    _this->_impl_.reward_count_ = from._impl_.reward_count_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void Rewardreward::CopyFrom(const Rewardreward& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Rewardreward)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void Rewardreward::InternalSwap(Rewardreward* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_count_)
      + sizeof(Rewardreward::_impl_.reward_count_)
      - PROTOBUF_FIELD_OFFSET(Rewardreward, _impl_.reward_item_)>(
          reinterpret_cast<char*>(&_impl_.reward_item_),
          reinterpret_cast<char*>(&other->_impl_.reward_item_));
}

::google::protobuf::Metadata Rewardreward::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class RewardTable::_Internal {
 public:
};

RewardTable::RewardTable(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:RewardTable)
}
inline PROTOBUF_NDEBUG_INLINE RewardTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::RewardTable& from_msg)
      : reward_{visibility, arena, from.reward_},
        _cached_size_{0} {}

RewardTable::RewardTable(
    ::google::protobuf::Arena* arena,
    const RewardTable& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  RewardTable* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.id_ = from._impl_.id_;

  // @@protoc_insertion_point(copy_constructor:RewardTable)
}
inline PROTOBUF_NDEBUG_INLINE RewardTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : reward_{visibility, arena},
        _cached_size_{0} {}

inline void RewardTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.id_ = {};
}
RewardTable::~RewardTable() {
  // @@protoc_insertion_point(destructor:RewardTable)
  SharedDtor(*this);
}
inline void RewardTable::SharedDtor(MessageLite& self) {
  RewardTable& this_ = static_cast<RewardTable&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* RewardTable::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) RewardTable(arena);
}
constexpr auto RewardTable::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(RewardTable, _impl_.reward_) +
          decltype(RewardTable::_impl_.reward_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(RewardTable), alignof(RewardTable), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&RewardTable::PlacementNew_,
                                 sizeof(RewardTable),
                                 alignof(RewardTable));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull RewardTable::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_RewardTable_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &RewardTable::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<RewardTable>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &RewardTable::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<RewardTable>(), &RewardTable::ByteSizeLong,
            &RewardTable::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(RewardTable, _impl_._cached_size_),
        false,
    },
    &RewardTable::kDescriptorMethods,
    &descriptor_table_reward_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* RewardTable::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 1, 0, 2> RewardTable::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::RewardTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .Rewardreward reward = 2;
    {::_pbi::TcParser::FastMtR1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(RewardTable, _impl_.reward_)}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(RewardTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(RewardTable, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(RewardTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // repeated .Rewardreward reward = 2;
    {PROTOBUF_FIELD_OFFSET(RewardTable, _impl_.reward_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Rewardreward>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void RewardTable::Clear() {
// @@protoc_insertion_point(message_clear_start:RewardTable)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.reward_.Clear();
  _impl_.id_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* RewardTable::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const RewardTable& this_ = static_cast<const RewardTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* RewardTable::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const RewardTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:RewardTable)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 id = 1;
          if (this_._internal_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_id(), target);
          }

          // repeated .Rewardreward reward = 2;
          for (unsigned i = 0, n = static_cast<unsigned>(
                                   this_._internal_reward_size());
               i < n; i++) {
            const auto& repfield = this_._internal_reward().Get(i);
            target =
                ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
                    2, repfield, repfield.GetCachedSize(),
                    target, stream);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:RewardTable)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t RewardTable::ByteSizeLong(const MessageLite& base) {
          const RewardTable& this_ = static_cast<const RewardTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t RewardTable::ByteSizeLong() const {
          const RewardTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:RewardTable)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .Rewardreward reward = 2;
            {
              total_size += 1UL * this_._internal_reward_size();
              for (const auto& msg : this_._internal_reward()) {
                total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
              }
            }
          }
           {
            // uint32 id = 1;
            if (this_._internal_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_id());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void RewardTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<RewardTable*>(&to_msg);
  auto& from = static_cast<const RewardTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:RewardTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_reward()->MergeFrom(
      from._internal_reward());
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void RewardTable::CopyFrom(const RewardTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:RewardTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void RewardTable::InternalSwap(RewardTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.reward_.InternalSwap(&other->_impl_.reward_);
        swap(_impl_.id_, other->_impl_.id_);
}

::google::protobuf::Metadata RewardTable::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class RewardTabledData::_Internal {
 public:
};

RewardTabledData::RewardTabledData(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:RewardTabledData)
}
inline PROTOBUF_NDEBUG_INLINE RewardTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::RewardTabledData& from_msg)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

RewardTabledData::RewardTabledData(
    ::google::protobuf::Arena* arena,
    const RewardTabledData& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  RewardTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:RewardTabledData)
}
inline PROTOBUF_NDEBUG_INLINE RewardTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void RewardTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
RewardTabledData::~RewardTabledData() {
  // @@protoc_insertion_point(destructor:RewardTabledData)
  SharedDtor(*this);
}
inline void RewardTabledData::SharedDtor(MessageLite& self) {
  RewardTabledData& this_ = static_cast<RewardTabledData&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* RewardTabledData::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) RewardTabledData(arena);
}
constexpr auto RewardTabledData::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(RewardTabledData, _impl_.data_) +
          decltype(RewardTabledData::_impl_.data_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(RewardTabledData), alignof(RewardTabledData), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&RewardTabledData::PlacementNew_,
                                 sizeof(RewardTabledData),
                                 alignof(RewardTabledData));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull RewardTabledData::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_RewardTabledData_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &RewardTabledData::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<RewardTabledData>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &RewardTabledData::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<RewardTabledData>(), &RewardTabledData::ByteSizeLong,
            &RewardTabledData::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(RewardTabledData, _impl_._cached_size_),
        false,
    },
    &RewardTabledData::kDescriptorMethods,
    &descriptor_table_reward_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* RewardTabledData::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> RewardTabledData::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::RewardTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .RewardTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(RewardTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .RewardTable data = 1;
    {PROTOBUF_FIELD_OFFSET(RewardTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::RewardTable>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void RewardTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:RewardTabledData)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* RewardTabledData::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const RewardTabledData& this_ = static_cast<const RewardTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* RewardTabledData::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const RewardTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:RewardTabledData)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // repeated .RewardTable data = 1;
          for (unsigned i = 0, n = static_cast<unsigned>(
                                   this_._internal_data_size());
               i < n; i++) {
            const auto& repfield = this_._internal_data().Get(i);
            target =
                ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
                    1, repfield, repfield.GetCachedSize(),
                    target, stream);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:RewardTabledData)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t RewardTabledData::ByteSizeLong(const MessageLite& base) {
          const RewardTabledData& this_ = static_cast<const RewardTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t RewardTabledData::ByteSizeLong() const {
          const RewardTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:RewardTabledData)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .RewardTable data = 1;
            {
              total_size += 1UL * this_._internal_data_size();
              for (const auto& msg : this_._internal_data()) {
                total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
              }
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void RewardTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<RewardTabledData*>(&to_msg);
  auto& from = static_cast<const RewardTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:RewardTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void RewardTabledData::CopyFrom(const RewardTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:RewardTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void RewardTabledData::InternalSwap(RewardTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata RewardTabledData::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ PROTOBUF_UNUSED =
        (::_pbi::AddDescriptors(&descriptor_table_reward_5fconfig_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
