// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: globalvariable_config.proto
// Protobuf C++ Version: 5.29.0

#include "globalvariable_config.pb.h"

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

inline constexpr GlobalVariableTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : tostring_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        id_{0u},
        touint32_{0u},
        toint32_{0},
        tofloat_{0},
        todouble_{0},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR GlobalVariableTable::GlobalVariableTable(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct GlobalVariableTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GlobalVariableTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GlobalVariableTableDefaultTypeInternal() {}
  union {
    GlobalVariableTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GlobalVariableTableDefaultTypeInternal _GlobalVariableTable_default_instance_;

inline constexpr GlobalVariableTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR GlobalVariableTabledData::GlobalVariableTabledData(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct GlobalVariableTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GlobalVariableTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GlobalVariableTabledDataDefaultTypeInternal() {}
  union {
    GlobalVariableTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GlobalVariableTabledDataDefaultTypeInternal _GlobalVariableTabledData_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_globalvariable_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_globalvariable_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_globalvariable_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.touint32_),
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.toint32_),
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.tostring_),
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.tofloat_),
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTable, _impl_.todouble_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::GlobalVariableTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::GlobalVariableTable)},
        {14, -1, -1, sizeof(::GlobalVariableTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_GlobalVariableTable_default_instance_._instance,
    &::_GlobalVariableTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_globalvariable_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\033globalvariable_config.proto\"y\n\023GlobalV"
    "ariableTable\022\n\n\002id\030\001 \001(\r\022\020\n\010touint32\030\002 \001"
    "(\r\022\017\n\007toint32\030\003 \001(\005\022\020\n\010tostring\030\004 \001(\t\022\017\n"
    "\007tofloat\030\005 \001(\002\022\020\n\010todouble\030\006 \001(\001\">\n\030Glob"
    "alVariableTabledData\022\"\n\004data\030\001 \003(\0132\024.Glo"
    "balVariableTableB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_globalvariable_5fconfig_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_globalvariable_5fconfig_2eproto = {
    false,
    false,
    235,
    descriptor_table_protodef_globalvariable_5fconfig_2eproto,
    "globalvariable_config.proto",
    &descriptor_table_globalvariable_5fconfig_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_globalvariable_5fconfig_2eproto::offsets,
    file_level_enum_descriptors_globalvariable_5fconfig_2eproto,
    file_level_service_descriptors_globalvariable_5fconfig_2eproto,
};
// ===================================================================

class GlobalVariableTable::_Internal {
 public:
};

GlobalVariableTable::GlobalVariableTable(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:GlobalVariableTable)
}
inline PROTOBUF_NDEBUG_INLINE GlobalVariableTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::GlobalVariableTable& from_msg)
      : tostring_(arena, from.tostring_),
        _cached_size_{0} {}

GlobalVariableTable::GlobalVariableTable(
    ::google::protobuf::Arena* arena,
    const GlobalVariableTable& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  GlobalVariableTable* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, id_),
           offsetof(Impl_, todouble_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::todouble_));

  // @@protoc_insertion_point(copy_constructor:GlobalVariableTable)
}
inline PROTOBUF_NDEBUG_INLINE GlobalVariableTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : tostring_(arena),
        _cached_size_{0} {}

inline void GlobalVariableTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, id_),
           0,
           offsetof(Impl_, todouble_) -
               offsetof(Impl_, id_) +
               sizeof(Impl_::todouble_));
}
GlobalVariableTable::~GlobalVariableTable() {
  // @@protoc_insertion_point(destructor:GlobalVariableTable)
  SharedDtor(*this);
}
inline void GlobalVariableTable::SharedDtor(MessageLite& self) {
  GlobalVariableTable& this_ = static_cast<GlobalVariableTable&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.tostring_.Destroy();
  this_._impl_.~Impl_();
}

