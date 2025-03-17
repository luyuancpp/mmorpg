// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/event/skill_event.proto
// Protobuf C++ Version: 6.31.0-dev

#include "proto/logic/event/skill_event.pb.h"

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

inline constexpr SkillExecutedEvent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        skill_id_{::uint64_t{0u}},
        caster_{::uint64_t{0u}},
        target_{::uint64_t{0u}},
        skill_table_id_{0u} {}

template <typename>
PROTOBUF_CONSTEXPR SkillExecutedEvent::SkillExecutedEvent(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(SkillExecutedEvent_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct SkillExecutedEventDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SkillExecutedEventDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SkillExecutedEventDefaultTypeInternal() {}
  union {
    SkillExecutedEvent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SkillExecutedEventDefaultTypeInternal _SkillExecutedEvent_default_instance_;
static constexpr const ::_pb::EnumDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_enum_descriptors_proto_2flogic_2fevent_2fskill_5fevent_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_service_descriptors_proto_2flogic_2fevent_2fskill_5fevent_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fevent_2fskill_5fevent_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        0x081, // bitmap
        PROTOBUF_FIELD_OFFSET(::SkillExecutedEvent, _impl_._has_bits_),
        7, // hasbit index offset
        PROTOBUF_FIELD_OFFSET(::SkillExecutedEvent, _impl_.skill_id_),
        PROTOBUF_FIELD_OFFSET(::SkillExecutedEvent, _impl_.skill_table_id_),
        PROTOBUF_FIELD_OFFSET(::SkillExecutedEvent, _impl_.caster_),
        PROTOBUF_FIELD_OFFSET(::SkillExecutedEvent, _impl_.target_),
        0,
        3,
        1,
        2,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, sizeof(::SkillExecutedEvent)},
};
static const ::_pb::Message* PROTOBUF_NONNULL const file_default_instances[] = {
    &::_SkillExecutedEvent_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fevent_2fskill_5fevent_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n#proto/logic/event/skill_event.proto\"^\n"
    "\022SkillExecutedEvent\022\020\n\010skill_id\030\001 \001(\004\022\026\n"
    "\016skill_table_id\030\002 \001(\r\022\016\n\006caster\030\003 \001(\004\022\016\n"
    "\006target\030\004 \001(\004B\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto = {
    false,
    false,
    152,
    descriptor_table_protodef_proto_2flogic_2fevent_2fskill_5fevent_2eproto,
    "proto/logic/event/skill_event.proto",
    &descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fevent_2fskill_5fevent_2eproto::offsets,
    file_level_enum_descriptors_proto_2flogic_2fevent_2fskill_5fevent_2eproto,
    file_level_service_descriptors_proto_2flogic_2fevent_2fskill_5fevent_2eproto,
};
// ===================================================================

class SkillExecutedEvent::_Internal {
 public:
  using HasBits =
      decltype(std::declval<SkillExecutedEvent>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_._has_bits_);
};

SkillExecutedEvent::SkillExecutedEvent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, SkillExecutedEvent_class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:SkillExecutedEvent)
}
SkillExecutedEvent::SkillExecutedEvent(
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const SkillExecutedEvent& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, SkillExecutedEvent_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(from._impl_) {
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
}
PROTOBUF_NDEBUG_INLINE SkillExecutedEvent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
      : _cached_size_{0} {}

inline void SkillExecutedEvent::SharedCtor(::_pb::Arena* PROTOBUF_NULLABLE arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, skill_id_),
           0,
           offsetof(Impl_, skill_table_id_) -
               offsetof(Impl_, skill_id_) +
               sizeof(Impl_::skill_table_id_));
}
SkillExecutedEvent::~SkillExecutedEvent() {
  // @@protoc_insertion_point(destructor:SkillExecutedEvent)
  SharedDtor(*this);
}
inline void SkillExecutedEvent::SharedDtor(MessageLite& self) {
  SkillExecutedEvent& this_ = static_cast<SkillExecutedEvent&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* PROTOBUF_NONNULL SkillExecutedEvent::PlacementNew_(
    const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena) {
  return ::new (mem) SkillExecutedEvent(arena);
}
constexpr auto SkillExecutedEvent::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(SkillExecutedEvent),
                                            alignof(SkillExecutedEvent));
}
constexpr auto SkillExecutedEvent::InternalGenerateClassData_() {
  return ::google::protobuf::internal::ClassDataFull{
      ::google::protobuf::internal::ClassData{
          &_SkillExecutedEvent_default_instance_._instance,
          &_table_.header,
          nullptr,  // OnDemandRegisterArenaDtor
          nullptr,  // IsInitialized
          &SkillExecutedEvent::MergeImpl,
          ::google::protobuf::Message::GetNewImpl<SkillExecutedEvent>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
          &SkillExecutedEvent::SharedDtor,
          ::google::protobuf::Message::GetClearImpl<SkillExecutedEvent>(), &SkillExecutedEvent::ByteSizeLong,
              &SkillExecutedEvent::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
          PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_._cached_size_),
          false,
      },
      &SkillExecutedEvent::kDescriptorMethods,
      &descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto,
      nullptr,  // tracker
  };
}

PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 const ::google::protobuf::internal::ClassDataFull
        SkillExecutedEvent_class_data_ =
            SkillExecutedEvent::InternalGenerateClassData_();

const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL SkillExecutedEvent::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&SkillExecutedEvent_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(SkillExecutedEvent_class_data_.tc_table);
  return SkillExecutedEvent_class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 0, 0, 2>
