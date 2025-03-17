// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/buff_event.proto
// Protobuf C++ Version: 6.31.0-dev

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
      : _cached_size_{0},
        period_{::uint64_t{0u}},
        periodic_timer_{::uint64_t{0u}},
        period_done_{0u} {}

template <typename>
PROTOBUF_CONSTEXPR BuffTestEvet::BuffTestEvet(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(BuffTestEvet_class_data_.base()),
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
static constexpr const ::_pb::EnumDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_enum_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_service_descriptors_proto_2flogic_2fevent_2fbuff_5fevent_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fevent_2fbuff_5fevent_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        0x081, // bitmap
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_._has_bits_),
        6, // hasbit index offset
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.period_),
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.periodic_timer_),
        PROTOBUF_FIELD_OFFSET(::BuffTestEvet, _impl_.period_done_),
        0,
        1,
        2,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, sizeof(::BuffTestEvet)},
};
static const ::_pb::Message* PROTOBUF_NONNULL const file_default_instances[] = {
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
  using HasBits =
      decltype(std::declval<BuffTestEvet>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_._has_bits_);
};

BuffTestEvet::BuffTestEvet(::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, BuffTestEvet_class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:BuffTestEvet)
}
BuffTestEvet::BuffTestEvet(
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const BuffTestEvet& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, BuffTestEvet_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(from._impl_) {
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
}
PROTOBUF_NDEBUG_INLINE BuffTestEvet::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
      : _cached_size_{0} {}

inline void BuffTestEvet::SharedCtor(::_pb::Arena* PROTOBUF_NULLABLE arena) {
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

inline void* PROTOBUF_NONNULL BuffTestEvet::PlacementNew_(
    const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena) {
  return ::new (mem) BuffTestEvet(arena);
}
constexpr auto BuffTestEvet::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(BuffTestEvet),
                                            alignof(BuffTestEvet));
}
constexpr auto BuffTestEvet::InternalGenerateClassData_() {
  return ::google::protobuf::internal::ClassDataFull{
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
}

PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 const ::google::protobuf::internal::ClassDataFull
        BuffTestEvet_class_data_ =
            BuffTestEvet::InternalGenerateClassData_();

const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL BuffTestEvet::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&BuffTestEvet_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(BuffTestEvet_class_data_.tc_table);
  return BuffTestEvet_class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 0, 2>
BuffTestEvet::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_._has_bits_),
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    BuffTestEvet_class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::BuffTestEvet>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint64 period = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(BuffTestEvet, _impl_.period_), 0>(),
     {8, 0, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_)}},
    // uint64 periodic_timer = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(BuffTestEvet, _impl_.periodic_timer_), 1>(),
     {16, 1, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.periodic_timer_)}},
    // uint32 period_done = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(BuffTestEvet, _impl_.period_done_), 2>(),
     {24, 2, 0, PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_done_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 period = 1;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt64)},
    // uint64 periodic_timer = 2;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.periodic_timer_), _Internal::kHasBitsOffset + 1, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt64)},
    // uint32 period_done = 3;
    {PROTOBUF_FIELD_OFFSET(BuffTestEvet, _impl_.period_done_), _Internal::kHasBitsOffset + 2, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt32)},
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

  cached_has_bits = _impl_._has_bits_[0];
  if ((cached_has_bits & 0x00000007u) != 0) {
    ::memset(&_impl_.period_, 0, static_cast<::size_t>(
        reinterpret_cast<char*>(&_impl_.period_done_) -
        reinterpret_cast<char*>(&_impl_.period_)) + sizeof(_impl_.period_done_));
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::uint8_t* PROTOBUF_NONNULL BuffTestEvet::_InternalSerialize(
    const ::google::protobuf::MessageLite& base, ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) {
  const BuffTestEvet& this_ = static_cast<const BuffTestEvet&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::uint8_t* PROTOBUF_NONNULL BuffTestEvet::_InternalSerialize(
    ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
  const BuffTestEvet& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(serialize_to_array_start:BuffTestEvet)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 period = 1;
  if ((this_._impl_._has_bits_[0] & 0x00000001u) != 0) {
    if (this_._internal_period() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
          1, this_._internal_period(), target);
    }
  }

  // uint64 periodic_timer = 2;
  if ((this_._impl_._has_bits_[0] & 0x00000002u) != 0) {
    if (this_._internal_periodic_timer() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
          2, this_._internal_periodic_timer(), target);
    }
  }

  // uint32 period_done = 3;
  if ((this_._impl_._has_bits_[0] & 0x00000004u) != 0) {
    if (this_._internal_period_done() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
          3, this_._internal_period_done(), target);
    }
  }

  if (ABSL_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
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
  cached_has_bits = this_._impl_._has_bits_[0];
  if ((cached_has_bits & 0x00000007u) != 0) {
    // uint64 period = 1;
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (this_._internal_period() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
            this_._internal_period());
      }
    }
    // uint64 periodic_timer = 2;
    if ((cached_has_bits & 0x00000002u) != 0) {
      if (this_._internal_periodic_timer() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
            this_._internal_periodic_timer());
      }
    }
    // uint32 period_done = 3;
    if ((cached_has_bits & 0x00000004u) != 0) {
      if (this_._internal_period_done() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
            this_._internal_period_done());
      }
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

  cached_has_bits = from._impl_._has_bits_[0];
  if ((cached_has_bits & 0x00000007u) != 0) {
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (from._internal_period() != 0) {
        _this->_impl_.period_ = from._impl_.period_;
      }
    }
    if ((cached_has_bits & 0x00000002u) != 0) {
      if (from._internal_periodic_timer() != 0) {
        _this->_impl_.periodic_timer_ = from._impl_.periodic_timer_;
      }
    }
    if ((cached_has_bits & 0x00000004u) != 0) {
      if (from._internal_period_done() != 0) {
        _this->_impl_.period_done_ = from._impl_.period_done_;
      }
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void BuffTestEvet::CopyFrom(const BuffTestEvet& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:BuffTestEvet)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void BuffTestEvet::InternalSwap(BuffTestEvet* PROTOBUF_RESTRICT PROTOBUF_NONNULL other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
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
    _static_init2_ [[maybe_unused]] =
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fevent_2fbuff_5fevent_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
