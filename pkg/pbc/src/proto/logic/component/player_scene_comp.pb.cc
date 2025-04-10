// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/player_scene_comp.proto
// Protobuf C++ Version: 5.29.0

#include "proto/logic/component/player_scene_comp.pb.h"

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

inline constexpr PlayerSceneInfoPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : scene_confid_{0u},
        guid_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR PlayerSceneInfoPBComponent::PlayerSceneInfoPBComponent(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct PlayerSceneInfoPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerSceneInfoPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerSceneInfoPBComponentDefaultTypeInternal() {}
  union {
    PlayerSceneInfoPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerSceneInfoPBComponentDefaultTypeInternal _PlayerSceneInfoPBComponent_default_instance_;

inline constexpr PlayerSceneContextPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        scene_info_{nullptr},
        scene_info_last_time_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR PlayerSceneContextPBComponent::PlayerSceneContextPBComponent(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct PlayerSceneContextPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PlayerSceneContextPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PlayerSceneContextPBComponentDefaultTypeInternal() {}
  union {
    PlayerSceneContextPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PlayerSceneContextPBComponentDefaultTypeInternal _PlayerSceneContextPBComponent_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::PlayerSceneInfoPBComponent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerSceneInfoPBComponent, _impl_.scene_confid_),
        PROTOBUF_FIELD_OFFSET(::PlayerSceneInfoPBComponent, _impl_.guid_),
        PROTOBUF_FIELD_OFFSET(::PlayerSceneContextPBComponent, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::PlayerSceneContextPBComponent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::PlayerSceneContextPBComponent, _impl_.scene_info_),
        PROTOBUF_FIELD_OFFSET(::PlayerSceneContextPBComponent, _impl_.scene_info_last_time_),
        0,
        1,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::PlayerSceneInfoPBComponent)},
        {10, 20, -1, sizeof(::PlayerSceneContextPBComponent)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_PlayerSceneInfoPBComponent_default_instance_._instance,
    &::_PlayerSceneContextPBComponent_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n-proto/logic/component/player_scene_com"
    "p.proto\"@\n\032PlayerSceneInfoPBComponent\022\024\n"
    "\014scene_confid\030\001 \001(\r\022\014\n\004guid\030\002 \001(\r\"\213\001\n\035Pl"
    "ayerSceneContextPBComponent\022/\n\nscene_inf"
    "o\030\001 \001(\0132\033.PlayerSceneInfoPBComponent\0229\n\024"
    "scene_info_last_time\030\002 \001(\0132\033.PlayerScene"
    "InfoPBComponentB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto = {
    false,
    false,
    274,
    descriptor_table_protodef_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto,
    "proto/logic/component/player_scene_comp.proto",
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto::offsets,
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto,
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto,
};
// ===================================================================

class PlayerSceneInfoPBComponent::_Internal {
 public:
};

PlayerSceneInfoPBComponent::PlayerSceneInfoPBComponent(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:PlayerSceneInfoPBComponent)
}
PlayerSceneInfoPBComponent::PlayerSceneInfoPBComponent(
    ::google::protobuf::Arena* arena, const PlayerSceneInfoPBComponent& from)
    : PlayerSceneInfoPBComponent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE PlayerSceneInfoPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void PlayerSceneInfoPBComponent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, scene_confid_),
           0,
           offsetof(Impl_, guid_) -
               offsetof(Impl_, scene_confid_) +
               sizeof(Impl_::guid_));
}
PlayerSceneInfoPBComponent::~PlayerSceneInfoPBComponent() {
  // @@protoc_insertion_point(destructor:PlayerSceneInfoPBComponent)
  SharedDtor(*this);
}
inline void PlayerSceneInfoPBComponent::SharedDtor(MessageLite& self) {
  PlayerSceneInfoPBComponent& this_ = static_cast<PlayerSceneInfoPBComponent&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* PlayerSceneInfoPBComponent::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) PlayerSceneInfoPBComponent(arena);
}
constexpr auto PlayerSceneInfoPBComponent::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(PlayerSceneInfoPBComponent),
                                            alignof(PlayerSceneInfoPBComponent));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull PlayerSceneInfoPBComponent::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_PlayerSceneInfoPBComponent_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &PlayerSceneInfoPBComponent::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<PlayerSceneInfoPBComponent>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &PlayerSceneInfoPBComponent::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<PlayerSceneInfoPBComponent>(), &PlayerSceneInfoPBComponent::ByteSizeLong,
            &PlayerSceneInfoPBComponent::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_._cached_size_),
        false,
    },
    &PlayerSceneInfoPBComponent::kDescriptorMethods,
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* PlayerSceneInfoPBComponent::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 0, 2> PlayerSceneInfoPBComponent::_table_ = {
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
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerSceneInfoPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 guid = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerSceneInfoPBComponent, _impl_.guid_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.guid_)}},
    // uint32 scene_confid = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(PlayerSceneInfoPBComponent, _impl_.scene_confid_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.scene_confid_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 scene_confid = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.scene_confid_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 guid = 2;
    {PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.guid_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void PlayerSceneInfoPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:PlayerSceneInfoPBComponent)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.scene_confid_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.guid_) -
      reinterpret_cast<char*>(&_impl_.scene_confid_)) + sizeof(_impl_.guid_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* PlayerSceneInfoPBComponent::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const PlayerSceneInfoPBComponent& this_ = static_cast<const PlayerSceneInfoPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* PlayerSceneInfoPBComponent::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const PlayerSceneInfoPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:PlayerSceneInfoPBComponent)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint32 scene_confid = 1;
          if (this_._internal_scene_confid() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                1, this_._internal_scene_confid(), target);
          }

          // uint32 guid = 2;
          if (this_._internal_guid() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
                2, this_._internal_guid(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:PlayerSceneInfoPBComponent)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t PlayerSceneInfoPBComponent::ByteSizeLong(const MessageLite& base) {
          const PlayerSceneInfoPBComponent& this_ = static_cast<const PlayerSceneInfoPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t PlayerSceneInfoPBComponent::ByteSizeLong() const {
          const PlayerSceneInfoPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:PlayerSceneInfoPBComponent)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
           {
            // uint32 scene_confid = 1;
            if (this_._internal_scene_confid() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_scene_confid());
            }
            // uint32 guid = 2;
            if (this_._internal_guid() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
                  this_._internal_guid());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void PlayerSceneInfoPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PlayerSceneInfoPBComponent*>(&to_msg);
  auto& from = static_cast<const PlayerSceneInfoPBComponent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:PlayerSceneInfoPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_scene_confid() != 0) {
    _this->_impl_.scene_confid_ = from._impl_.scene_confid_;
  }
  if (from._internal_guid() != 0) {
    _this->_impl_.guid_ = from._impl_.guid_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PlayerSceneInfoPBComponent::CopyFrom(const PlayerSceneInfoPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:PlayerSceneInfoPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void PlayerSceneInfoPBComponent::InternalSwap(PlayerSceneInfoPBComponent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.guid_)
      + sizeof(PlayerSceneInfoPBComponent::_impl_.guid_)
      - PROTOBUF_FIELD_OFFSET(PlayerSceneInfoPBComponent, _impl_.scene_confid_)>(
          reinterpret_cast<char*>(&_impl_.scene_confid_),
          reinterpret_cast<char*>(&other->_impl_.scene_confid_));
}

::google::protobuf::Metadata PlayerSceneInfoPBComponent::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class PlayerSceneContextPBComponent::_Internal {
 public:
  using HasBits =
      decltype(std::declval<PlayerSceneContextPBComponent>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_._has_bits_);
};

PlayerSceneContextPBComponent::PlayerSceneContextPBComponent(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:PlayerSceneContextPBComponent)
}
inline PROTOBUF_NDEBUG_INLINE PlayerSceneContextPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::PlayerSceneContextPBComponent& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

PlayerSceneContextPBComponent::PlayerSceneContextPBComponent(
    ::google::protobuf::Arena* arena,
    const PlayerSceneContextPBComponent& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  PlayerSceneContextPBComponent* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.scene_info_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::PlayerSceneInfoPBComponent>(
                              arena, *from._impl_.scene_info_)
                        : nullptr;
  _impl_.scene_info_last_time_ = (cached_has_bits & 0x00000002u) ? ::google::protobuf::Message::CopyConstruct<::PlayerSceneInfoPBComponent>(
                              arena, *from._impl_.scene_info_last_time_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:PlayerSceneContextPBComponent)
}
inline PROTOBUF_NDEBUG_INLINE PlayerSceneContextPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void PlayerSceneContextPBComponent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, scene_info_),
           0,
           offsetof(Impl_, scene_info_last_time_) -
               offsetof(Impl_, scene_info_) +
               sizeof(Impl_::scene_info_last_time_));
}
PlayerSceneContextPBComponent::~PlayerSceneContextPBComponent() {
  // @@protoc_insertion_point(destructor:PlayerSceneContextPBComponent)
  SharedDtor(*this);
}
inline void PlayerSceneContextPBComponent::SharedDtor(MessageLite& self) {
  PlayerSceneContextPBComponent& this_ = static_cast<PlayerSceneContextPBComponent&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  delete this_._impl_.scene_info_;
  delete this_._impl_.scene_info_last_time_;
  this_._impl_.~Impl_();
}

