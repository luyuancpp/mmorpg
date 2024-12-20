// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: class_config.proto
// Protobuf C++ Version: 5.26.1

#include "class_config.pb.h"

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

inline constexpr ClassTable::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : skill_{},
        _skill_cached_byte_size_{0},
        id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ClassTable::ClassTable(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ClassTableDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ClassTableDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ClassTableDefaultTypeInternal() {}
  union {
    ClassTable _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ClassTableDefaultTypeInternal _ClassTable_default_instance_;

inline constexpr ClassTabledData::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : data_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ClassTabledData::ClassTabledData(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ClassTabledDataDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ClassTabledDataDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ClassTabledDataDefaultTypeInternal() {}
  union {
    ClassTabledData _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ClassTabledDataDefaultTypeInternal _ClassTabledData_default_instance_;
static ::_pb::Metadata file_level_metadata_class_5fconfig_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_class_5fconfig_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_class_5fconfig_2eproto = nullptr;
const ::uint32_t
    TableStruct_class_5fconfig_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::ClassTable, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::ClassTable, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::ClassTable, _impl_.skill_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::ClassTabledData, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::ClassTabledData, _impl_.data_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::ClassTable)},
        {10, -1, -1, sizeof(::ClassTabledData)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_ClassTable_default_instance_._instance,
    &::_ClassTabledData_default_instance_._instance,
};
const char descriptor_table_protodef_class_5fconfig_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\022class_config.proto\"\'\n\nClassTable\022\n\n\002id"
    "\030\001 \001(\r\022\r\n\005skill\030\002 \003(\r\",\n\017ClassTabledData"
    "\022\031\n\004data\030\001 \003(\0132\013.ClassTableB\tZ\007pb/gameb\006"
    "proto3"
};
static ::absl::once_flag descriptor_table_class_5fconfig_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_class_5fconfig_2eproto = {
    false,
    false,
    126,
    descriptor_table_protodef_class_5fconfig_2eproto,
    "class_config.proto",
    &descriptor_table_class_5fconfig_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_class_5fconfig_2eproto::offsets,
    file_level_metadata_class_5fconfig_2eproto,
    file_level_enum_descriptors_class_5fconfig_2eproto,
    file_level_service_descriptors_class_5fconfig_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_class_5fconfig_2eproto_getter() {
  return &descriptor_table_class_5fconfig_2eproto;
}
// ===================================================================

class ClassTable::_Internal {
 public:
};

ClassTable::ClassTable(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:ClassTable)
}
inline PROTOBUF_NDEBUG_INLINE ClassTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : skill_{visibility, arena, from.skill_},
        _skill_cached_byte_size_{0},
        _cached_size_{0} {}

ClassTable::ClassTable(
    ::google::protobuf::Arena* arena,
    const ClassTable& from)
    : ::google::protobuf::Message(arena) {
  ClassTable* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  _impl_.id_ = from._impl_.id_;

  // @@protoc_insertion_point(copy_constructor:ClassTable)
}
inline PROTOBUF_NDEBUG_INLINE ClassTable::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : skill_{visibility, arena},
        _skill_cached_byte_size_{0},
        _cached_size_{0} {}

inline void ClassTable::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.id_ = {};
}
ClassTable::~ClassTable() {
  // @@protoc_insertion_point(destructor:ClassTable)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void ClassTable::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
ClassTable::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(ClassTable, _impl_._cached_size_),
              false,
          },
          &ClassTable::MergeImpl,
          &ClassTable::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void ClassTable::Clear() {
// @@protoc_insertion_point(message_clear_start:ClassTable)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.skill_.Clear();
  _impl_.id_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* ClassTable::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> ClassTable::_table_ = {
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
    &_ClassTable_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::ClassTable>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated uint32 skill = 2;
    {::_pbi::TcParser::FastV32P1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(ClassTable, _impl_.skill_)}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(ClassTable, _impl_.id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(ClassTable, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(ClassTable, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // repeated uint32 skill = 2;
    {PROTOBUF_FIELD_OFFSET(ClassTable, _impl_.skill_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* ClassTable::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:ClassTable)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 id = 1;
  if (this->_internal_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_id(), target);
  }

  // repeated uint32 skill = 2;
  {
    int byte_size = _impl_._skill_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt32Packed(
          2, _internal_skill(), byte_size, target);
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:ClassTable)
  return target;
}

::size_t ClassTable::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:ClassTable)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated uint32 skill = 2;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt32Size(
        this->_internal_skill())
    ;
    _impl_._skill_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
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

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void ClassTable::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ClassTable*>(&to_msg);
  auto& from = static_cast<const ClassTable&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:ClassTable)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_skill()->MergeFrom(from._internal_skill());
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ClassTable::CopyFrom(const ClassTable& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:ClassTable)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool ClassTable::IsInitialized() const {
  return true;
}

void ClassTable::InternalSwap(ClassTable* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.skill_.InternalSwap(&other->_impl_.skill_);
        swap(_impl_.id_, other->_impl_.id_);
}

::google::protobuf::Metadata ClassTable::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_class_5fconfig_2eproto_getter,
                                   &descriptor_table_class_5fconfig_2eproto_once,
                                   file_level_metadata_class_5fconfig_2eproto[0]);
}
// ===================================================================

