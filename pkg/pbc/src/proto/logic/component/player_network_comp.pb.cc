// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/component/player_network_comp.proto
// Protobuf C++ Version: 5.26.1

#include "proto/logic/component/player_network_comp.pb.h"

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

inline constexpr PlayerSessionPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : player_id_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR PlayerSessionPBComponent::PlayerSessionPBComponent(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct PlayerSessionPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerSessionPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerSessionPBComponentDefaultTypeInternal() {}
  union {
    PlayerSessionPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerSessionPBComponentDefaultTypeInternal _PlayerSessionPBComponent_default_instance_;

inline constexpr PlayerNodeInfoPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : gate_session_id_{::uint64_t{0u}},
        centre_node_id_{0u},
        scene_node_id_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR PlayerNodeInfoPBComponent::PlayerNodeInfoPBComponent(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct PlayerNodeInfoPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerNodeInfoPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerNodeInfoPBComponentDefaultTypeInternal() {}
  union {
    PlayerNodeInfoPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerNodeInfoPBComponentDefaultTypeInternal _PlayerNodeInfoPBComponent_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::PlayerNodeInfoPBComponent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerNodeInfoPBComponent, _impl_.gate_session_id_),
        PROTOBUF_FIELD_OFFSET(::PlayerNodeInfoPBComponent, _impl_.centre_node_id_),
        PROTOBUF_FIELD_OFFSET(::PlayerNodeInfoPBComponent, _impl_.scene_node_id_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::PlayerSessionPBComponent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerSessionPBComponent, _impl_.player_id_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::PlayerNodeInfoPBComponent)},
        {11, -1, -1, sizeof(::PlayerSessionPBComponent)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_PlayerNodeInfoPBComponent_default_instance_._instance,
    &::_PlayerSessionPBComponent_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n/proto/logic/component/player_network_c"
    "omp.proto\"c\n\031PlayerNodeInfoPBComponent\022\027"
    "\n\017gate_session_id\030\001 \001(\004\022\026\n\016centre_node_i"
    "d\030\002 \001(\r\022\025\n\rscene_node_id\030\003 \001(\r\"-\n\030Player"
    "SessionPBComponent\022\021\n\tplayer_id\030\001 \001(\004B\tZ"
    "\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = {
    false,
    false,
    216,
    descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    "proto/logic/component/player_network_comp.proto",
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto::offsets,
    file_level_metadata_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_getter() {
  return &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto;
}
// ===================================================================

class PlayerNodeInfoPBComponent::_Internal {
 public:
};

PlayerNodeInfoPBComponent::PlayerNodeInfoPBComponent(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:PlayerNodeInfoPBComponent)
}
PlayerNodeInfoPBComponent::PlayerNodeInfoPBComponent(
    ::google::protobuf::Arena* arena, const PlayerNodeInfoPBComponent& from)
    : PlayerNodeInfoPBComponent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE PlayerNodeInfoPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void PlayerNodeInfoPBComponent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, gate_session_id_),
           0,
           offsetof(Impl_, scene_node_id_) -
               offsetof(Impl_, gate_session_id_) +
               sizeof(Impl_::scene_node_id_));
}
PlayerNodeInfoPBComponent::~PlayerNodeInfoPBComponent() {
  // @@protoc_insertion_point(destructor:PlayerNodeInfoPBComponent)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void PlayerNodeInfoPBComponent::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
PlayerNodeInfoPBComponent::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_._cached_size_),
              false,
          },
          &PlayerNodeInfoPBComponent::MergeImpl,
          &PlayerNodeInfoPBComponent::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void PlayerNodeInfoPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:PlayerNodeInfoPBComponent)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.gate_session_id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.scene_node_id_) -
      reinterpret_cast<char*>(&_impl_.gate_session_id_)) + sizeof(_impl_.scene_node_id_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* PlayerNodeInfoPBComponent::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 0, 2> PlayerNodeInfoPBComponent::_table_ = {
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
    &_PlayerNodeInfoPBComponent_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerNodeInfoPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // uint64 gate_session_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(PlayerNodeInfoPBComponent, _impl_.gate_session_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.gate_session_id_)}},
    // uint32 centre_node_id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerNodeInfoPBComponent, _impl_.centre_node_id_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.centre_node_id_)}},
    // uint32 scene_node_id = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerNodeInfoPBComponent, _impl_.scene_node_id_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.scene_node_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 gate_session_id = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.gate_session_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 centre_node_id = 2;
    {PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.centre_node_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 scene_node_id = 3;
    {PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.scene_node_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* PlayerNodeInfoPBComponent::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:PlayerNodeInfoPBComponent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 gate_session_id = 1;
  if (this->_internal_gate_session_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_gate_session_id(), target);
  }

  // uint32 centre_node_id = 2;
  if (this->_internal_centre_node_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_centre_node_id(), target);
  }

  // uint32 scene_node_id = 3;
  if (this->_internal_scene_node_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        3, this->_internal_scene_node_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:PlayerNodeInfoPBComponent)
  return target;
}

::size_t PlayerNodeInfoPBComponent::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:PlayerNodeInfoPBComponent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 gate_session_id = 1;
  if (this->_internal_gate_session_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_gate_session_id());
  }

  // uint32 centre_node_id = 2;
  if (this->_internal_centre_node_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_centre_node_id());
  }

  // uint32 scene_node_id = 3;
  if (this->_internal_scene_node_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_scene_node_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void PlayerNodeInfoPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PlayerNodeInfoPBComponent*>(&to_msg);
  auto& from = static_cast<const PlayerNodeInfoPBComponent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:PlayerNodeInfoPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_gate_session_id() != 0) {
    _this->_impl_.gate_session_id_ = from._impl_.gate_session_id_;
  }
  if (from._internal_centre_node_id() != 0) {
    _this->_impl_.centre_node_id_ = from._impl_.centre_node_id_;
  }
  if (from._internal_scene_node_id() != 0) {
    _this->_impl_.scene_node_id_ = from._impl_.scene_node_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PlayerNodeInfoPBComponent::CopyFrom(const PlayerNodeInfoPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:PlayerNodeInfoPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool PlayerNodeInfoPBComponent::IsInitialized() const {
  return true;
}

void PlayerNodeInfoPBComponent::InternalSwap(PlayerNodeInfoPBComponent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.scene_node_id_)
      + sizeof(PlayerNodeInfoPBComponent::_impl_.scene_node_id_)
      - PROTOBUF_FIELD_OFFSET(PlayerNodeInfoPBComponent, _impl_.gate_session_id_)>(
          reinterpret_cast<char*>(&_impl_.gate_session_id_),
          reinterpret_cast<char*>(&other->_impl_.gate_session_id_));
}

::google::protobuf::Metadata PlayerNodeInfoPBComponent::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto[0]);
}
// ===================================================================

