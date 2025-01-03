// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/component/time_comp.proto
// Protobuf C++ Version: 5.26.1

#include "proto/logic/component/time_comp.pb.h"

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

inline constexpr TimeMeterComp::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : start_{::uint64_t{0u}},
        duration_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR TimeMeterComp::TimeMeterComp(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct TimeMeterCompDefaultTypeInternal {
  PROTOBUF_CONSTEXPR TimeMeterCompDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~TimeMeterCompDefaultTypeInternal() {}
  union {
    TimeMeterComp _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 TimeMeterCompDefaultTypeInternal _TimeMeterComp_default_instance_;

inline constexpr CooldownTimeComp::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : start_{::uint64_t{0u}},
        cooldown_table_id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR CooldownTimeComp::CooldownTimeComp(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CooldownTimeCompDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CooldownTimeCompDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CooldownTimeCompDefaultTypeInternal() {}
  union {
    CooldownTimeComp _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CooldownTimeCompDefaultTypeInternal _CooldownTimeComp_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR CooldownTimeListComp_CooldownListEntry_DoNotUse::CooldownTimeListComp_CooldownListEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct CooldownTimeListComp_CooldownListEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CooldownTimeListComp_CooldownListEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CooldownTimeListComp_CooldownListEntry_DoNotUseDefaultTypeInternal() {}
  union {
    CooldownTimeListComp_CooldownListEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CooldownTimeListComp_CooldownListEntry_DoNotUseDefaultTypeInternal _CooldownTimeListComp_CooldownListEntry_DoNotUse_default_instance_;

inline constexpr CooldownTimeListComp::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : cooldown_list_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR CooldownTimeListComp::CooldownTimeListComp(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CooldownTimeListCompDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CooldownTimeListCompDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CooldownTimeListCompDefaultTypeInternal() {}
  union {
    CooldownTimeListComp _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CooldownTimeListCompDefaultTypeInternal _CooldownTimeListComp_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[4];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::TimeMeterComp, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::TimeMeterComp, _impl_.start_),
        PROTOBUF_FIELD_OFFSET(::TimeMeterComp, _impl_.duration_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CooldownTimeComp, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CooldownTimeComp, _impl_.start_),
        PROTOBUF_FIELD_OFFSET(::CooldownTimeComp, _impl_.cooldown_table_id_),
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp_CooldownListEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp_CooldownListEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp_CooldownListEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp_CooldownListEntry_DoNotUse, value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CooldownTimeListComp, _impl_.cooldown_list_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::TimeMeterComp)},
        {10, -1, -1, sizeof(::CooldownTimeComp)},
        {20, 30, -1, sizeof(::CooldownTimeListComp_CooldownListEntry_DoNotUse)},
        {32, -1, -1, sizeof(::CooldownTimeListComp)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_TimeMeterComp_default_instance_._instance,
    &::_CooldownTimeComp_default_instance_._instance,
    &::_CooldownTimeListComp_CooldownListEntry_DoNotUse_default_instance_._instance,
    &::_CooldownTimeListComp_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n%proto/logic/component/time_comp.proto\""
    "0\n\rTimeMeterComp\022\r\n\005start\030\001 \001(\004\022\020\n\010durat"
    "ion\030\002 \001(\004\"<\n\020CooldownTimeComp\022\r\n\005start\030\001"
    " \001(\004\022\031\n\021cooldown_table_id\030\002 \001(\r\"\236\001\n\024Cool"
    "downTimeListComp\022>\n\rcooldown_list\030\001 \003(\0132"
    "\'.CooldownTimeListComp.CooldownListEntry"
    "\032F\n\021CooldownListEntry\022\013\n\003key\030\001 \001(\r\022 \n\005va"
    "lue\030\002 \001(\0132\021.CooldownTimeComp:\0028\001B\tZ\007pb/g"
    "ameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto = {
    false,
    false,
    331,
    descriptor_table_protodef_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto,
    "proto/logic/component/time_comp.proto",
    &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once,
    nullptr,
    0,
    4,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto::offsets,
    file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto,
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto,
    file_level_service_descriptors_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_getter() {
  return &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto;
}
// ===================================================================

class TimeMeterComp::_Internal {
 public:
};

TimeMeterComp::TimeMeterComp(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:TimeMeterComp)
}
TimeMeterComp::TimeMeterComp(
    ::google::protobuf::Arena* arena, const TimeMeterComp& from)
    : TimeMeterComp(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE TimeMeterComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void TimeMeterComp::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, start_),
           0,
           offsetof(Impl_, duration_) -
               offsetof(Impl_, start_) +
               sizeof(Impl_::duration_));
}
TimeMeterComp::~TimeMeterComp() {
  // @@protoc_insertion_point(destructor:TimeMeterComp)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void TimeMeterComp::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
TimeMeterComp::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_._cached_size_),
              false,
          },
          &TimeMeterComp::MergeImpl,
          &TimeMeterComp::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void TimeMeterComp::Clear() {
// @@protoc_insertion_point(message_clear_start:TimeMeterComp)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.start_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.duration_) -
      reinterpret_cast<char*>(&_impl_.start_)) + sizeof(_impl_.duration_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* TimeMeterComp::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> TimeMeterComp::_table_ = {
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
    &_TimeMeterComp_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::TimeMeterComp>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 duration = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(TimeMeterComp, _impl_.duration_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.duration_)}},
    // uint64 start = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(TimeMeterComp, _impl_.start_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.start_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 start = 1;
    {PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.start_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint64 duration = 2;
    {PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.duration_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* TimeMeterComp::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:TimeMeterComp)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 start = 1;
  if (this->_internal_start() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_start(), target);
  }

  // uint64 duration = 2;
  if (this->_internal_duration() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        2, this->_internal_duration(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:TimeMeterComp)
  return target;
}

::size_t TimeMeterComp::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:TimeMeterComp)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 start = 1;
  if (this->_internal_start() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_start());
  }

  // uint64 duration = 2;
  if (this->_internal_duration() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_duration());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void TimeMeterComp::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<TimeMeterComp*>(&to_msg);
  auto& from = static_cast<const TimeMeterComp&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:TimeMeterComp)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_start() != 0) {
    _this->_impl_.start_ = from._impl_.start_;
  }
  if (from._internal_duration() != 0) {
    _this->_impl_.duration_ = from._impl_.duration_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void TimeMeterComp::CopyFrom(const TimeMeterComp& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:TimeMeterComp)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool TimeMeterComp::IsInitialized() const {
  return true;
}

void TimeMeterComp::InternalSwap(TimeMeterComp* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.duration_)
      + sizeof(TimeMeterComp::_impl_.duration_)
      - PROTOBUF_FIELD_OFFSET(TimeMeterComp, _impl_.start_)>(
          reinterpret_cast<char*>(&_impl_.start_),
          reinterpret_cast<char*>(&other->_impl_.start_));
}