class ClassTabledData::_Internal {
 public:
};

ClassTabledData::ClassTabledData(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:ClassTabledData)
}
inline PROTOBUF_NDEBUG_INLINE ClassTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : data_{visibility, arena, from.data_},
        _cached_size_{0} {}

ClassTabledData::ClassTabledData(
    ::google::protobuf::Arena* arena,
    const ClassTabledData& from)
    : ::google::protobuf::Message(arena) {
  ClassTabledData* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:ClassTabledData)
}
inline PROTOBUF_NDEBUG_INLINE ClassTabledData::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : data_{visibility, arena},
        _cached_size_{0} {}

inline void ClassTabledData::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
ClassTabledData::~ClassTabledData() {
  // @@protoc_insertion_point(destructor:ClassTabledData)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void ClassTabledData::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
ClassTabledData::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(ClassTabledData, _impl_._cached_size_),
              false,
          },
          &ClassTabledData::MergeImpl,
          &ClassTabledData::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void ClassTabledData::Clear() {
// @@protoc_insertion_point(message_clear_start:ClassTabledData)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.data_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* ClassTabledData::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> ClassTabledData::_table_ = {
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
    &_ClassTabledData_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::ClassTabledData>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .ClassTable data = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(ClassTabledData, _impl_.data_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .ClassTable data = 1;
    {PROTOBUF_FIELD_OFFSET(ClassTabledData, _impl_.data_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::ClassTable>()},
  }}, {{
  }},
};

::uint8_t* ClassTabledData::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:ClassTabledData)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated .ClassTable data = 1;
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
  // @@protoc_insertion_point(serialize_to_array_end:ClassTabledData)
  return target;
}

::size_t ClassTabledData::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:ClassTabledData)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated .ClassTable data = 1;
  total_size += 1UL * this->_internal_data_size();
  for (const auto& msg : this->_internal_data()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void ClassTabledData::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ClassTabledData*>(&to_msg);
  auto& from = static_cast<const ClassTabledData&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:ClassTabledData)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_data()->MergeFrom(
      from._internal_data());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ClassTabledData::CopyFrom(const ClassTabledData& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:ClassTabledData)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool ClassTabledData::IsInitialized() const {
  return true;
}

void ClassTabledData::InternalSwap(ClassTabledData* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.data_.InternalSwap(&other->_impl_.data_);
}

::google::protobuf::Metadata ClassTabledData::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_class_5fconfig_2eproto_getter,
                                   &descriptor_table_class_5fconfig_2eproto_once,
                                   file_level_metadata_class_5fconfig_2eproto[1]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_class_5fconfig_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