class PlayerSessionPBComponent::_Internal {
 public:
};

PlayerSessionPBComponent::PlayerSessionPBComponent(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:PlayerSessionPBComponent)
}
PlayerSessionPBComponent::PlayerSessionPBComponent(
    ::google::protobuf::Arena* arena, const PlayerSessionPBComponent& from)
    : PlayerSessionPBComponent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE PlayerSessionPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void PlayerSessionPBComponent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.player_id_ = {};
}
PlayerSessionPBComponent::~PlayerSessionPBComponent() {
  // @@protoc_insertion_point(destructor:PlayerSessionPBComponent)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void PlayerSessionPBComponent::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
PlayerSessionPBComponent::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(PlayerSessionPBComponent, _impl_._cached_size_),
              false,
          },
          &PlayerSessionPBComponent::MergeImpl,
          &PlayerSessionPBComponent::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void PlayerSessionPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:PlayerSessionPBComponent)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.player_id_ = ::uint64_t{0u};
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* PlayerSessionPBComponent::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> PlayerSessionPBComponent::_table_ = {
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
    &_PlayerSessionPBComponent_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerSessionPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 player_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(PlayerSessionPBComponent, _impl_.player_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionPBComponent, _impl_.player_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 player_id = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionPBComponent, _impl_.player_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* PlayerSessionPBComponent::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:PlayerSessionPBComponent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 player_id = 1;
  if (this->_internal_player_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_player_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:PlayerSessionPBComponent)
  return target;
}

::size_t PlayerSessionPBComponent::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:PlayerSessionPBComponent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 player_id = 1;
  if (this->_internal_player_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_player_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void PlayerSessionPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PlayerSessionPBComponent*>(&to_msg);
  auto& from = static_cast<const PlayerSessionPBComponent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:PlayerSessionPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_player_id() != 0) {
    _this->_impl_.player_id_ = from._impl_.player_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PlayerSessionPBComponent::CopyFrom(const PlayerSessionPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:PlayerSessionPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool PlayerSessionPBComponent::IsInitialized() const {
  return true;
}

void PlayerSessionPBComponent::InternalSwap(PlayerSessionPBComponent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.player_id_, other->_impl_.player_id_);
}

::google::protobuf::Metadata PlayerSessionPBComponent::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto[1]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
