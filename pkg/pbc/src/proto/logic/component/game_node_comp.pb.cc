// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/component/game_node_comp.proto
// Protobuf C++ Version: 5.26.1

#include "logic/component/game_node_comp.pb.h"

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

inline constexpr GameNodePlayerInfoPBComponent::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : player_size_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct GameNodePlayerInfoPBComponentDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GameNodePlayerInfoPBComponentDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GameNodePlayerInfoPBComponentDefaultTypeInternal() {}
  union {
    GameNodePlayerInfoPBComponent _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GameNodePlayerInfoPBComponentDefaultTypeInternal _GameNodePlayerInfoPBComponent_default_instance_;
static ::_pb::Metadata file_level_metadata_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto[1];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = nullptr;
const ::uint32_t
    TableStruct_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::GameNodePlayerInfoPBComponent, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::GameNodePlayerInfoPBComponent, _impl_.player_size_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::GameNodePlayerInfoPBComponent)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_GameNodePlayerInfoPBComponent_default_instance_._instance,
};
const char descriptor_table_protodef_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n$logic/component/game_node_comp.proto\"4"
    "\n\035GameNodePlayerInfoPBComponent\022\023\n\013playe"
    "r_size\030\001 \001(\rB\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto = {
    false,
    false,
    111,
    descriptor_table_protodef_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
    "logic/component/game_node_comp.proto",
    &descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto::offsets,
    file_level_metadata_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
    file_level_enum_descriptors_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
    file_level_service_descriptors_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_getter() {
  return &descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto;
}
// ===================================================================

class GameNodePlayerInfoPBComponent::_Internal {
 public:
};

GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:GameNodePlayerInfoPBComponent)
}
GameNodePlayerInfoPBComponent::GameNodePlayerInfoPBComponent(
    ::google::protobuf::Arena* arena, const GameNodePlayerInfoPBComponent& from)
    : GameNodePlayerInfoPBComponent(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE GameNodePlayerInfoPBComponent::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void GameNodePlayerInfoPBComponent::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.player_size_ = {};
}
GameNodePlayerInfoPBComponent::~GameNodePlayerInfoPBComponent() {
  // @@protoc_insertion_point(destructor:GameNodePlayerInfoPBComponent)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void GameNodePlayerInfoPBComponent::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
GameNodePlayerInfoPBComponent::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_._cached_size_),
              false,
          },
          &GameNodePlayerInfoPBComponent::MergeImpl,
          &GameNodePlayerInfoPBComponent::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void GameNodePlayerInfoPBComponent::Clear() {
// @@protoc_insertion_point(message_clear_start:GameNodePlayerInfoPBComponent)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.player_size_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* GameNodePlayerInfoPBComponent::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> GameNodePlayerInfoPBComponent::_table_ = {
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
    &_GameNodePlayerInfoPBComponent_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::GameNodePlayerInfoPBComponent>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 player_size = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(GameNodePlayerInfoPBComponent, _impl_.player_size_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_.player_size_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 player_size = 1;
    {PROTOBUF_FIELD_OFFSET(GameNodePlayerInfoPBComponent, _impl_.player_size_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* GameNodePlayerInfoPBComponent::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:GameNodePlayerInfoPBComponent)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 player_size = 1;
  if (this->_internal_player_size() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_player_size(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:GameNodePlayerInfoPBComponent)
  return target;
}

::size_t GameNodePlayerInfoPBComponent::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:GameNodePlayerInfoPBComponent)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint32 player_size = 1;
  if (this->_internal_player_size() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_player_size());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void GameNodePlayerInfoPBComponent::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<GameNodePlayerInfoPBComponent*>(&to_msg);
  auto& from = static_cast<const GameNodePlayerInfoPBComponent&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:GameNodePlayerInfoPBComponent)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_player_size() != 0) {
    _this->_impl_.player_size_ = from._impl_.player_size_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void GameNodePlayerInfoPBComponent::CopyFrom(const GameNodePlayerInfoPBComponent& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:GameNodePlayerInfoPBComponent)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool GameNodePlayerInfoPBComponent::IsInitialized() const {
  return true;
}

void GameNodePlayerInfoPBComponent::InternalSwap(GameNodePlayerInfoPBComponent* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.player_size_, other->_impl_.player_size_);
}

::google::protobuf::Metadata GameNodePlayerInfoPBComponent::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_getter,
                                   &descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto_once,
                                   file_level_metadata_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto[0]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_logic_2fcomponent_2fgame_5fnode_5fcomp_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
