// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: cooldown_config.proto
// Protobuf C++ Version: 5.29.0

#include "cooldown_config.pb.h"

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

inline constexpr CooldownTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : id_{0u},
        duration_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR CooldownTable::CooldownTable(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct CooldownTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CooldownTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CooldownTableDefaultTypeInternal() {}
  union {
    CooldownTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CooldownTableDefaultTypeInternal _CooldownTable_default_instance_;

inline constexpr CooldownTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR CooldownTabledData::CooldownTabledData(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct CooldownTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CooldownTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CooldownTabledDataDefaultTypeInternal() {}
  union {
    CooldownTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CooldownTabledDataDefaultTypeInternal _CooldownTabledData_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_cooldown_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_cooldown_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_cooldown_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CooldownTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CooldownTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::CooldownTable, _impl_.duration_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CooldownTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CooldownTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::CooldownTable)},
        {10, -1, -1, sizeof(::CooldownTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_CooldownTable_default_instance_._instance,
    &::_CooldownTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_cooldown_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\025cooldown_config.proto\"-\n\rCooldownTable"
    "\022\n\n\002id\030\001 \001(\r\022\020\n\010duration\030\002 \001(\r\"2\n\022Cooldo"
    "wnTabledData\022\034\n\004data\030\001 \003(\0132\016.CooldownTab"
    "leB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_cooldown_5fconfig_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_cooldown_5fconfig_2eproto = {
    false,
    false,
    141,
    descriptor_table_protodef_cooldown_5fconfig_2eproto,
    "cooldown_config.proto",
    &descriptor_table_cooldown_5fconfig_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_cooldown_5fconfig_2eproto::offsets,
    file_level_enum_descriptors_cooldown_5fconfig_2eproto,
    file_level_service_descriptors_cooldown_5fconfig_2eproto,
};
// ===================================================================

class CooldownTable::_Internal {
 public:
};

CooldownTable::CooldownTable(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:CooldownTable)
}
CooldownTable::CooldownTable(
    ::google::protobuf::Arena* arena, const CooldownTable& from)
    : CooldownTable(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE CooldownTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void CooldownTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           0,
           offsetof(Impl_, duration_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::duration_));
}
CooldownTable::~CooldownTable() {
  // @@protoc_insertion_point(destructor:CooldownTable)
  SharedDtor(*this);
}
inline void CooldownTable::SharedDtor(MessageLite& self) {
  CooldownTable& this_ = static_cast<CooldownTable&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* CooldownTable::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) CooldownTable(arena);
}
constexpr auto CooldownTable::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(CooldownTable),
                                            alignof(CooldownTable));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull CooldownTable::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_CooldownTable_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &CooldownTable::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<CooldownTable>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &CooldownTable::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<CooldownTable>(), &CooldownTable::ByteSizeLong,
            &CooldownTable::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_._cached_size_),
        false,
    },
    &CooldownTable::kDescriptorMethods,
    &descriptor_table_cooldown_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* CooldownTable::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> CooldownTable::_table_ = {
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
    ::_pbi::TcParser::GetTable<::CooldownTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 duration = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(CooldownTable, _impl_.duration_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.duration_)}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(CooldownTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 duration = 2;
    {PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.duration_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void CooldownTable::Clear() {
// @@protoc_insertion_point(message_clear_start:CooldownTable)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.duration_) -
      reinterpret_cast<char*>(&_impl_.id_)) + sizeof(_impl_.duration_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* CooldownTable::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const CooldownTable& this_ = static_cast<const CooldownTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* CooldownTable::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const CooldownTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:CooldownTable)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 id = 1;
          if (this_._internal_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_id(), target);
          }

          // uint32 duration = 2;
          if (this_._internal_duration() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                2, this_._internal_duration(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:CooldownTable)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t CooldownTable::ByteSizeLong(const MessageLite& base) {
          const CooldownTable& this_ = static_cast<const CooldownTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t CooldownTable::ByteSizeLong() const {
          const CooldownTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:CooldownTable)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // uint32 id = 1;
            if (this_._internal_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_id());
            }
            // uint32 duration = 2;
            if (this_._internal_duration() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_duration());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void CooldownTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<CooldownTable*>(&to_msg);
  auto& from = static_cast<const CooldownTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:CooldownTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  if (from._internal_duration() != 0) {
    _this->_impl_.duration_ = from._impl_.duration_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void CooldownTable::CopyFrom(const CooldownTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:CooldownTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void CooldownTable::InternalSwap(CooldownTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.duration_)
      + sizeof(CooldownTable::_impl_.duration_)
      - PROTOBUF_FIELD_OFFSET(CooldownTable, _impl_.id_)>(
          reinterpret_cast<char*>(&_impl_.id_),
          reinterpret_cast<char*>(&other->_impl_.id_));
}

::google::protobuf::Metadata CooldownTable::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class CooldownTabledData::_Internal {
 public:
};

CooldownTabledData::CooldownTabledData(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:CooldownTabledData)
}
inline PROTOBUF_NDEBUG_INLINE CooldownTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::CooldownTabledData& from_msg)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

CooldownTabledData::CooldownTabledData(
    ::google::protobuf::Arena* arena,
    const CooldownTabledData& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  CooldownTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:CooldownTabledData)
}
inline PROTOBUF_NDEBUG_INLINE CooldownTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void CooldownTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
CooldownTabledData::~CooldownTabledData() {
  // @@protoc_insertion_point(destructor:CooldownTabledData)
  SharedDtor(*this);
}
inline void CooldownTabledData::SharedDtor(MessageLite& self) {
  CooldownTabledData& this_ = static_cast<CooldownTabledData&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* CooldownTabledData::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) CooldownTabledData(arena);
}
constexpr auto CooldownTabledData::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(CooldownTabledData, _impl_.data_) +
          decltype(CooldownTabledData::_impl_.data_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(CooldownTabledData), alignof(CooldownTabledData), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&CooldownTabledData::PlacementNew_,
                                 sizeof(CooldownTabledData),
                                 alignof(CooldownTabledData));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull CooldownTabledData::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_CooldownTabledData_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &CooldownTabledData::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<CooldownTabledData>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &CooldownTabledData::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<CooldownTabledData>(), &CooldownTabledData::ByteSizeLong,
            &CooldownTabledData::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(CooldownTabledData, _impl_._cached_size_),
        false,
    },
    &CooldownTabledData::kDescriptorMethods,
    &descriptor_table_cooldown_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* CooldownTabledData::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> CooldownTabledData::_table_ = {
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
    ::_pbi::TcParser::GetTable<::CooldownTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .CooldownTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(CooldownTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .CooldownTable data = 1;
    {PROTOBUF_FIELD_OFFSET(CooldownTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::CooldownTable>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void CooldownTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:CooldownTabledData)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* CooldownTabledData::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const CooldownTabledData& this_ = static_cast<const CooldownTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* CooldownTabledData::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const CooldownTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:CooldownTabledData)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // repeated .CooldownTable data = 1;
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
          // @@protoc_insertion_point(serialize_to_array_end:CooldownTabledData)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t CooldownTabledData::ByteSizeLong(const MessageLite& base) {
          const CooldownTabledData& this_ = static_cast<const CooldownTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t CooldownTabledData::ByteSizeLong() const {
          const CooldownTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:CooldownTabledData)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .CooldownTable data = 1;
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

void CooldownTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<CooldownTabledData*>(&to_msg);
  auto& from = static_cast<const CooldownTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:CooldownTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void CooldownTabledData::CopyFrom(const CooldownTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:CooldownTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void CooldownTabledData::InternalSwap(CooldownTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata CooldownTabledData::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_cooldown_5fconfig_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