inline void* GlobalVariableTable::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) GlobalVariableTable(arena);
}
constexpr auto GlobalVariableTable::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::CopyInit(sizeof(GlobalVariableTable),
                                            alignof(GlobalVariableTable));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull GlobalVariableTable::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_GlobalVariableTable_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &GlobalVariableTable::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<GlobalVariableTable>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &GlobalVariableTable::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<GlobalVariableTable>(), &GlobalVariableTable::ByteSizeLong,
            &GlobalVariableTable::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_._cached_size_),
        false,
    },
    &GlobalVariableTable::kDescriptorMethods,
    &descriptor_table_globalvariable_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* GlobalVariableTable::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<3, 6, 0, 36, 2> GlobalVariableTable::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    6, 56,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967232,  // skipmap
    offsetof(decltype(_table_), field_entries),
    6,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::GlobalVariableTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(GlobalVariableTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.id_)}},
    // uint32 touint32 = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(GlobalVariableTable, _impl_.touint32_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.touint32_)}},
    // int32 toint32 = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(GlobalVariableTable, _impl_.toint32_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.toint32_)}},
    // string tostring = 4;
    {::_pbi::TcParser::FastUS1,
     {34, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.tostring_)}},
    // float tofloat = 5;
    {::_pbi::TcParser::FastF32S1,
     {45, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.tofloat_)}},
    // double todouble = 6;
    {::_pbi::TcParser::FastF64S1,
     {49, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.todouble_)}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 touint32 = 2;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.touint32_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // int32 toint32 = 3;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.toint32_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // string tostring = 4;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.tostring_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // float tofloat = 5;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.tofloat_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kFloat)},
    // double todouble = 6;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.todouble_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kDouble)},
  }},
  // no aux_entries
  {{
    "\23\0\0\0\10\0\0\0"
    "GlobalVariableTable"
    "tostring"
  }},
};

PROTOBUF_NOINLINE void GlobalVariableTable::Clear() {
// @@protoc_insertion_point(message_clear_start:GlobalVariableTable)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.tostring_.ClearToEmpty();
  ::memset(&_impl_.id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.todouble_) -
      reinterpret_cast<char*>(&_impl_.id_)) + sizeof(_impl_.todouble_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* GlobalVariableTable::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const GlobalVariableTable& this_ = static_cast<const GlobalVariableTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* GlobalVariableTable::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const GlobalVariableTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:GlobalVariableTable)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 id = 1;
          if (this_._internal_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_id(), target);
          }

          // uint32 touint32 = 2;
          if (this_._internal_touint32() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                2, this_._internal_touint32(), target);
          }

          // int32 toint32 = 3;
          if (this_._internal_toint32() != 0) {
            target = ::google::protobuf::internal::WireFormatLite::
                WriteInt32ToArrayWithField<3>(
                    stream, this_._internal_toint32(), target);
          }

          // string tostring = 4;
          if (!this_._internal_tostring().empty()) {
            const std::string& _s = this_._internal_tostring();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "GlobalVariableTable.tostring");
            target = stream->WriteStringMaybeAliased(4, _s, target);
          }

          // float tofloat = 5;
          if (::absl::bit_cast<::uint32_t>(this_._internal_tofloat()) != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteFloatToArray(
                5, this_._internal_tofloat(), target);
          }

          // double todouble = 6;
          if (::absl::bit_cast<::uint64_t>(this_._internal_todouble()) != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteDoubleToArray(
                6, this_._internal_todouble(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:GlobalVariableTable)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t GlobalVariableTable::ByteSizeLong(const MessageLite& base) {
          const GlobalVariableTable& this_ = static_cast<const GlobalVariableTable&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t GlobalVariableTable::ByteSizeLong() const {
          const GlobalVariableTable& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:GlobalVariableTable)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // string tostring = 4;
            if (!this_._internal_tostring().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_tostring());
            }
            // uint32 id = 1;
            if (this_._internal_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_id());
            }
            // uint32 touint32 = 2;
            if (this_._internal_touint32() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_touint32());
            }
            // int32 toint32 = 3;
            if (this_._internal_toint32() != 0) {
              total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
                  this_._internal_toint32());
            }
            // float tofloat = 5;
            if (::absl::bit_cast<::uint32_t>(this_._internal_tofloat()) != 0) {
              total_size += 5;
            }
            // double todouble = 6;
            if (::absl::bit_cast<::uint64_t>(this_._internal_todouble()) != 0) {
              total_size += 9;
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void GlobalVariableTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<GlobalVariableTable*>(&to_msg);
  auto& from = static_cast<const GlobalVariableTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:GlobalVariableTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_tostring().empty()) {
    _this->_internal_set_tostring(from._internal_tostring());
  }
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  if (from._internal_touint32() != 0) {
    _this->_impl_.touint32_ = from._impl_.touint32_;
  }
  if (from._internal_toint32() != 0) {
    _this->_impl_.toint32_ = from._impl_.toint32_;
  }
  if (::absl::bit_cast<::uint32_t>(from._internal_tofloat()) != 0) {
    _this->_impl_.tofloat_ = from._impl_.tofloat_;
  }
  if (::absl::bit_cast<::uint64_t>(from._internal_todouble()) != 0) {
    _this->_impl_.todouble_ = from._impl_.todouble_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void GlobalVariableTable::CopyFrom(const GlobalVariableTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:GlobalVariableTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void GlobalVariableTable::InternalSwap(GlobalVariableTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.tostring_, &other->_impl_.tostring_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.todouble_)
      + sizeof(GlobalVariableTable::_impl_.todouble_)
      - PROTOBUF_FIELD_OFFSET(GlobalVariableTable, _impl_.id_)>(
          reinterpret_cast<char*>(&_impl_.id_),
          reinterpret_cast<char*>(&other->_impl_.id_));
}

::google::protobuf::Metadata GlobalVariableTable::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class GlobalVariableTabledData::_Internal {
 public:
};

GlobalVariableTabledData::GlobalVariableTabledData(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:GlobalVariableTabledData)
}
inline PROTOBUF_NDEBUG_INLINE GlobalVariableTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::GlobalVariableTabledData& from_msg)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

