// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: buff_config.proto
// Protobuf C++ Version: 5.26.1

#include "buff_config.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/generated_message_tctable_impl.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
      template <typename>
PROTOBUF_CONSTEXPR BuffTable_TagEntry_DoNotUse::BuffTable_TagEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct BuffTable_TagEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BuffTable_TagEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BuffTable_TagEntry_DoNotUseDefaultTypeInternal() {}
  union {
    BuffTable_TagEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BuffTable_TagEntry_DoNotUseDefaultTypeInternal _BuffTable_TagEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR BuffTable_ImmunetagEntry_DoNotUse::BuffTable_ImmunetagEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct BuffTable_ImmunetagEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BuffTable_ImmunetagEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BuffTable_ImmunetagEntry_DoNotUseDefaultTypeInternal() {}
  union {
    BuffTable_ImmunetagEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BuffTable_ImmunetagEntry_DoNotUseDefaultTypeInternal _BuffTable_ImmunetagEntry_DoNotUse_default_instance_;

inline constexpr BuffTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : tag_{},
        immunetag_{},
        intervaleffect_{},
        _intervaleffect_cached_byte_size_{0},
        id_{0u},
        nocaster_{0u},
        level_{0u},
        maxlayer_{0u},
        duration_{0u},
        forceinterrupt_{0u},
        interval_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR BuffTable::BuffTable(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct BuffTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BuffTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BuffTableDefaultTypeInternal() {}
  union {
    BuffTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BuffTableDefaultTypeInternal _BuffTable_default_instance_;

inline constexpr BuffTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR BuffTabledData::BuffTabledData(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct BuffTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BuffTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BuffTabledDataDefaultTypeInternal() {}
  union {
    BuffTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BuffTabledDataDefaultTypeInternal _BuffTabledData_default_instance_;
static ::_pb::Metadata file_level_metadata_buff_5fconfig_2eproto[4];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_buff_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_buff_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_buff_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        PROTOBUF_FIELD_OFFSET(::BuffTable_TagEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::BuffTable_TagEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BuffTable_TagEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::BuffTable_TagEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::BuffTable_ImmunetagEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::BuffTable_ImmunetagEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BuffTable_ImmunetagEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::BuffTable_ImmunetagEntry_DoNotUse, value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::BuffTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.nocaster_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.tag_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.immunetag_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.level_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.maxlayer_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.duration_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.forceinterrupt_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.interval_),
        PROTOBUF_FIELD_OFFSET(::BuffTable, _impl_.intervaleffect_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::BuffTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BuffTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, 10, -1, sizeof(::BuffTable_TagEntry_DoNotUse)},
        {12, 22, -1, sizeof(::BuffTable_ImmunetagEntry_DoNotUse)},
        {24, -1, -1, sizeof(::BuffTable)},
        {42, -1, -1, sizeof(::BuffTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_BuffTable_TagEntry_DoNotUse_default_instance_._instance,
    &::_BuffTable_ImmunetagEntry_DoNotUse_default_instance_._instance,
    &::_BuffTable_default_instance_._instance,
    &::_BuffTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_buff_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\021buff_config.proto\"\314\002\n\tBuffTable\022\n\n\002id\030"
    "\001 \001(\r\022\020\n\010nocaster\030\002 \001(\r\022 \n\003tag\030\003 \003(\0132\023.B"
    "uffTable.TagEntry\022,\n\timmunetag\030\004 \003(\0132\031.B"
    "uffTable.ImmunetagEntry\022\r\n\005level\030\005 \001(\r\022\020"
    "\n\010maxlayer\030\006 \001(\r\022\020\n\010duration\030\007 \001(\r\022\026\n\016fo"
    "rceinterrupt\030\010 \001(\r\022\020\n\010interval\030\t \001(\r\022\026\n\016"
    "intervaleffect\030\n \003(\r\032*\n\010TagEntry\022\013\n\003key\030"
    "\001 \001(\t\022\r\n\005value\030\002 \001(\010:\0028\001\0320\n\016ImmunetagEnt"
    "ry\022\013\n\003key\030\001 \001(\t\022\r\n\005value\030\002 \001(\010:\0028\001\"*\n\016Bu"
    "ffTabledData\022\030\n\004data\030\001 \003(\0132\n.BuffTableB\t"
    "Z\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_buff_5fconfig_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_buff_5fconfig_2eproto = {
    false,
    false,
    417,
    descriptor_table_protodef_buff_5fconfig_2eproto,
    "buff_config.proto",
    &descriptor_table_buff_5fconfig_2eproto_once,
    nullptr,
    0,
    4,
    schemas,
    file_default_instances,
    TableStruct_buff_5fconfig_2eproto::offsets,
    file_level_metadata_buff_5fconfig_2eproto,
    file_level_enum_descriptors_buff_5fconfig_2eproto,
    file_level_service_descriptors_buff_5fconfig_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_buff_5fconfig_2eproto_getter() {
  return &descriptor_table_buff_5fconfig_2eproto;
}
// ===================================================================

BuffTable_TagEntry_DoNotUse::BuffTable_TagEntry_DoNotUse() {}
BuffTable_TagEntry_DoNotUse::BuffTable_TagEntry_DoNotUse(::google::protobuf::Arena* arena)
    : SuperType(arena) {}
::google::protobuf::Metadata BuffTable_TagEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_buff_5fconfig_2eproto_getter,
                                   &descriptor_table_buff_5fconfig_2eproto_once,
                                   file_level_metadata_buff_5fconfig_2eproto[0]);
}
// ===================================================================

BuffTable_ImmunetagEntry_DoNotUse::BuffTable_ImmunetagEntry_DoNotUse() {}
BuffTable_ImmunetagEntry_DoNotUse::BuffTable_ImmunetagEntry_DoNotUse(::google::protobuf::Arena* arena)
    : SuperType(arena) {}
::google::protobuf::Metadata BuffTable_ImmunetagEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_buff_5fconfig_2eproto_getter,
                                   &descriptor_table_buff_5fconfig_2eproto_once,
                                   file_level_metadata_buff_5fconfig_2eproto[1]);
}
// ===================================================================

class BuffTable::_Internal {
 public:
};

BuffTable::BuffTable(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:BuffTable)
}
inline PROTOBUF_NDEBUG_INLINE BuffTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : tag_{visibility, arena, from.tag_},
        immunetag_{visibility, arena, from.immunetag_},
        intervaleffect_{visibility, arena, from.intervaleffect_},
        _intervaleffect_cached_byte_size_{0},
        _cached_size_{0} {}

BuffTable::BuffTable(
    ::google::protobuf::Arena* arena,
    const BuffTable& from)
    : ::google::protobuf::Message(arena) {
  BuffTable* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, id_),
           offsetof(Impl_, interval_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::interval_));

  // @@protoc_insertion_point(copy_constructor:BuffTable)
}
inline PROTOBUF_NDEBUG_INLINE BuffTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : tag_{visibility, arena},
        immunetag_{visibility, arena},
        intervaleffect_{visibility, arena},
        _intervaleffect_cached_byte_size_{0},
        _cached_size_{0} {}

inline void BuffTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           0,
           offsetof(Impl_, interval_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::interval_));
}
BuffTable::~BuffTable() {
  // @@protoc_insertion_point(destructor:BuffTable)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void BuffTable::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
BuffTable::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(BuffTable, _impl_._cached_size_),
              false,
          },
          &BuffTable::MergeImpl,
          &BuffTable::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void BuffTable::Clear() {
// @@protoc_insertion_point(message_clear_start:BuffTable)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.tag_.Clear();
  _impl_.immunetag_.Clear();
  _impl_.intervaleffect_.Clear();
  ::memset(&_impl_.id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.interval_) -
      reinterpret_cast<char*>(&_impl_.id_)) + sizeof(_impl_.interval_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* BuffTable::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<4, 10, 2, 38, 2> BuffTable::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    10, 120,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294966272,  // skipmap
    offsetof(decltype(_table_), field_entries),
    10,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_BuffTable_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::BuffTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.id_)}},
    // uint32 nocaster = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.nocaster_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.nocaster_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    // uint32 level = 5;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.level_), 63>(),
     {40, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.level_)}},
    // uint32 maxlayer = 6;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.maxlayer_), 63>(),
     {48, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.maxlayer_)}},
    // uint32 duration = 7;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.duration_), 63>(),
     {56, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.duration_)}},
    // uint32 forceinterrupt = 8;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.forceinterrupt_), 63>(),
     {64, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.forceinterrupt_)}},
    // uint32 interval = 9;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTable, _impl_.interval_), 63>(),
     {72, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.interval_)}},
    // repeated uint32 intervaleffect = 10;
    {::_pbi::TcParser::FastV32P1,
     {82, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.intervaleffect_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 nocaster = 2;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.nocaster_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // map<string, bool> tag = 3;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.tag_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, bool> immunetag = 4;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.immunetag_), 0, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // uint32 level = 5;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.level_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 maxlayer = 6;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.maxlayer_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 duration = 7;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.duration_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 forceinterrupt = 8;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.forceinterrupt_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 interval = 9;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.interval_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // repeated uint32 intervaleffect = 10;
    {PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.intervaleffect_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt32)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(BuffTable()._impl_.tag_)>(
        1, 0, 0, 9,
        8)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(BuffTable()._impl_.immunetag_)>(
        1, 0, 0, 9,
        8)},
  }}, {{
    "\11\0\0\3\11\0\0\0\0\0\0\0\0\0\0\0"
    "BuffTable"
    "tag"
    "immunetag"
  }},
};