SkillExecutedEvent::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_._has_bits_),
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    SkillExecutedEvent_class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::SkillExecutedEvent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 target = 4;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(SkillExecutedEvent, _impl_.target_), 2>(),
     {32, 2, 0, PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.target_)}},
    // uint64 skill_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(SkillExecutedEvent, _impl_.skill_id_), 0>(),
     {8, 0, 0, PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_id_)}},
    // uint32 skill_table_id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(SkillExecutedEvent, _impl_.skill_table_id_), 3>(),
     {16, 3, 0, PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_table_id_)}},
    // uint64 caster = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(SkillExecutedEvent, _impl_.caster_), 1>(),
     {24, 1, 0, PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.caster_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 skill_id = 1;
    {PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_id_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt64)},
    // uint32 skill_table_id = 2;
    {PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_table_id_), _Internal::kHasBitsOffset + 3, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt32)},
    // uint64 caster = 3;
    {PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.caster_), _Internal::kHasBitsOffset + 1, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt64)},
    // uint64 target = 4;
    {PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.target_), _Internal::kHasBitsOffset + 2, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};
PROTOBUF_NOINLINE void SkillExecutedEvent::Clear() {
// @@protoc_insertion_point(message_clear_start:SkillExecutedEvent)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if ((cached_has_bits & 0x0000000fu) != 0) {
    ::memset(&_impl_.skill_id_, 0, static_cast<::size_t>(
        reinterpret_cast<char*>(&_impl_.skill_table_id_) -
        reinterpret_cast<char*>(&_impl_.skill_id_)) + sizeof(_impl_.skill_table_id_));
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::uint8_t* PROTOBUF_NONNULL SkillExecutedEvent::_InternalSerialize(
    const ::google::protobuf::MessageLite& base, ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) {
  const SkillExecutedEvent& this_ = static_cast<const SkillExecutedEvent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::uint8_t* PROTOBUF_NONNULL SkillExecutedEvent::_InternalSerialize(
    ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
  const SkillExecutedEvent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(serialize_to_array_start:SkillExecutedEvent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 skill_id = 1;
  if ((this_._impl_._has_bits_[0] & 0x00000001u) != 0) {
    if (this_._internal_skill_id() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
          1, this_._internal_skill_id(), target);
    }
  }

  // uint32 skill_table_id = 2;
  if ((this_._impl_._has_bits_[0] & 0x00000008u) != 0) {
    if (this_._internal_skill_table_id() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
          2, this_._internal_skill_table_id(), target);
    }
  }

  // uint64 caster = 3;
  if ((this_._impl_._has_bits_[0] & 0x00000002u) != 0) {
    if (this_._internal_caster() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
          3, this_._internal_caster(), target);
    }
  }

  // uint64 target = 4;
  if ((this_._impl_._has_bits_[0] & 0x00000004u) != 0) {
    if (this_._internal_target() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
          4, this_._internal_target(), target);
    }
  }

  if (ABSL_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:SkillExecutedEvent)
  return target;
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::size_t SkillExecutedEvent::ByteSizeLong(const MessageLite& base) {
  const SkillExecutedEvent& this_ = static_cast<const SkillExecutedEvent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::size_t SkillExecutedEvent::ByteSizeLong() const {
  const SkillExecutedEvent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(message_byte_size_start:SkillExecutedEvent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void)cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(&this_);
  cached_has_bits = this_._impl_._has_bits_[0];
  if ((cached_has_bits & 0x0000000fu) != 0) {
    // uint64 skill_id = 1;
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (this_._internal_skill_id() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
            this_._internal_skill_id());
      }
    }
    // uint64 caster = 3;
    if ((cached_has_bits & 0x00000002u) != 0) {
      if (this_._internal_caster() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
            this_._internal_caster());
      }
    }
    // uint64 target = 4;
    if ((cached_has_bits & 0x00000004u) != 0) {
      if (this_._internal_target() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
            this_._internal_target());
      }
    }
    // uint32 skill_table_id = 2;
    if ((cached_has_bits & 0x00000008u) != 0) {
      if (this_._internal_skill_table_id() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
            this_._internal_skill_table_id());
      }
    }
  }
  return this_.MaybeComputeUnknownFieldsSize(total_size,
                                             &this_._impl_._cached_size_);
}

void SkillExecutedEvent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<SkillExecutedEvent*>(&to_msg);
  auto& from = static_cast<const SkillExecutedEvent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:SkillExecutedEvent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if ((cached_has_bits & 0x0000000fu) != 0) {
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (from._internal_skill_id() != 0) {
        _this->_impl_.skill_id_ = from._impl_.skill_id_;
      }
    }
    if ((cached_has_bits & 0x00000002u) != 0) {
      if (from._internal_caster() != 0) {
        _this->_impl_.caster_ = from._impl_.caster_;
      }
    }
    if ((cached_has_bits & 0x00000004u) != 0) {
      if (from._internal_target() != 0) {
        _this->_impl_.target_ = from._impl_.target_;
      }
    }
    if ((cached_has_bits & 0x00000008u) != 0) {
      if (from._internal_skill_table_id() != 0) {
        _this->_impl_.skill_table_id_ = from._impl_.skill_table_id_;
      }
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void SkillExecutedEvent::CopyFrom(const SkillExecutedEvent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:SkillExecutedEvent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void SkillExecutedEvent::InternalSwap(SkillExecutedEvent* PROTOBUF_RESTRICT PROTOBUF_NONNULL other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_table_id_)
      + sizeof(SkillExecutedEvent::_impl_.skill_table_id_)
      - PROTOBUF_FIELD_OFFSET(SkillExecutedEvent, _impl_.skill_id_)>(
          reinterpret_cast<char*>(&_impl_.skill_id_),
          reinterpret_cast<char*>(&other->_impl_.skill_id_));
}

::google::protobuf::Metadata SkillExecutedEvent::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fevent_2fskill_5fevent_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
