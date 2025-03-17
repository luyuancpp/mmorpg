// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/logic/component/game_node_comp.proto
// Protobuf C++ Version: 6.31.0-dev

#include "proto/logic/component/game_node_comp.pb.h"

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

inline constexpr GameNodePlayerInfoPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        player_size_{0u} {}

template <typename>
PROTOBUF_CONSTEXPR GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(GameNodePlayerInfoPBComponent_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct GameNodePlayerInfoPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GameNodePlayerInfoPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GameNodePlayerInfoPBComponentDefaultTypeInternal() {}
  union {
    GameNodePlayerInfoPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GameNodePlayerInfoPBComponentDefaultTypeInternal _GameNodePlayerInfoPBComponent_default_instance_;
static constexpr const ::_pb::EnumDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        0x081, // bitmap
        PROTOBUF_FIELD_OFFSET(::GameNodePlayerInfoPBComponent, _impl_._has_bits_),
        4, // hasbit index offset
        PROTOBUF_FIELD_OFFSET(::GameNodePlayerInfoPBComponent, _impl_.player_size_),
        0,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, sizeof(::GameNodePlayerInfoPBComponent)},
};
static const ::_pb::Message* PROTOBUF_NONNULL const file_default_instances[] = {
    &::_GameNodePlayerInfoPBComponent_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n*proto/logic/component/game_node_comp.p"
    "roto\"4\n\035GameNodePlayerInfoPBComponent\022\023\n"
    "\013player_size\030\001 \001(\rB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = {
    false,
    false,
    117,
    descriptor_table_protodef_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
    "proto/logic/component/game_node_comp.proto",
    &descriptor_table_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto::offsets,
    file_level_enum_descriptors_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
    file_level_service_descriptors_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
};
// ===================================================================

class GameNodePlayerInfoPBComponent::_Internal {
 public:
  using HasBits =
      decltype(std::declval<GameNodePlayerInfoPBComponent>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_._has_bits_);
};

GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, GameNodePlayerInfoPBComponent_class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:GameNodePlayerInfoPBComponent)
}
GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const GameNodePlayerInfoPBComponent& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, GameNodePlayerInfoPBComponent_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(from._impl_) {
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
}
PROTOBUF_NDEBUG_INLINE GameNodePlayerInfoPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
      : _cached_size_{0} {}

inline void GameNodePlayerInfoPBComponent::SharedCtor(::_pb::Arena* PROTOBUF_NULLABLE arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.player_size_ = {};
}
GameNodePlayerInfoPBComponent::~GameNodePlayerInfoPBComponent() {
  // @@protoc_insertion_point(destructor:GameNodePlayerInfoPBComponent)
  SharedDtor(*this);
}
inline void GameNodePlayerInfoPBComponent::SharedDtor(MessageLite& self) {
  GameNodePlayerInfoPBComponent& this_ = static_cast<GameNodePlayerInfoPBComponent&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* PROTOBUF_NONNULL GameNodePlayerInfoPBComponent::PlacementNew_(
    const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena) {
  return ::new (mem) GameNodePlayerInfoPBComponent(arena);
}
constexpr auto GameNodePlayerInfoPBComponent::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(GameNodePlayerInfoPBComponent),
                                            alignof(GameNodePlayerInfoPBComponent));
}
constexpr auto GameNodePlayerInfoPBComponent::InternalGenerateClassData_() {
  return ::google::protobuf::internal::ClassDataFull{
      ::google::protobuf::internal::ClassData{
          &_GameNodePlayerInfoPBComponent_default_instance_._instance,
          &_table_.header,
          nullptr,  // OnDemandRegisterArenaDtor
          nullptr,  // IsInitialized
          &GameNodePlayerInfoPBComponent::MergeImpl,
          ::google::protobuf::Message::GetNewImpl<GameNodePlayerInfoPBComponent>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
          &GameNodePlayerInfoPBComponent::SharedDtor,
          ::google::protobuf::Message::GetClearImpl<GameNodePlayerInfoPBComponent>(), &GameNodePlayerInfoPBComponent::ByteSizeLong,
              &GameNodePlayerInfoPBComponent::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
          PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_._cached_size_),
          false,
      },
      &GameNodePlayerInfoPBComponent::kDescriptorMethods,
      &descriptor_table_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
      nullptr,  // tracker
  };
}

PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 const ::google::protobuf::internal::ClassDataFull
        GameNodePlayerInfoPBComponent_class_data_ =
            GameNodePlayerInfoPBComponent::InternalGenerateClassData_();

const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL GameNodePlayerInfoPBComponent::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&GameNodePlayerInfoPBComponent_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(GameNodePlayerInfoPBComponent_class_data_.tc_table);
  return GameNodePlayerInfoPBComponent_class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2>
GameNodePlayerInfoPBComponent::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_._has_bits_),
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    GameNodePlayerInfoPBComponent_class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::GameNodePlayerInfoPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 player_size = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(GameNodePlayerInfoPBComponent, _impl_.player_size_), 0>(),
     {8, 0, 0, PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_.player_size_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 player_size = 1;
    {PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_.player_size_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};
PROTOBUF_NOINLINE void GameNodePlayerInfoPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:GameNodePlayerInfoPBComponent)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.player_size_ = 0u;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::uint8_t* PROTOBUF_NONNULL GameNodePlayerInfoPBComponent::_InternalSerialize(
    const ::google::protobuf::MessageLite& base, ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) {
  const GameNodePlayerInfoPBComponent& this_ = static_cast<const GameNodePlayerInfoPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::uint8_t* PROTOBUF_NONNULL GameNodePlayerInfoPBComponent::_InternalSerialize(
    ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
  const GameNodePlayerInfoPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(serialize_to_array_start:GameNodePlayerInfoPBComponent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 player_size = 1;
  if ((this_._impl_._has_bits_[0] & 0x00000001u) != 0) {
    if (this_._internal_player_size() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
          1, this_._internal_player_size(), target);
    }
  }

  if (ABSL_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:GameNodePlayerInfoPBComponent)
  return target;
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::size_t GameNodePlayerInfoPBComponent::ByteSizeLong(const MessageLite& base) {
  const GameNodePlayerInfoPBComponent& this_ = static_cast<const GameNodePlayerInfoPBComponent&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::size_t GameNodePlayerInfoPBComponent::ByteSizeLong() const {
  const GameNodePlayerInfoPBComponent& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(message_byte_size_start:GameNodePlayerInfoPBComponent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void)cached_has_bits;

   {
    // uint32 player_size = 1;
    cached_has_bits = this_._impl_._has_bits_[0];
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (this_._internal_player_size() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
            this_._internal_player_size());
      }
    }
  }
  return this_.MaybeComputeUnknownFieldsSize(total_size,
                                             &this_._impl_._cached_size_);
}

void GameNodePlayerInfoPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<GameNodePlayerInfoPBComponent*>(&to_msg);
  auto& from = static_cast<const GameNodePlayerInfoPBComponent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:GameNodePlayerInfoPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if ((cached_has_bits & 0x00000001u) != 0) {
    if (from._internal_player_size() != 0) {
      _this->_impl_.player_size_ = from._impl_.player_size_;
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void GameNodePlayerInfoPBComponent::CopyFrom(const GameNodePlayerInfoPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:GameNodePlayerInfoPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void GameNodePlayerInfoPBComponent::InternalSwap(GameNodePlayerInfoPBComponent* PROTOBUF_RESTRICT PROTOBUF_NONNULL other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  swap(_impl_.player_size_, other->_impl_.player_size_);
}

::google::protobuf::Metadata GameNodePlayerInfoPBComponent::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fcomponent_2fgame_5fnode_5fcomp_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