::uint8_t* BuffTable::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:BuffTable)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 id = 1;
  if (this->_internal_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_id(), target);
  }

  // uint32 nocaster = 2;
  if (this->_internal_nocaster() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_nocaster(), target);
  }

  // map<string, bool> tag = 3;
  if (!_internal_tag().empty()) {
    using MapType = ::google::protobuf::Map<std::string, bool>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, bool,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_BOOL>;
    const auto& field = _internal_tag();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "BuffTable.tag");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "BuffTable.tag");
      }
    }
  }

  // map<string, bool> immunetag = 4;
  if (!_internal_immunetag().empty()) {
    using MapType = ::google::protobuf::Map<std::string, bool>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, bool,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_BOOL>;
    const auto& field = _internal_immunetag();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            4, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "BuffTable.immunetag");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            4, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "BuffTable.immunetag");
      }
    }
  }

  // uint32 level = 5;
  if (this->_internal_level() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        5, this->_internal_level(), target);
  }

  // uint32 maxlayer = 6;
  if (this->_internal_maxlayer() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        6, this->_internal_maxlayer(), target);
  }

  // uint32 duration = 7;
  if (this->_internal_duration() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        7, this->_internal_duration(), target);
  }

  // uint32 forceinterrupt = 8;
  if (this->_internal_forceinterrupt() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        8, this->_internal_forceinterrupt(), target);
  }

  // uint32 interval = 9;
  if (this->_internal_interval() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        9, this->_internal_interval(), target);
  }

  // repeated uint32 intervaleffect = 10;
  {
    int byte_size = _impl_._intervaleffect_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt32Packed(
          10, _internal_intervaleffect(), byte_size, target);
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:BuffTable)
  return target;
}

