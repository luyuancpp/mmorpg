// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/player_network_comp.proto
// Protobuf C++ Version: 5.29.0

#include "proto/logic/component/player_network_comp.pb.h"

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
              template <typename>
PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::MapEntry(_class_data_.base()){}
#else   // PROTOBUF_CUSTOM_VTABLE
    : PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::MapEntry() {
}
#endif  // PROTOBUF_CUSTOM_VTABLE
struct PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal() {}
  union {
    PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUseDefaultTypeInternal _PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse_default_instance_;

inline constexpr PlayerSessionSnapshotPBComp::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : node_id_{},
        login_token_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        player_id_{::uint64_t{0u}},
        gate_session_id_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBComp::PlayerSessionSnapshotPBComp(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct PlayerSessionSnapshotPBCompDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerSessionSnapshotPBCompDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerSessionSnapshotPBCompDefaultTypeInternal() {}
  union {
    PlayerSessionSnapshotPBComp _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerSessionSnapshotPBCompDefaultTypeInternal _PlayerSessionSnapshotPBComp_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.key_),
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp, _impl_.player_id_),
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp, _impl_.gate_session_id_),
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp, _impl_.node_id_),
        PROTOBUF_FIELD_OFFSET(::PlayerSessionSnapshotPBComp, _impl_.login_token_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, 10, -1, sizeof(::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse)},
        {12, -1, -1, sizeof(::PlayerSessionSnapshotPBComp)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse_default_instance_._instance,
    &::_PlayerSessionSnapshotPBComp_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n/proto/logic/component/player_network_c"
    "omp.proto\"\310\001\n\033PlayerSessionSnapshotPBCom"
    "p\022\021\n\tplayer_id\030\001 \001(\004\022\027\n\017gate_session_id\030"
    "\002 \001(\004\0229\n\007node_id\030\003 \003(\0132(.PlayerSessionSn"
    "apshotPBComp.NodeIdEntry\022\023\n\013login_token\030"
    "\004 \001(\t\032-\n\013NodeIdEntry\022\013\n\003key\030\001 \001(\r\022\r\n\005val"
    "ue\030\002 \001(\r:\0028\001B\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto = {
    false,
    false,
    271,
    descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    "proto/logic/component/player_network_comp.proto",
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto::offsets,
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
};
// ===================================================================

#if defined(PROTOBUF_CUSTOM_VTABLE)
              PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse() : SuperType(_class_data_.base()) {}
              PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(::google::protobuf::Arena* arena)
                  : SuperType(arena, _class_data_.base()) {}
#else   // PROTOBUF_CUSTOM_VTABLE
              PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse() : SuperType() {}
              PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
#endif  // PROTOBUF_CUSTOM_VTABLE
              inline void* PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::PlacementNew_(const void*, void* mem,
                                                      ::google::protobuf::Arena* arena) {
                return ::new (mem) PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse(arena);
              }
              constexpr auto PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::InternalNewImpl_() {
                return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse),
                                                          alignof(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse));
              }
              PROTOBUF_CONSTINIT
              PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
              const ::google::protobuf::internal::ClassDataFull PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::_class_data_ = {
                  ::google::protobuf::internal::ClassData{
                      &_PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse_default_instance_._instance,
                      &_table_.header,
                      nullptr,  // OnDemandRegisterArenaDtor
                      nullptr,  // IsInitialized
                      &PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::MergeImpl,
                      ::google::protobuf::Message::GetNewImpl<PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse>(),
              #if defined(PROTOBUF_CUSTOM_VTABLE)
                      &PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::SharedDtor,
                      static_cast<void (::google::protobuf::MessageLite::*)()>(
                          &PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::ClearImpl),
                          ::google::protobuf::Message::ByteSizeLongImpl, ::google::protobuf::Message::_InternalSerializeImpl
                          ,
              #endif  // PROTOBUF_CUSTOM_VTABLE
                      PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_._cached_size_),
                      false,
                  },
                  &PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::kDescriptorMethods,
                  &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
                  nullptr,  // tracker
              };
              const ::google::protobuf::internal::ClassData* PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::GetClassData() const {
                ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
                ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
                return _class_data_.base();
              }
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::DiscardEverythingFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 value = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.value_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.value_)}},
    // uint32 key = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.key_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.key_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 key = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.key_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 value = 2;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp_NodeIdEntry_DoNotUse, _impl_.value_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

// ===================================================================

class PlayerSessionSnapshotPBComp::_Internal {
 public:
};

