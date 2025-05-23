// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: scene_config.proto
// Protobuf C++ Version: 5.29.0

#include "scene_config.pb.h"

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

inline constexpr SceneTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : id_{0u},
        dungeon_id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR SceneTable::SceneTable(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct SceneTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SceneTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SceneTableDefaultTypeInternal() {}
  union {
    SceneTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SceneTableDefaultTypeInternal _SceneTable_default_instance_;

inline constexpr SceneTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR SceneTabledData::SceneTabledData(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct SceneTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SceneTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SceneTabledDataDefaultTypeInternal() {}
  union {
    SceneTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SceneTabledDataDefaultTypeInternal _SceneTabledData_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_scene_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_scene_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_scene_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::SceneTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::SceneTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::SceneTable, _impl_.dungeon_id_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::SceneTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::SceneTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::SceneTable)},
        {10, -1, -1, sizeof(::SceneTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_SceneTable_default_instance_._instance,
    &::_SceneTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_scene_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\022scene_config.proto\",\n\nSceneTable\022\n\n\002id"
    "\030\001 \001(\r\022\022\n\ndungeon_id\030\002 \001(\r\",\n\017SceneTable"
    "dData\022\031\n\004data\030\001 \003(\0132\013.SceneTableB\tZ\007pb/g"
    "ameb\006proto3"
};
static ::absl::once_flag descriptor_table_scene_5fconfig_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_scene_5fconfig_2eproto = {
    false,
    false,
    131,
    descriptor_table_protodef_scene_5fconfig_2eproto,
    "scene_config.proto",
    &descriptor_table_scene_5fconfig_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_scene_5fconfig_2eproto::offsets,
    file_level_enum_descriptors_scene_5fconfig_2eproto,
    file_level_service_descriptors_scene_5fconfig_2eproto,
};
// ===================================================================

class SceneTable::_Internal {
 public:
};

SceneTable::SceneTable(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:SceneTable)
}
SceneTable::SceneTable(
    ::google::protobuf::Arena* arena, const SceneTable& from)
    : SceneTable(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE SceneTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void SceneTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           0,
           offsetof(Impl_, dungeon_id_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::dungeon_id_));
}
SceneTable::~SceneTable() {
  // @@protoc_insertion_point(destructor:SceneTable)
  SharedDtor(*this);
}
inline void SceneTable::SharedDtor(MessageLite& self) {
  SceneTable& this_ = static_cast<SceneTable&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* SceneTable::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) SceneTable(arena);
}
constexpr auto SceneTable::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(SceneTable),
                                            alignof(SceneTable));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull SceneTable::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_SceneTable_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &SceneTable::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<SceneTable>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &SceneTable::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<SceneTable>(), &SceneTable::ByteSizeLong,
            &SceneTable::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(SceneTable, _impl_._cached_size_),
        false,
    },
    &SceneTable::kDescriptorMethods,
    &descriptor_table_scene_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* SceneTable::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> SceneTable::_table_ = {
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
    ::_pbi::TcParser::GetTable<::SceneTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 dungeon_id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(SceneTable, _impl_.dungeon_id_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.dungeon_id_)}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(SceneTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 dungeon_id = 2;
    {PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.dungeon_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void SceneTable::Clear() {
// @@protoc_insertion_point(message_clear_start:SceneTable)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.dungeon_id_) -
      reinterpret_cast<char*>(&_impl_.id_)) + sizeof(_impl_.dungeon_id_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* SceneTable::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const SceneTable& this_ = static_cast<const SceneTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* SceneTable::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const SceneTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:SceneTable)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 id = 1;
          if (this_._internal_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_id(), target);
          }

          // uint32 dungeon_id = 2;
          if (this_._internal_dungeon_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                2, this_._internal_dungeon_id(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:SceneTable)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t SceneTable::ByteSizeLong(const MessageLite& base) {
          const SceneTable& this_ = static_cast<const SceneTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t SceneTable::ByteSizeLong() const {
          const SceneTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:SceneTable)
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
            // uint32 dungeon_id = 2;
            if (this_._internal_dungeon_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_dungeon_id());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void SceneTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<SceneTable*>(&to_msg);
  auto& from = static_cast<const SceneTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:SceneTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  if (from._internal_dungeon_id() != 0) {
    _this->_impl_.dungeon_id_ = from._impl_.dungeon_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void SceneTable::CopyFrom(const SceneTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:SceneTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void SceneTable::InternalSwap(SceneTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.dungeon_id_)
      + sizeof(SceneTable::_impl_.dungeon_id_)
      - PROTOBUF_FIELD_OFFSET(SceneTable, _impl_.id_)>(
          reinterpret_cast<char*>(&_impl_.id_),
          reinterpret_cast<char*>(&other->_impl_.id_));
}

::google::protobuf::Metadata SceneTable::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class SceneTabledData::_Internal {
 public:
};

SceneTabledData::SceneTabledData(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:SceneTabledData)
}
inline PROTOBUF_NDEBUG_INLINE SceneTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::SceneTabledData& from_msg)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

SceneTabledData::SceneTabledData(
    ::google::protobuf::Arena* arena,
    const SceneTabledData& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SceneTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:SceneTabledData)
}
inline PROTOBUF_NDEBUG_INLINE SceneTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void SceneTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
SceneTabledData::~SceneTabledData() {
  // @@protoc_insertion_point(destructor:SceneTabledData)
  SharedDtor(*this);
}
inline void SceneTabledData::SharedDtor(MessageLite& self) {
  SceneTabledData& this_ = static_cast<SceneTabledData&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* SceneTabledData::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) SceneTabledData(arena);
}
constexpr auto SceneTabledData::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(SceneTabledData, _impl_.data_) +
          decltype(SceneTabledData::_impl_.data_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(SceneTabledData), alignof(SceneTabledData), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&SceneTabledData::PlacementNew_,
                                 sizeof(SceneTabledData),
                                 alignof(SceneTabledData));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull SceneTabledData::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_SceneTabledData_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &SceneTabledData::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<SceneTabledData>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &SceneTabledData::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<SceneTabledData>(), &SceneTabledData::ByteSizeLong,
            &SceneTabledData::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(SceneTabledData, _impl_._cached_size_),
        false,
    },
    &SceneTabledData::kDescriptorMethods,
    &descriptor_table_scene_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* SceneTabledData::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> SceneTabledData::_table_ = {
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
    ::_pbi::TcParser::GetTable<::SceneTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .SceneTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(SceneTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .SceneTable data = 1;
    {PROTOBUF_FIELD_OFFSET(SceneTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::SceneTable>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void SceneTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:SceneTabledData)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* SceneTabledData::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const SceneTabledData& this_ = static_cast<const SceneTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* SceneTabledData::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const SceneTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:SceneTabledData)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // repeated .SceneTable data = 1;
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
          // @@protoc_insertion_point(serialize_to_array_end:SceneTabledData)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t SceneTabledData::ByteSizeLong(const MessageLite& base) {
          const SceneTabledData& this_ = static_cast<const SceneTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t SceneTabledData::ByteSizeLong() const {
          const SceneTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:SceneTabledData)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .SceneTable data = 1;
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

void SceneTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<SceneTabledData*>(&to_msg);
  auto& from = static_cast<const SceneTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:SceneTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void SceneTabledData::CopyFrom(const SceneTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:SceneTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void SceneTabledData::InternalSwap(SceneTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata SceneTabledData::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_scene_5fconfig_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