::size_t BuffTable::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:BuffTable)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // map<string, bool> tag = 3;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_tag_size());
  for (const auto& entry : _internal_tag()) {
    total_size += _pbi::MapEntryFuncs<std::string, bool,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_BOOL>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, bool> immunetag = 4;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_immunetag_size());
  for (const auto& entry : _internal_immunetag()) {
    total_size += _pbi::MapEntryFuncs<std::string, bool,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_BOOL>::ByteSizeLong(entry.first, entry.second);
  }
  // repeated uint32 intervaleffect = 10;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt32Size(
        this->_internal_intervaleffect())
    ;
    _impl_._intervaleffect_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // uint32 id = 1;
  if (this->_internal_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_id());
  }

  // uint32 nocaster = 2;
  if (this->_internal_nocaster() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_nocaster());
  }

  // uint32 level = 5;
  if (this->_internal_level() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_level());
  }

  // uint32 maxlayer = 6;
  if (this->_internal_maxlayer() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_maxlayer());
  }

  // uint32 duration = 7;
  if (this->_internal_duration() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_duration());
  }

  // uint32 forceinterrupt = 8;
  if (this->_internal_forceinterrupt() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_forceinterrupt());
  }

  // uint32 interval = 9;
  if (this->_internal_interval() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_interval());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void BuffTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<BuffTable*>(&to_msg);
  auto& from = static_cast<const BuffTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:BuffTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.tag_.MergeFrom(from._impl_.tag_);
  _this->_impl_.immunetag_.MergeFrom(from._impl_.immunetag_);
  _this->_internal_mutable_intervaleffect()->MergeFrom(from._internal_intervaleffect());
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  if (from._internal_nocaster() != 0) {
    _this->_impl_.nocaster_ = from._impl_.nocaster_;
  }
  if (from._internal_level() != 0) {
    _this->_impl_.level_ = from._impl_.level_;
  }
  if (from._internal_maxlayer() != 0) {
    _this->_impl_.maxlayer_ = from._impl_.maxlayer_;
  }
  if (from._internal_duration() != 0) {
    _this->_impl_.duration_ = from._impl_.duration_;
  }
  if (from._internal_forceinterrupt() != 0) {
    _this->_impl_.forceinterrupt_ = from._impl_.forceinterrupt_;
  }
  if (from._internal_interval() != 0) {
    _this->_impl_.interval_ = from._impl_.interval_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void BuffTable::CopyFrom(const BuffTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:BuffTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool BuffTable::IsInitialized() const {
  return true;
}

void BuffTable::InternalSwap(BuffTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.tag_.InternalSwap(&other->_impl_.tag_);
  _impl_.immunetag_.InternalSwap(&other->_impl_.immunetag_);
  _impl_.intervaleffect_.InternalSwap(&other->_impl_.intervaleffect_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.interval_)
      + sizeof(BuffTable::_impl_.interval_)
      - PROTOBUF_FIELD_OFFSET(BuffTable, _impl_.id_)>(
          reinterpret_cast<char*>(&_impl_.id_),
          reinterpret_cast<char*>(&other->_impl_.id_));
}

::google::protobuf::Metadata BuffTable::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_buff_5fconfig_2eproto_getter,
                                   &descriptor_table_buff_5fconfig_2eproto_once,
                                   file_level_metadata_buff_5fconfig_2eproto[2]);
}
// ===================================================================