::google::protobuf::Metadata TimeMeterComp::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[0]);
}
// ===================================================================

class CooldownTimeComp::_Internal {
 public:
};

CooldownTimeComp::CooldownTimeComp(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:CooldownTimeComp)
}
CooldownTimeComp::CooldownTimeComp(
    ::google::protobuf::Arena* arena, const CooldownTimeComp& from)
    : CooldownTimeComp(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE CooldownTimeComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void CooldownTimeComp::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, start_),
           0,
           offsetof(Impl_, cooldown_table_id_) -
               offsetof(Impl_, start_) +
               sizeof(Impl_::cooldown_table_id_));
}
CooldownTimeComp::~CooldownTimeComp() {
  // @@protoc_insertion_point(destructor:CooldownTimeComp)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void CooldownTimeComp::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CooldownTimeComp::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_._cached_size_),
              false,
          },
          &CooldownTimeComp::MergeImpl,
          &CooldownTimeComp::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void CooldownTimeComp::Clear() {
// @@protoc_insertion_point(message_clear_start:CooldownTimeComp)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.start_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.cooldown_table_id_) -
      reinterpret_cast<char*>(&_impl_.start_)) + sizeof(_impl_.cooldown_table_id_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* CooldownTimeComp::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> CooldownTimeComp::_table_ = {
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
    &_CooldownTimeComp_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::CooldownTimeComp>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 cooldown_table_id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(CooldownTimeComp, _impl_.cooldown_table_id_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.cooldown_table_id_)}},
    // uint64 start = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(CooldownTimeComp, _impl_.start_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.start_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 start = 1;
    {PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.start_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 cooldown_table_id = 2;
    {PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.cooldown_table_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* CooldownTimeComp::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:CooldownTimeComp)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 start = 1;
  if (this->_internal_start() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_start(), target);
  }

  // uint32 cooldown_table_id = 2;
  if (this->_internal_cooldown_table_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_cooldown_table_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:CooldownTimeComp)
  return target;
}

::size_t CooldownTimeComp::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:CooldownTimeComp)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 start = 1;
  if (this->_internal_start() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_start());
  }

  // uint32 cooldown_table_id = 2;
  if (this->_internal_cooldown_table_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_cooldown_table_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void CooldownTimeComp::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<CooldownTimeComp*>(&to_msg);
  auto& from = static_cast<const CooldownTimeComp&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:CooldownTimeComp)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_start() != 0) {
    _this->_impl_.start_ = from._impl_.start_;
  }
  if (from._internal_cooldown_table_id() != 0) {
    _this->_impl_.cooldown_table_id_ = from._impl_.cooldown_table_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void CooldownTimeComp::CopyFrom(const CooldownTimeComp& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:CooldownTimeComp)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool CooldownTimeComp::IsInitialized() const {
  return true;
}

void CooldownTimeComp::InternalSwap(CooldownTimeComp* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.cooldown_table_id_)
      + sizeof(CooldownTimeComp::_impl_.cooldown_table_id_)
      - PROTOBUF_FIELD_OFFSET(CooldownTimeComp, _impl_.start_)>(
          reinterpret_cast<char*>(&_impl_.start_),
          reinterpret_cast<char*>(&other->_impl_.start_));
}