GlobalVariableTabledData::GlobalVariableTabledData(
    ::google::protobuf::Arena* arena,
    const GlobalVariableTabledData& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  GlobalVariableTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:GlobalVariableTabledData)
}
inline PROTOBUF_NDEBUG_INLINE GlobalVariableTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void GlobalVariableTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
GlobalVariableTabledData::~GlobalVariableTabledData() {
  // @@protoc_insertion_point(destructor:GlobalVariableTabledData)
  SharedDtor(*this);
}
inline void GlobalVariableTabledData::SharedDtor(MessageLite& self) {
  GlobalVariableTabledData& this_ = static_cast<GlobalVariableTabledData&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* GlobalVariableTabledData::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) GlobalVariableTabledData(arena);
}
constexpr auto GlobalVariableTabledData::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(GlobalVariableTabledData, _impl_.data_) +
          decltype(GlobalVariableTabledData::_impl_.data_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(GlobalVariableTabledData), alignof(GlobalVariableTabledData), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&GlobalVariableTabledData::PlacementNew_,
                                 sizeof(GlobalVariableTabledData),
                                 alignof(GlobalVariableTabledData));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull GlobalVariableTabledData::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_GlobalVariableTabledData_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &GlobalVariableTabledData::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<GlobalVariableTabledData>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &GlobalVariableTabledData::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<GlobalVariableTabledData>(), &GlobalVariableTabledData::ByteSizeLong,
            &GlobalVariableTabledData::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(GlobalVariableTabledData, _impl_._cached_size_),
        false,
    },
    &GlobalVariableTabledData::kDescriptorMethods,
    &descriptor_table_globalvariable_5fconfig_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* GlobalVariableTabledData::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> GlobalVariableTabledData::_table_ = {
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
    ::_pbi::TcParser::GetTable<::GlobalVariableTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .GlobalVariableTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(GlobalVariableTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .GlobalVariableTable data = 1;
    {PROTOBUF_FIELD_OFFSET(GlobalVariableTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::GlobalVariableTable>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void GlobalVariableTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:GlobalVariableTabledData)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* GlobalVariableTabledData::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const GlobalVariableTabledData& this_ = static_cast<const GlobalVariableTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* GlobalVariableTabledData::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const GlobalVariableTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:GlobalVariableTabledData)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // repeated .GlobalVariableTable data = 1;
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
          // @@protoc_insertion_point(serialize_to_array_end:GlobalVariableTabledData)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t GlobalVariableTabledData::ByteSizeLong(const MessageLite& base) {
          const GlobalVariableTabledData& this_ = static_cast<const GlobalVariableTabledData&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t GlobalVariableTabledData::ByteSizeLong() const {
          const GlobalVariableTabledData& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:GlobalVariableTabledData)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // repeated .GlobalVariableTable data = 1;
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

void GlobalVariableTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<GlobalVariableTabledData*>(&to_msg);
  auto& from = static_cast<const GlobalVariableTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:GlobalVariableTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void GlobalVariableTabledData::CopyFrom(const GlobalVariableTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:GlobalVariableTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void GlobalVariableTabledData::InternalSwap(GlobalVariableTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata GlobalVariableTabledData::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_globalvariable_5fconfig_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