class BuffTabledData::_Internal {
 public:
};

BuffTabledData::BuffTabledData(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:BuffTabledData)
}
inline PROTOBUF_NDEBUG_INLINE BuffTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

BuffTabledData::BuffTabledData(
    ::google::protobuf::Arena* arena,
    const BuffTabledData& from)
    : ::google::protobuf::Message(arena) {
  BuffTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:BuffTabledData)
}
inline PROTOBUF_NDEBUG_INLINE BuffTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void BuffTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
BuffTabledData::~BuffTabledData() {
  // @@protoc_insertion_point(destructor:BuffTabledData)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void BuffTabledData::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
BuffTabledData::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(BuffTabledData, _impl_._cached_size_),
              false,
          },
          &BuffTabledData::MergeImpl,
          &BuffTabledData::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void BuffTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:BuffTabledData)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* BuffTabledData::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> BuffTabledData::_table_ = {
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
    &_BuffTabledData_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::BuffTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .BuffTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .BuffTable data = 1;
    {PROTOBUF_FIELD_OFFSET(BuffTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::BuffTable>()},
  }}, {{
  }},
};

::uint8_t* BuffTabledData::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:BuffTabledData)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated .BuffTable data = 1;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_data_size());
       i < n; i++) {
    const auto& repfield = this->_internal_data().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            1, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:BuffTabledData)
  return target;
}

::size_t BuffTabledData::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:BuffTabledData)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated .BuffTable data = 1;
  total_size += 1UL * this->_internal_data_size();
  for (const auto& msg : this->_internal_data()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void BuffTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<BuffTabledData*>(&to_msg);
  auto& from = static_cast<const BuffTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:BuffTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void BuffTabledData::CopyFrom(const BuffTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:BuffTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool BuffTabledData::IsInitialized() const {
  return true;
}

void BuffTabledData::InternalSwap(BuffTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata BuffTabledData::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_buff_5fconfig_2eproto_getter,
                                   &descriptor_table_buff_5fconfig_2eproto_once,
                                   file_level_metadata_buff_5fconfig_2eproto[3]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_buff_5fconfig_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"