::google::protobuf::Metadata CooldownTimeComp::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[1]);
}
// ===================================================================

CooldownTimeListComp_CooldownListEntry_DoNotUse::CooldownTimeListComp_CooldownListEntry_DoNotUse() {}
CooldownTimeListComp_CooldownListEntry_DoNotUse::CooldownTimeListComp_CooldownListEntry_DoNotUse(::google::protobuf::Arena* arena)
    : SuperType(arena) {}
::google::protobuf::Metadata CooldownTimeListComp_CooldownListEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[2]);
}
// ===================================================================

class CooldownTimeListComp::_Internal {
 public:
};

CooldownTimeListComp::CooldownTimeListComp(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:CooldownTimeListComp)
}
inline PROTOBUF_NDEBUG_INLINE CooldownTimeListComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : cooldown_list_{visibility, arena, from.cooldown_list_},
        _cached_size_{0} {}

CooldownTimeListComp::CooldownTimeListComp(
    ::google::protobuf::Arena* arena,
    const CooldownTimeListComp& from)
    : ::google::protobuf::Message(arena) {
  CooldownTimeListComp* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:CooldownTimeListComp)
}
inline PROTOBUF_NDEBUG_INLINE CooldownTimeListComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : cooldown_list_{visibility, arena},
        _cached_size_{0} {}

inline void CooldownTimeListComp::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
CooldownTimeListComp::~CooldownTimeListComp() {
  // @@protoc_insertion_point(destructor:CooldownTimeListComp)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void CooldownTimeListComp::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CooldownTimeListComp::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(CooldownTimeListComp, _impl_._cached_size_),
              false,
          },
          &CooldownTimeListComp::MergeImpl,
          &CooldownTimeListComp::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void CooldownTimeListComp::Clear() {
// @@protoc_insertion_point(message_clear_start:CooldownTimeListComp)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.cooldown_list_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* CooldownTimeListComp::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 2, 0, 2> CooldownTimeListComp::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_CooldownTimeListComp_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::CooldownTimeListComp>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // map<uint32, .CooldownTimeComp> cooldown_list = 1;
    {PROTOBUF_FIELD_OFFSET(CooldownTimeListComp, _impl_.cooldown_list_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(CooldownTimeListComp()._impl_.cooldown_list_)>(
        0, 0, 0, 13,
        11)},
    {::_pbi::TcParser::CreateInArenaStorageCb<::CooldownTimeComp>},
  }}, {{
  }},
};

::uint8_t* CooldownTimeListComp::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:CooldownTimeListComp)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // map<uint32, .CooldownTimeComp> cooldown_list = 1;
  if (!_internal_cooldown_list().empty()) {
    using MapType = ::google::protobuf::Map<::uint32_t, ::CooldownTimeComp>;
    using WireHelper = _pbi::MapEntryFuncs<::uint32_t, ::CooldownTimeComp,
                                   _pbi::WireFormatLite::TYPE_UINT32,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>;
    const auto& field = _internal_cooldown_list();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterFlat<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
      }
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:CooldownTimeListComp)
  return target;
}

::size_t CooldownTimeListComp::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:CooldownTimeListComp)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // map<uint32, .CooldownTimeComp> cooldown_list = 1;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_cooldown_list_size());
  for (const auto& entry : _internal_cooldown_list()) {
    total_size += _pbi::MapEntryFuncs<::uint32_t, ::CooldownTimeComp,
                                   _pbi::WireFormatLite::TYPE_UINT32,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>::ByteSizeLong(entry.first, entry.second);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void CooldownTimeListComp::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<CooldownTimeListComp*>(&to_msg);
  auto& from = static_cast<const CooldownTimeListComp&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:CooldownTimeListComp)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.cooldown_list_.MergeFrom(from._impl_.cooldown_list_);
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void CooldownTimeListComp::CopyFrom(const CooldownTimeListComp& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:CooldownTimeListComp)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool CooldownTimeListComp::IsInitialized() const {
  return true;
}

void CooldownTimeListComp::InternalSwap(CooldownTimeListComp* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.cooldown_list_.InternalSwap(&other->_impl_.cooldown_list_);
}

::google::protobuf::Metadata CooldownTimeListComp::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto[3]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2ftime_5fcomp_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
