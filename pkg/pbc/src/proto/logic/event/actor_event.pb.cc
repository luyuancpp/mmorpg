// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/event/actor_event.proto
// Protobuf C++ Version: 5.26.1

#include "proto/logic/event/actor_event.pb.h"

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

inline constexpr InterruptCurrentStatePbEvent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : actor_entity_{::uint64_t{0u}},
        actor_action_{0u},
        actor_state_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR InterruptCurrentStatePbEvent::InterruptCurrentStatePbEvent(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct InterruptCurrentStatePbEventDefaultTypeInternal {
  PROTOBUF_CONSTEXPR InterruptCurrentStatePbEventDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~InterruptCurrentStatePbEventDefaultTypeInternal() {}
  union {
    InterruptCurrentStatePbEvent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 InterruptCurrentStatePbEventDefaultTypeInternal _InterruptCurrentStatePbEvent_default_instance_;

inline constexpr InitializeActorComponentsEvent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : actor_entity_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR InitializeActorComponentsEvent::InitializeActorComponentsEvent(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct InitializeActorComponentsEventDefaultTypeInternal {
  PROTOBUF_CONSTEXPR InitializeActorComponentsEventDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~InitializeActorComponentsEventDefaultTypeInternal() {}
  union {
    InitializeActorComponentsEvent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 InitializeActorComponentsEventDefaultTypeInternal _InitializeActorComponentsEvent_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2flogic_2fevent_2factor_5fevent_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fevent_2factor_5fevent_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fevent_2factor_5fevent_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fevent_2factor_5fevent_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::InitializeActorComponentsEvent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::InitializeActorComponentsEvent, _impl_.actor_entity_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::InterruptCurrentStatePbEvent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::InterruptCurrentStatePbEvent, _impl_.actor_entity_),
        PROTOBUF_FIELD_OFFSET(::InterruptCurrentStatePbEvent, _impl_.actor_action_),
        PROTOBUF_FIELD_OFFSET(::InterruptCurrentStatePbEvent, _impl_.actor_state_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::InitializeActorComponentsEvent)},
        {9, -1, -1, sizeof(::InterruptCurrentStatePbEvent)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_InitializeActorComponentsEvent_default_instance_._instance,
    &::_InterruptCurrentStatePbEvent_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fevent_2factor_5fevent_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n#proto/logic/event/actor_event.proto\"6\n"
    "\036InitializeActorComponentsEvent\022\024\n\014actor"
    "_entity\030\001 \001(\004\"_\n\034InterruptCurrentStatePb"
    "Event\022\024\n\014actor_entity\030\001 \001(\004\022\024\n\014actor_act"
    "ion\030\002 \001(\r\022\023\n\013actor_state\030\003 \001(\rB\tZ\007pb/gam"
    "eb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto = {
    false,
    false,
    209,
    descriptor_table_protodef_proto_2flogic_2fevent_2factor_5fevent_2eproto,
    "proto/logic/event/actor_event.proto",
    &descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fevent_2factor_5fevent_2eproto::offsets,
    file_level_metadata_proto_2flogic_2fevent_2factor_5fevent_2eproto,
    file_level_enum_descriptors_proto_2flogic_2fevent_2factor_5fevent_2eproto,
    file_level_service_descriptors_proto_2flogic_2fevent_2factor_5fevent_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_getter() {
  return &descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto;
}
// ===================================================================

class InitializeActorComponentsEvent::_Internal {
 public:
};

InitializeActorComponentsEvent::InitializeActorComponentsEvent(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:InitializeActorComponentsEvent)
}
InitializeActorComponentsEvent::InitializeActorComponentsEvent(
    ::google::protobuf::Arena* arena, const InitializeActorComponentsEvent& from)
    : InitializeActorComponentsEvent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE InitializeActorComponentsEvent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void InitializeActorComponentsEvent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.actor_entity_ = {};
}
InitializeActorComponentsEvent::~InitializeActorComponentsEvent() {
  // @@protoc_insertion_point(destructor:InitializeActorComponentsEvent)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void InitializeActorComponentsEvent::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
InitializeActorComponentsEvent::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(InitializeActorComponentsEvent, _impl_._cached_size_),
              false,
          },
          &InitializeActorComponentsEvent::MergeImpl,
          &InitializeActorComponentsEvent::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void InitializeActorComponentsEvent::Clear() {
// @@protoc_insertion_point(message_clear_start:InitializeActorComponentsEvent)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.actor_entity_ = ::uint64_t{0u};
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* InitializeActorComponentsEvent::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> InitializeActorComponentsEvent::_table_ = {
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
    &_InitializeActorComponentsEvent_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::InitializeActorComponentsEvent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 actor_entity = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(InitializeActorComponentsEvent, _impl_.actor_entity_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(InitializeActorComponentsEvent, _impl_.actor_entity_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 actor_entity = 1;
    {PROTOBUF_FIELD_OFFSET(InitializeActorComponentsEvent, _impl_.actor_entity_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* InitializeActorComponentsEvent::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:InitializeActorComponentsEvent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 actor_entity = 1;
  if (this->_internal_actor_entity() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_actor_entity(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:InitializeActorComponentsEvent)
  return target;
}

::size_t InitializeActorComponentsEvent::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:InitializeActorComponentsEvent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 actor_entity = 1;
  if (this->_internal_actor_entity() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_actor_entity());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void InitializeActorComponentsEvent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<InitializeActorComponentsEvent*>(&to_msg);
  auto& from = static_cast<const InitializeActorComponentsEvent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:InitializeActorComponentsEvent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_actor_entity() != 0) {
    _this->_impl_.actor_entity_ = from._impl_.actor_entity_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void InitializeActorComponentsEvent::CopyFrom(const InitializeActorComponentsEvent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:InitializeActorComponentsEvent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool InitializeActorComponentsEvent::IsInitialized() const {
  return true;
}

void InitializeActorComponentsEvent::InternalSwap(InitializeActorComponentsEvent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.actor_entity_, other->_impl_.actor_entity_);
}

::google::protobuf::Metadata InitializeActorComponentsEvent::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fevent_2factor_5fevent_2eproto[0]);
}
// ===================================================================

class InterruptCurrentStatePbEvent::_Internal {
 public:
};

InterruptCurrentStatePbEvent::InterruptCurrentStatePbEvent(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:InterruptCurrentStatePbEvent)
}
InterruptCurrentStatePbEvent::InterruptCurrentStatePbEvent(
    ::google::protobuf::Arena* arena, const InterruptCurrentStatePbEvent& from)
    : InterruptCurrentStatePbEvent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE InterruptCurrentStatePbEvent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void InterruptCurrentStatePbEvent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, actor_entity_),
           0,
           offsetof(Impl_, actor_state_) -
               offsetof(Impl_, actor_entity_) +
               sizeof(Impl_::actor_state_));
}
InterruptCurrentStatePbEvent::~InterruptCurrentStatePbEvent() {
  // @@protoc_insertion_point(destructor:InterruptCurrentStatePbEvent)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void InterruptCurrentStatePbEvent::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
InterruptCurrentStatePbEvent::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_._cached_size_),
              false,
          },
          &InterruptCurrentStatePbEvent::MergeImpl,
          &InterruptCurrentStatePbEvent::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void InterruptCurrentStatePbEvent::Clear() {
// @@protoc_insertion_point(message_clear_start:InterruptCurrentStatePbEvent)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.actor_entity_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.actor_state_) -
      reinterpret_cast<char*>(&_impl_.actor_entity_)) + sizeof(_impl_.actor_state_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* InterruptCurrentStatePbEvent::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 0, 2> InterruptCurrentStatePbEvent::_table_ = {
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
    &_InterruptCurrentStatePbEvent_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::InterruptCurrentStatePbEvent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint64 actor_entity = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(InterruptCurrentStatePbEvent, _impl_.actor_entity_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_entity_)}},
    // uint32 actor_action = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(InterruptCurrentStatePbEvent, _impl_.actor_action_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_action_)}},
    // uint32 actor_state = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(InterruptCurrentStatePbEvent, _impl_.actor_state_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_state_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 actor_entity = 1;
    {PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_entity_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 actor_action = 2;
    {PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_action_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 actor_state = 3;
    {PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_state_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* InterruptCurrentStatePbEvent::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:InterruptCurrentStatePbEvent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 actor_entity = 1;
  if (this->_internal_actor_entity() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_actor_entity(), target);
  }

  // uint32 actor_action = 2;
  if (this->_internal_actor_action() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_actor_action(), target);
  }

  // uint32 actor_state = 3;
  if (this->_internal_actor_state() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        3, this->_internal_actor_state(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:InterruptCurrentStatePbEvent)
  return target;
}

::size_t InterruptCurrentStatePbEvent::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:InterruptCurrentStatePbEvent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 actor_entity = 1;
  if (this->_internal_actor_entity() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_actor_entity());
  }

  // uint32 actor_action = 2;
  if (this->_internal_actor_action() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_actor_action());
  }

  // uint32 actor_state = 3;
  if (this->_internal_actor_state() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_actor_state());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void InterruptCurrentStatePbEvent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<InterruptCurrentStatePbEvent*>(&to_msg);
  auto& from = static_cast<const InterruptCurrentStatePbEvent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:InterruptCurrentStatePbEvent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_actor_entity() != 0) {
    _this->_impl_.actor_entity_ = from._impl_.actor_entity_;
  }
  if (from._internal_actor_action() != 0) {
    _this->_impl_.actor_action_ = from._impl_.actor_action_;
  }
  if (from._internal_actor_state() != 0) {
    _this->_impl_.actor_state_ = from._impl_.actor_state_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void InterruptCurrentStatePbEvent::CopyFrom(const InterruptCurrentStatePbEvent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:InterruptCurrentStatePbEvent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool InterruptCurrentStatePbEvent::IsInitialized() const {
  return true;
}

void InterruptCurrentStatePbEvent::InternalSwap(InterruptCurrentStatePbEvent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_state_)
      + sizeof(InterruptCurrentStatePbEvent::_impl_.actor_state_)
      - PROTOBUF_FIELD_OFFSET(InterruptCurrentStatePbEvent, _impl_.actor_entity_)>(
          reinterpret_cast<char*>(&_impl_.actor_entity_),
          reinterpret_cast<char*>(&other->_impl_.actor_entity_));
}

::google::protobuf::Metadata InterruptCurrentStatePbEvent::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fevent_2factor_5fevent_2eproto[1]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fevent_2factor_5fevent_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