PlayerSessionSnapshotPBComp::PlayerSessionSnapshotPBComp(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:PlayerSessionSnapshotPBComp)
}
inline PROTOBUF_NDEBUG_INLINE PlayerSessionSnapshotPBComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::PlayerSessionSnapshotPBComp& from_msg)
      : node_id_{visibility, arena, from.node_id_},
        login_token_(arena, from.login_token_),
        _cached_size_{0} {}

PlayerSessionSnapshotPBComp::PlayerSessionSnapshotPBComp(
    ::google::protobuf::Arena* arena,
    const PlayerSessionSnapshotPBComp& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  PlayerSessionSnapshotPBComp* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, player_id_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, player_id_),
           offsetof(Impl_, gate_session_id_) -
               offsetof(Impl_, player_id_) +
               sizeof(Impl_::gate_session_id_));

  // @@protoc_insertion_point(copy_constructor:PlayerSessionSnapshotPBComp)
}
inline PROTOBUF_NDEBUG_INLINE PlayerSessionSnapshotPBComp::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : node_id_{visibility, arena},
        login_token_(arena),
        _cached_size_{0} {}

inline void PlayerSessionSnapshotPBComp::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, player_id_),
           0,
           offsetof(Impl_, gate_session_id_) -
               offsetof(Impl_, player_id_) +
               sizeof(Impl_::gate_session_id_));
}
PlayerSessionSnapshotPBComp::~PlayerSessionSnapshotPBComp() {
  // @@protoc_insertion_point(destructor:PlayerSessionSnapshotPBComp)
  SharedDtor(*this);
}
inline void PlayerSessionSnapshotPBComp::SharedDtor(MessageLite& self) {
  PlayerSessionSnapshotPBComp& this_ = static_cast<PlayerSessionSnapshotPBComp&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.login_token_.Destroy();
  this_._impl_.~Impl_();
}

inline void* PlayerSessionSnapshotPBComp::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) PlayerSessionSnapshotPBComp(arena);
}
constexpr auto PlayerSessionSnapshotPBComp::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.node_id_) +
          decltype(PlayerSessionSnapshotPBComp::_impl_.node_id_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
      PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.node_id_) +
          decltype(PlayerSessionSnapshotPBComp::_impl_.node_id_)::
              InternalGetArenaOffsetAlt(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::CopyInit(
        sizeof(PlayerSessionSnapshotPBComp), alignof(PlayerSessionSnapshotPBComp), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&PlayerSessionSnapshotPBComp::PlacementNew_,
                                 sizeof(PlayerSessionSnapshotPBComp),
                                 alignof(PlayerSessionSnapshotPBComp));
  }
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull PlayerSessionSnapshotPBComp::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_PlayerSessionSnapshotPBComp_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &PlayerSessionSnapshotPBComp::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<PlayerSessionSnapshotPBComp>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &PlayerSessionSnapshotPBComp::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<PlayerSessionSnapshotPBComp>(), &PlayerSessionSnapshotPBComp::ByteSizeLong,
            &PlayerSessionSnapshotPBComp::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_._cached_size_),
        false,
    },
    &PlayerSessionSnapshotPBComp::kDescriptorMethods,
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* PlayerSessionSnapshotPBComp::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 1, 47, 2> PlayerSessionSnapshotPBComp::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerSessionSnapshotPBComp>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string login_token = 4;
    {::_pbi::TcParser::FastUS1,
     {34, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.login_token_)}},
    // uint64 player_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(PlayerSessionSnapshotPBComp, _impl_.player_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.player_id_)}},
    // uint64 gate_session_id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(PlayerSessionSnapshotPBComp, _impl_.gate_session_id_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.gate_session_id_)}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 player_id = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.player_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint64 gate_session_id = 2;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.gate_session_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // map<uint32, uint32> node_id = 3;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.node_id_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // string login_token = 4;
    {PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.login_token_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(PlayerSessionSnapshotPBComp()._impl_.node_id_)>(
        0, 0, 0, 13,
        13)},
  }}, {{
    "\33\0\0\0\13\0\0\0"
    "PlayerSessionSnapshotPBComp"
    "login_token"
  }},
};