inline void* PlayerSceneContextPBComponent::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) PlayerSceneContextPBComponent(arena);
}
constexpr auto PlayerSceneContextPBComponent::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(PlayerSceneContextPBComponent),
                                            alignof(PlayerSceneContextPBComponent));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull PlayerSceneContextPBComponent::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_PlayerSceneContextPBComponent_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &PlayerSceneContextPBComponent::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<PlayerSceneContextPBComponent>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &PlayerSceneContextPBComponent::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<PlayerSceneContextPBComponent>(), &PlayerSceneContextPBComponent::ByteSizeLong,
            &PlayerSceneContextPBComponent::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_._cached_size_),
        false,
    },
    &PlayerSceneContextPBComponent::kDescriptorMethods,
    &descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* PlayerSceneContextPBComponent::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 2, 0, 2> PlayerSceneContextPBComponent::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::PlayerSceneContextPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .PlayerSceneInfoPBComponent scene_info_last_time = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 1, 1, PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_last_time_)}},
    // .PlayerSceneInfoPBComponent scene_info = 1;
    {::_pbi::TcParser::FastMtS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .PlayerSceneInfoPBComponent scene_info = 1;
    {PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .PlayerSceneInfoPBComponent scene_info_last_time = 2;
    {PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_last_time_), _Internal::kHasBitsOffset + 1, 1,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::PlayerSceneInfoPBComponent>()},
    {::_pbi::TcParser::GetTable<::PlayerSceneInfoPBComponent>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void PlayerSceneContextPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:PlayerSceneContextPBComponent)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(_impl_.scene_info_ != nullptr);
      _impl_.scene_info_->Clear();
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(_impl_.scene_info_last_time_ != nullptr);
      _impl_.scene_info_last_time_->Clear();
    }
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* PlayerSceneContextPBComponent::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const PlayerSceneContextPBComponent& this_ = static_cast<const PlayerSceneContextPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* PlayerSceneContextPBComponent::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const PlayerSceneContextPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:PlayerSceneContextPBComponent)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          cached_has_bits = this_._impl_._has_bits_[0];
          // .PlayerSceneInfoPBComponent scene_info = 1;
          if (cached_has_bits & 0x00000001u) {
            target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
                1, *this_._impl_.scene_info_, this_._impl_.scene_info_->GetCachedSize(), target,
                stream);
          }

          // .PlayerSceneInfoPBComponent scene_info_last_time = 2;
          if (cached_has_bits & 0x00000002u) {
            target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
                2, *this_._impl_.scene_info_last_time_, this_._impl_.scene_info_last_time_->GetCachedSize(), target,
                stream);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:PlayerSceneContextPBComponent)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t PlayerSceneContextPBComponent::ByteSizeLong(const MessageLite& base) {
          const PlayerSceneContextPBComponent& this_ = static_cast<const PlayerSceneContextPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t PlayerSceneContextPBComponent::ByteSizeLong() const {
          const PlayerSceneContextPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:PlayerSceneContextPBComponent)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

          ::_pbi::Prefetch5LinesFrom7Lines(&this_);
          cached_has_bits = this_._impl_._has_bits_[0];
          if (cached_has_bits & 0x00000003u) {
            // .PlayerSceneInfoPBComponent scene_info = 1;
            if (cached_has_bits & 0x00000001u) {
              total_size += 1 +
                            ::google::protobuf::internal::WireFormatLite::MessageSize(*this_._impl_.scene_info_);
            }
            // .PlayerSceneInfoPBComponent scene_info_last_time = 2;
            if (cached_has_bits & 0x00000002u) {
              total_size += 1 +
                            ::google::protobuf::internal::WireFormatLite::MessageSize(*this_._impl_.scene_info_last_time_);
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void PlayerSceneContextPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PlayerSceneContextPBComponent*>(&to_msg);
  auto& from = static_cast<const PlayerSceneContextPBComponent&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:PlayerSceneContextPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(from._impl_.scene_info_ != nullptr);
      if (_this->_impl_.scene_info_ == nullptr) {
        _this->_impl_.scene_info_ =
            ::google::protobuf::Message::CopyConstruct<::PlayerSceneInfoPBComponent>(arena, *from._impl_.scene_info_);
      } else {
        _this->_impl_.scene_info_->MergeFrom(*from._impl_.scene_info_);
      }
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(from._impl_.scene_info_last_time_ != nullptr);
      if (_this->_impl_.scene_info_last_time_ == nullptr) {
        _this->_impl_.scene_info_last_time_ =
            ::google::protobuf::Message::CopyConstruct<::PlayerSceneInfoPBComponent>(arena, *from._impl_.scene_info_last_time_);
      } else {
        _this->_impl_.scene_info_last_time_->MergeFrom(*from._impl_.scene_info_last_time_);
      }
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PlayerSceneContextPBComponent::CopyFrom(const PlayerSceneContextPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:PlayerSceneContextPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void PlayerSceneContextPBComponent::InternalSwap(PlayerSceneContextPBComponent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_last_time_)
      + sizeof(PlayerSceneContextPBComponent::_impl_.scene_info_last_time_)
      - PROTOBUF_FIELD_OFFSET(PlayerSceneContextPBComponent, _impl_.scene_info_)>(
          reinterpret_cast<char*>(&_impl_.scene_info_),
          reinterpret_cast<char*>(&other->_impl_.scene_info_));
}

::google::protobuf::Metadata PlayerSceneContextPBComponent::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fplayer_5fscene_5fcomp_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
