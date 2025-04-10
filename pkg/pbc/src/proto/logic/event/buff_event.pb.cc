// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/buff_event.proto
// Protobuf C++ Version: 5.29.0

#include "proto/logic/event/buff_event.pb.h"

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

inline constexpr BuffTestEvet::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : period_{::uint64_t{0u}},
        periodic_timer_{::uint64_t{0u}},
        period_done_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR BuffTestEvet::BuffTestEvet(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct BuffTestEvetDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BuffTestEvetDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BuffTestEvetDefaultTypeInternal() {}
  union {
    BuffTestEvet _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BuffTestEvetDefaultTypeInternal _BuffTestEvet_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fevent_2fbuff_5fevent_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.period_),
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.periodic_timer_),
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.period_done_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::BuffTestEvet)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_BuffTestEvet_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fevent_2fbuff_5fevent_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\"proto/logic/event/buff_event.proto\"K\n\014"
    "BuffTestEvet\022\016\n\006period\030\001 \001(\004\022\026\n\016periodic"
    "_timer\030\002 \001(\004\022\023\n\013period_done\030\003 \001(\rB\tZ\007pb/"
    "gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto = {
    false,
    false,
    132,
    descriptor_table_protodef_proto_2flogic_2fevent_2fbuff_5fevent_2eproto,
    "proto/logic/event/buff_event.proto",
    &descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fevent_2fbuff_5fevent_2eproto::offsets,
    file_level_enum_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto,
    file_level_service_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto,
};
// ===================================================================

class BuffTestEvet::_Internal {
 public:
};

BuffTestEvet::BuffTestEvet(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:BuffTestEvet)
}
BuffTestEvet::BuffTestEvet(
    ::google::protobuf::Arena* arena, const BuffTestEvet& from)
    : BuffTestEvet(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE BuffTestEvet::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void BuffTestEvet::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, period_),
           0,
           offsetof(Impl_, period_done_) -
               offsetof(Impl_, period_) +
               sizeof(Impl_::period_done_));
}
BuffTestEvet::~BuffTestEvet() {
  // @@protoc_insertion_point(destructor:BuffTestEvet)
  SharedDtor(*this);
}
inline void BuffTestEvet::SharedDtor(MessageLite& self) {
  BuffTestEvet& this_ = static_cast<BuffTestEvet&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* BuffTestEvet::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) BuffTestEvet(arena);
}
constexpr auto BuffTestEvet::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(BuffTestEvet),
                                            alignof(BuffTestEvet));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull BuffTestEvet::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_BuffTestEvet_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &BuffTestEvet::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<BuffTestEvet>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &BuffTestEvet::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<BuffTestEvet>(), &BuffTestEvet::ByteSizeLong,
            &BuffTestEvet::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_._cached_size_),
        false,
    },
    &BuffTestEvet::kDescriptorMethods,
    &descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* BuffTestEvet::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 0, 2> BuffTestEvet::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::BuffTestEvet>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint64 period = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(BuffTestEvet, _impl_.period_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_)}},
    // uint64 periodic_timer = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(BuffTestEvet, _impl_.periodic_timer_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.periodic_timer_)}},
    // uint32 period_done = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTestEvet, _impl_.period_done_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_done_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 period = 1;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint64 periodic_timer = 2;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.periodic_timer_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 period_done = 3;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_done_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void BuffTestEvet::Clear() {
// @@protoc_insertion_point(message_clear_start:BuffTestEvet)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.period_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.period_done_) -
      reinterpret_cast<char*>(&_impl_.period_)) + sizeof(_impl_.period_done_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* BuffTestEvet::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const BuffTestEvet& this_ = static_cast<const BuffTestEvet&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* BuffTestEvet::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const BuffTestEvet& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:BuffTestEvet)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint64 period = 1;
          if (this_._internal_period() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
                1, this_._internal_period(), target);
          }

          // uint64 periodic_timer = 2;
          if (this_._internal_periodic_timer() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
                2, this_._internal_periodic_timer(), target);
          }

          // uint32 period_done = 3;
          if (this_._internal_period_done() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                3, this_._internal_period_done(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:BuffTestEvet)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t BuffTestEvet::ByteSizeLong(const MessageLite& base) {
          const BuffTestEvet& this_ = static_cast<const BuffTestEvet&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t BuffTestEvet::ByteSizeLong() const {
          const BuffTestEvet& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:BuffTestEvet)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // uint64 period = 1;
            if (this_._internal_period() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
                  this_._internal_period());
            }
            // uint64 periodic_timer = 2;
            if (this_._internal_periodic_timer() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
                  this_._internal_periodic_timer());
            }
            // uint32 period_done = 3;
            if (this_._internal_period_done() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_period_done());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void BuffTestEvet::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<BuffTestEvet*>(&to_msg);
  auto& from = static_cast<const BuffTestEvet&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:BuffTestEvet)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_period() != 0) {
    _this->_impl_.period_ = from._impl_.period_;
  }
  if (from._internal_periodic_timer() != 0) {
    _this->_impl_.periodic_timer_ = from._impl_.periodic_timer_;
  }
  if (from._internal_period_done() != 0) {
    _this->_impl_.period_done_ = from._impl_.period_done_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void BuffTestEvet::CopyFrom(const BuffTestEvet& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:BuffTestEvet)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void BuffTestEvet::InternalSwap(BuffTestEvet* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_done_)
      + sizeof(BuffTestEvet::_impl_.period_done_)
      - PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_)>(
          reinterpret_cast<char*>(&_impl_.period_),
          reinterpret_cast<char*>(&other->_impl_.period_));
}

::google::protobuf::Metadata BuffTestEvet::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
