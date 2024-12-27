// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/common/statistics.proto
// Protobuf C++ Version: 5.26.1

#include "proto/common/statistics.pb.h"

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

inline constexpr MessageStatistics::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : count_{0u},
        flow_rate_second_{0u},
        flow_rate_total_{::uint64_t{0u}},
        second_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR MessageStatistics::MessageStatistics(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct MessageStatisticsDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MessageStatisticsDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MessageStatisticsDefaultTypeInternal() {}
  union {
    MessageStatistics _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MessageStatisticsDefaultTypeInternal _MessageStatistics_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2fcommon_2fstatistics_2eproto[1];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2fcommon_2fstatistics_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2fcommon_2fstatistics_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2fcommon_2fstatistics_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::MessageStatistics, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::MessageStatistics, _impl_.count_),
        PROTOBUF_FIELD_OFFSET(::MessageStatistics, _impl_.flow_rate_second_),
        PROTOBUF_FIELD_OFFSET(::MessageStatistics, _impl_.flow_rate_total_),
        PROTOBUF_FIELD_OFFSET(::MessageStatistics, _impl_.second_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::MessageStatistics)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_MessageStatistics_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2fcommon_2fstatistics_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\035proto/common/statistics.proto\"e\n\021Messa"
    "geStatistics\022\r\n\005count\030\001 \001(\r\022\030\n\020flow_rate"
    "_second\030\002 \001(\r\022\027\n\017flow_rate_total\030\003 \001(\004\022\016"
    "\n\006second\030\004 \001(\rB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2fcommon_2fstatistics_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2fcommon_2fstatistics_2eproto = {
    false,
    false,
    153,
    descriptor_table_protodef_proto_2fcommon_2fstatistics_2eproto,
    "proto/common/statistics.proto",
    &descriptor_table_proto_2fcommon_2fstatistics_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2fcommon_2fstatistics_2eproto::offsets,
    file_level_metadata_proto_2fcommon_2fstatistics_2eproto,
    file_level_enum_descriptors_proto_2fcommon_2fstatistics_2eproto,
    file_level_service_descriptors_proto_2fcommon_2fstatistics_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2fcommon_2fstatistics_2eproto_getter() {
  return &descriptor_table_proto_2fcommon_2fstatistics_2eproto;
}
// ===================================================================

class MessageStatistics::_Internal {
 public:
};

MessageStatistics::MessageStatistics(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:MessageStatistics)
}
MessageStatistics::MessageStatistics(
    ::google::protobuf::Arena* arena, const MessageStatistics& from)
    : MessageStatistics(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE MessageStatistics::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void MessageStatistics::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, count_),
           0,
           offsetof(Impl_, second_) -
               offsetof(Impl_, count_) +
               sizeof(Impl_::second_));
}
MessageStatistics::~MessageStatistics() {
  // @@protoc_insertion_point(destructor:MessageStatistics)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void MessageStatistics::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
MessageStatistics::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_._cached_size_),
              false,
          },
          &MessageStatistics::MergeImpl,
          &MessageStatistics::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void MessageStatistics::Clear() {
// @@protoc_insertion_point(message_clear_start:MessageStatistics)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.count_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.second_) -
      reinterpret_cast<char*>(&_impl_.count_)) + sizeof(_impl_.second_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* MessageStatistics::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 0, 0, 2> MessageStatistics::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_MessageStatistics_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::MessageStatistics>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 second = 4;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(MessageStatistics, _impl_.second_), 63>(),
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.second_)}},
    // uint32 count = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(MessageStatistics, _impl_.count_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.count_)}},
    // uint32 flow_rate_second = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(MessageStatistics, _impl_.flow_rate_second_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.flow_rate_second_)}},
    // uint64 flow_rate_total = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(MessageStatistics, _impl_.flow_rate_total_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.flow_rate_total_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 count = 1;
    {PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.count_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 flow_rate_second = 2;
    {PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.flow_rate_second_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint64 flow_rate_total = 3;
    {PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.flow_rate_total_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 second = 4;
    {PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.second_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* MessageStatistics::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:MessageStatistics)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 count = 1;
  if (this->_internal_count() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_count(), target);
  }

  // uint32 flow_rate_second = 2;
  if (this->_internal_flow_rate_second() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_flow_rate_second(), target);
  }

  // uint64 flow_rate_total = 3;
  if (this->_internal_flow_rate_total() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        3, this->_internal_flow_rate_total(), target);
  }

  // uint32 second = 4;
  if (this->_internal_second() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        4, this->_internal_second(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:MessageStatistics)
  return target;
}

::size_t MessageStatistics::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:MessageStatistics)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint32 count = 1;
  if (this->_internal_count() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_count());
  }

  // uint32 flow_rate_second = 2;
  if (this->_internal_flow_rate_second() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_flow_rate_second());
  }

  // uint64 flow_rate_total = 3;
  if (this->_internal_flow_rate_total() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_flow_rate_total());
  }

  // uint32 second = 4;
  if (this->_internal_second() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_second());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void MessageStatistics::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<MessageStatistics*>(&to_msg);
  auto& from = static_cast<const MessageStatistics&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:MessageStatistics)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_count() != 0) {
    _this->_impl_.count_ = from._impl_.count_;
  }
  if (from._internal_flow_rate_second() != 0) {
    _this->_impl_.flow_rate_second_ = from._impl_.flow_rate_second_;
  }
  if (from._internal_flow_rate_total() != 0) {
    _this->_impl_.flow_rate_total_ = from._impl_.flow_rate_total_;
  }
  if (from._internal_second() != 0) {
    _this->_impl_.second_ = from._impl_.second_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void MessageStatistics::CopyFrom(const MessageStatistics& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:MessageStatistics)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool MessageStatistics::IsInitialized() const {
  return true;
}

void MessageStatistics::InternalSwap(MessageStatistics* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.second_)
      + sizeof(MessageStatistics::_impl_.second_)
      - PROTOBUF_FIELD_OFFSET(MessageStatistics, _impl_.count_)>(
          reinterpret_cast<char*>(&_impl_.count_),
          reinterpret_cast<char*>(&other->_impl_.count_));
}

::google::protobuf::Metadata MessageStatistics::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fcommon_2fstatistics_2eproto_getter,
                                   &descriptor_table_proto_2fcommon_2fstatistics_2eproto_once,
                                   file_level_metadata_proto_2fcommon_2fstatistics_2eproto[0]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_proto_2fcommon_2fstatistics_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