PROTOBUF_NOINLINE void PlayerSessionSnapshotPBComp::Clear() {
// @@protoc_insertion_point(message_clear_start:PlayerSessionSnapshotPBComp)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.node_id_.Clear();
  _impl_.login_token_.ClearToEmpty();
  ::memset(&_impl_.player_id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.gate_session_id_) -
      reinterpret_cast<char*>(&_impl_.player_id_)) + sizeof(_impl_.gate_session_id_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* PlayerSessionSnapshotPBComp::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const PlayerSessionSnapshotPBComp& this_ = static_cast<const PlayerSessionSnapshotPBComp&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* PlayerSessionSnapshotPBComp::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const PlayerSessionSnapshotPBComp& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:PlayerSessionSnapshotPBComp)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint64 player_id = 1;
          if (this_._internal_player_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
                1, this_._internal_player_id(), target);
          }

          // uint64 gate_session_id = 2;
          if (this_._internal_gate_session_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
                2, this_._internal_gate_session_id(), target);
          }

          // map<uint32, uint32> node_id = 3;
          if (!this_._internal_node_id().empty()) {
            using MapType = ::google::protobuf::Map<::uint32_t, ::uint32_t>;
            using WireHelper = _pbi::MapEntryFuncs<::uint32_t, ::uint32_t,
                                           _pbi::WireFormatLite::TYPE_UINT32,
                                           _pbi::WireFormatLite::TYPE_UINT32>;
            const auto& field = this_._internal_node_id();

            if (stream->IsSerializationDeterministic() && field.size() > 1) {
              for (const auto& entry : ::google::protobuf::internal::MapSorterFlat<MapType>(field)) {
                target = WireHelper::InternalSerialize(
                    3, entry.first, entry.second, target, stream);
              }
            } else {
              for (const auto& entry : field) {
                target = WireHelper::InternalSerialize(
                    3, entry.first, entry.second, target, stream);
              }
            }
          }

          // string login_token = 4;
          if (!this_._internal_login_token().empty()) {
            const std::string& _s = this_._internal_login_token();
            ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "PlayerSessionSnapshotPBComp.login_token");
            target = stream->WriteStringMaybeAliased(4, _s, target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:PlayerSessionSnapshotPBComp)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t PlayerSessionSnapshotPBComp::ByteSizeLong(const MessageLite& base) {
          const PlayerSessionSnapshotPBComp& this_ = static_cast<const PlayerSessionSnapshotPBComp&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t PlayerSessionSnapshotPBComp::ByteSizeLong() const {
          const PlayerSessionSnapshotPBComp& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:PlayerSessionSnapshotPBComp)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // map<uint32, uint32> node_id = 3;
            {
              total_size +=
                  1 * ::google::protobuf::internal::FromIntSize(this_._internal_node_id_size());
              for (const auto& entry : this_._internal_node_id()) {
                total_size += _pbi::MapEntryFuncs<::uint32_t, ::uint32_t,
                                               _pbi::WireFormatLite::TYPE_UINT32,
                                               _pbi::WireFormatLite::TYPE_UINT32>::ByteSizeLong(entry.first, entry.second);
              }
            }
          }
           {
            // string login_token = 4;
            if (!this_._internal_login_token().empty()) {
              total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                              this_._internal_login_token());
            }
            // uint64 player_id = 1;
            if (this_._internal_player_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
                  this_._internal_player_id());
            }
            // uint64 gate_session_id = 2;
            if (this_._internal_gate_session_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
                  this_._internal_gate_session_id());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void PlayerSessionSnapshotPBComp::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PlayerSessionSnapshotPBComp*>(&to_msg);
  auto& from = static_cast<const PlayerSessionSnapshotPBComp&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:PlayerSessionSnapshotPBComp)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.node_id_.MergeFrom(from._impl_.node_id_);
  if (!from._internal_login_token().empty()) {
    _this->_internal_set_login_token(from._internal_login_token());
  }
  if (from._internal_player_id() != 0) {
    _this->_impl_.player_id_ = from._impl_.player_id_;
  }
  if (from._internal_gate_session_id() != 0) {
    _this->_impl_.gate_session_id_ = from._impl_.gate_session_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PlayerSessionSnapshotPBComp::CopyFrom(const PlayerSessionSnapshotPBComp& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:PlayerSessionSnapshotPBComp)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void PlayerSessionSnapshotPBComp::InternalSwap(PlayerSessionSnapshotPBComp* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.node_id_.InternalSwap(&other->_impl_.node_id_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.login_token_, &other->_impl_.login_token_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.gate_session_id_)
      + sizeof(PlayerSessionSnapshotPBComp::_impl_.gate_session_id_)
      - PROTOBUF_FIELD_OFFSET(PlayerSessionSnapshotPBComp, _impl_.player_id_)>(
          reinterpret_cast<char*>(&_impl_.player_id_),
          reinterpret_cast<char*>(&other->_impl_.player_id_));
}

::google::protobuf::Metadata PlayerSessionSnapshotPBComp::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fnetwork_5fcomp_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
