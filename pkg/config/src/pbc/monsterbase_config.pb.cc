// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: monsterbase_config.proto
// Protobuf C++ Version: 5.29.0

#include "monsterbase_config.pb.h"

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

inline constexpr MonsterBaseTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR MonsterBaseTable::MonsterBaseTable(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct MonsterBaseTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MonsterBaseTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MonsterBaseTableDefaultTypeInternal() {}
  union {
    MonsterBaseTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MonsterBaseTableDefaultTypeInternal _MonsterBaseTable_default_instance_;

inline constexpr MonsterBaseTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR MonsterBaseTabledData::MonsterBaseTabledData(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct MonsterBaseTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MonsterBaseTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MonsterBaseTabledDataDefaultTypeInternal() {}
  union {
    MonsterBaseTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MonsterBaseTabledDataDefaultTypeInternal _MonsterBaseTabledData_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_monsterbase_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_monsterbase_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_monsterbase_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::MonsterBaseTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::MonsterBaseTable, _impl_.id_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::MonsterBaseTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::MonsterBaseTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::MonsterBaseTable)},
        {9, -1, -1, sizeof(::MonsterBaseTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_MonsterBaseTable_default_instance_._instance,
    &::_MonsterBaseTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_monsterbase_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\030monsterbase_config.proto\"\036\n\020MonsterBas"
    "eTable\022\n\n\002id\030\001 \001(\r\"8\n\025MonsterBaseTabledD"
    "ata\022\037\n\004data\030\001 \003(\0132\021.MonsterBaseTableB\tZ\007"
    "pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_monsterbase_5fconfig_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_monsterbase_5fconfig_2eproto = {
    false,
    false,
    135,
    descriptor_table_protodef_monsterbase_5fconfig_2eproto,
    "monsterbase_config.proto",
    &descriptor_table_monsterbase_5fconfig_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_monsterbase_5fconfig_2eproto::offsets,
    file_level_enum_descriptors_monsterbase_5fconfig_2eproto,
    file_level_service_descriptors_monsterbase_5fconfig_2eproto,
};
// ===================================================================

class MonsterBaseTable::_Internal {
 public:
};

MonsterBaseTable::MonsterBaseTable(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:MonsterBaseTable)
}
MonsterBaseTable::MonsterBaseTable(
    ::google::protobuf::Arena* arena, const MonsterBaseTable& from)
    : MonsterBaseTable(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE MonsterBaseTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void MonsterBaseTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.id_ = {};
}
MonsterBaseTable::~MonsterBaseTable() {
  // @@protoc_insertion_point(destructor:MonsterBaseTable)
  SharedDtor(*this);
}
inline void MonsterBaseTable::SharedDtor(MessageLite& self) {
  MonsterBaseTable& this_ = static_cast<MonsterBaseTable&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* MonsterBaseTable::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) MonsterBaseTable(arena);
}
constexpr auto MonsterBaseTable::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(MonsterBaseTable),
                                            alignof(MonsterBaseTable));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull MonsterBaseTable::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_MonsterBaseTable_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &MonsterBaseTable::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<MonsterBaseTable>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &MonsterBaseTable::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<MonsterBaseTable>(), &MonsterBaseTable::ByteSizeLong,
            &MonsterBaseTable::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(MonsterBaseTable, _impl_._cached_size_),
        false,
    },
    &MonsterBaseTable::kDescriptorMethods,
    &descriptor_table_monsterbase_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* MonsterBaseTable::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> MonsterBaseTable::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::MonsterBaseTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(MonsterBaseTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(MonsterBaseTable, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(MonsterBaseTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void MonsterBaseTable::Clear() {
// @@protoc_insertion_point(message_clear_start:MonsterBaseTable)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.id_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* MonsterBaseTable::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const MonsterBaseTable& this_ = static_cast<const MonsterBaseTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* MonsterBaseTable::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const MonsterBaseTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:MonsterBaseTable)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 id = 1;
          if (this_._internal_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_id(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:MonsterBaseTable)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t MonsterBaseTable::ByteSizeLong(const MessageLite& base) {
          const MonsterBaseTable& this_ = static_cast<const MonsterBaseTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t MonsterBaseTable::ByteSizeLong() const {
          const MonsterBaseTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:MonsterBaseTable)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

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

void MonsterBaseTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<MonsterBaseTable*>(&to_msg);
  auto& from = static_cast<const MonsterBaseTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:MonsterBaseTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void MonsterBaseTable::CopyFrom(const MonsterBaseTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:MonsterBaseTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void MonsterBaseTable::InternalSwap(MonsterBaseTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.id_, other->_impl_.id_);
}

::google::protobuf::Metadata MonsterBaseTable::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class MonsterBaseTabledData::_Internal {
 public:
};

MonsterBaseTabledData::MonsterBaseTabledData(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:MonsterBaseTabledData)
}
inline PROTOBUF_NDEBUG_INLINE MonsterBaseTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::MonsterBaseTabledData& from_msg)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

MonsterBaseTabledData::MonsterBaseTabledData(
    ::google::protobuf::Arena* arena,
    const MonsterBaseTabledData& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  MonsterBaseTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:MonsterBaseTabledData)
}
inline PROTOBUF_NDEBUG_INLINE MonsterBaseTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void MonsterBaseTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
MonsterBaseTabledData::~MonsterBaseTabledData() {
  // @@protoc_insertion_point(destructor:MonsterBaseTabledData)
  SharedDtor(*this);
}
inline void MonsterBaseTabledData::SharedDtor(MessageLite& self) {
  MonsterBaseTabledData& this_ = static_cast<MonsterBaseTabledData&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* MonsterBaseTabledData::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) MonsterBaseTabledData(arena);
}
constexpr auto MonsterBaseTabledData::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(MonsterBaseTabledData, _impl_.data_) +
          decltype(MonsterBaseTabledData::_impl_.data_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(MonsterBaseTabledData), alignof(MonsterBaseTabledData), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&MonsterBaseTabledData::PlacementNew_,
                                 sizeof(MonsterBaseTabledData),
                                 alignof(MonsterBaseTabledData));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull MonsterBaseTabledData::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_MonsterBaseTabledData_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &MonsterBaseTabledData::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<MonsterBaseTabledData>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &MonsterBaseTabledData::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<MonsterBaseTabledData>(), &MonsterBaseTabledData::ByteSizeLong,
            &MonsterBaseTabledData::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(MonsterBaseTabledData, _impl_._cached_size_),
        false,
    },
    &MonsterBaseTabledData::kDescriptorMethods,
    &descriptor_table_monsterbase_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* MonsterBaseTabledData::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> MonsterBaseTabledData::_table_ = {
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
    ::_pbi::TcParser::GetTable<::MonsterBaseTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .MonsterBaseTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(MonsterBaseTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .MonsterBaseTable data = 1;
    {PROTOBUF_FIELD_OFFSET(MonsterBaseTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::MonsterBaseTable>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void MonsterBaseTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:MonsterBaseTabledData)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* MonsterBaseTabledData::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const MonsterBaseTabledData& this_ = static_cast<const MonsterBaseTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* MonsterBaseTabledData::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const MonsterBaseTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:MonsterBaseTabledData)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // repeated .MonsterBaseTable data = 1;
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
          // @@protoc_insertion_point(serialize_to_array_end:MonsterBaseTabledData)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t MonsterBaseTabledData::ByteSizeLong(const MessageLite& base) {
          const MonsterBaseTabledData& this_ = static_cast<const MonsterBaseTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t MonsterBaseTabledData::ByteSizeLong() const {
          const MonsterBaseTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:MonsterBaseTabledData)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .MonsterBaseTable data = 1;
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

void MonsterBaseTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<MonsterBaseTabledData*>(&to_msg);
  auto& from = static_cast<const MonsterBaseTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:MonsterBaseTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void MonsterBaseTabledData::CopyFrom(const MonsterBaseTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:MonsterBaseTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void MonsterBaseTabledData::InternalSwap(MonsterBaseTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata MonsterBaseTabledData::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_monsterbase_5fconfig_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
