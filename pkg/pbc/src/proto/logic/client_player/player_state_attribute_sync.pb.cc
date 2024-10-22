// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/client_player/player_state_attribute_sync.proto
// Protobuf C++ Version: 5.26.1

#include "logic/client_player/player_state_attribute_sync.pb.h"

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

inline constexpr SyncEntityVelocityS2C::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        velocity_{nullptr},
        entity_id_{::uint64_t{0u}} {}

template <typename>
PROTOBUF_CONSTEXPR SyncEntityVelocityS2C::SyncEntityVelocityS2C(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct SyncEntityVelocityS2CDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SyncEntityVelocityS2CDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SyncEntityVelocityS2CDefaultTypeInternal() {}
  union {
    SyncEntityVelocityS2C _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SyncEntityVelocityS2CDefaultTypeInternal _SyncEntityVelocityS2C_default_instance_;
static ::_pb::Metadata file_level_metadata_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[1];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto = nullptr;
static const ::_pb::ServiceDescriptor*
    file_level_service_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[1];
const ::uint32_t
    TableStruct_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        PROTOBUF_FIELD_OFFSET(::SyncEntityVelocityS2C, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::SyncEntityVelocityS2C, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::SyncEntityVelocityS2C, _impl_.entity_id_),
        PROTOBUF_FIELD_OFFSET(::SyncEntityVelocityS2C, _impl_.velocity_),
        ~0u,
        0,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, 10, -1, sizeof(::SyncEntityVelocityS2C)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_SyncEntityVelocityS2C_default_instance_._instance,
};
const char descriptor_table_protodef_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n5logic/client_player/player_state_attri"
    "bute_sync.proto\032\020common/tip.proto\032\021commo"
    "n/comp.proto\032\022common/empty.proto\032 logic/"
    "component/skill_comp.proto\032 logic/compon"
    "ent/actor_comp.proto\"G\n\025SyncEntityVeloci"
    "tyS2C\022\021\n\tentity_id\030\001 \001(\004\022\033\n\010velocity\030\002 \001"
    "(\0132\t.Velocity2H\n\026EntityStateSyncService\022"
    ".\n\014SyncVelocity\022\026.SyncEntityVelocityS2C\032"
    "\006.EmptyB\014Z\007pb/game\200\001\001b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_deps[5] =
    {
        &::descriptor_table_common_2fcomp_2eproto,
        &::descriptor_table_common_2fempty_2eproto,
        &::descriptor_table_common_2ftip_2eproto,
        &::descriptor_table_logic_2fcomponent_2factor_5fcomp_2eproto,
        &::descriptor_table_logic_2fcomponent_2fskill_5fcomp_2eproto,
};
static ::absl::once_flag descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto = {
    false,
    false,
    349,
    descriptor_table_protodef_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto,
    "logic/client_player/player_state_attribute_sync.proto",
    &descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_once,
    descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_deps,
    5,
    1,
    schemas,
    file_default_instances,
    TableStruct_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto::offsets,
    file_level_metadata_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto,
    file_level_enum_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto,
    file_level_service_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_getter() {
  return &descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto;
}
// ===================================================================

class SyncEntityVelocityS2C::_Internal {
 public:
  using HasBits = decltype(std::declval<SyncEntityVelocityS2C>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_._has_bits_);
};

void SyncEntityVelocityS2C::clear_velocity() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.velocity_ != nullptr) _impl_.velocity_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
SyncEntityVelocityS2C::SyncEntityVelocityS2C(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:SyncEntityVelocityS2C)
}
inline PROTOBUF_NDEBUG_INLINE SyncEntityVelocityS2C::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

SyncEntityVelocityS2C::SyncEntityVelocityS2C(
    ::google::protobuf::Arena* arena,
    const SyncEntityVelocityS2C& from)
    : ::google::protobuf::Message(arena) {
  SyncEntityVelocityS2C* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.velocity_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::Velocity>(
                              arena, *from._impl_.velocity_)
                        : nullptr;
  _impl_.entity_id_ = from._impl_.entity_id_;

  // @@protoc_insertion_point(copy_constructor:SyncEntityVelocityS2C)
}
inline PROTOBUF_NDEBUG_INLINE SyncEntityVelocityS2C::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void SyncEntityVelocityS2C::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, velocity_),
           0,
           offsetof(Impl_, entity_id_) -
               offsetof(Impl_, velocity_) +
               sizeof(Impl_::entity_id_));
}
SyncEntityVelocityS2C::~SyncEntityVelocityS2C() {
  // @@protoc_insertion_point(destructor:SyncEntityVelocityS2C)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void SyncEntityVelocityS2C::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.velocity_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
SyncEntityVelocityS2C::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_._cached_size_),
              false,
          },
          &SyncEntityVelocityS2C::MergeImpl,
          &SyncEntityVelocityS2C::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void SyncEntityVelocityS2C::Clear() {
// @@protoc_insertion_point(message_clear_start:SyncEntityVelocityS2C)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.velocity_ != nullptr);
    _impl_.velocity_->Clear();
  }
  _impl_.entity_id_ = ::uint64_t{0u};
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* SyncEntityVelocityS2C::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 1, 0, 2> SyncEntityVelocityS2C::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_SyncEntityVelocityS2C_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::SyncEntityVelocityS2C>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .Velocity velocity = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.velocity_)}},
    // uint64 entity_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(SyncEntityVelocityS2C, _impl_.entity_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.entity_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 entity_id = 1;
    {PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.entity_id_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // .Velocity velocity = 2;
    {PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.velocity_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Velocity>()},
  }}, {{
  }},
};

::uint8_t* SyncEntityVelocityS2C::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:SyncEntityVelocityS2C)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 entity_id = 1;
  if (this->_internal_entity_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_entity_id(), target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Velocity velocity = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.velocity_, _impl_.velocity_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:SyncEntityVelocityS2C)
  return target;
}

::size_t SyncEntityVelocityS2C::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:SyncEntityVelocityS2C)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // .Velocity velocity = 2;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.velocity_);
  }

  // uint64 entity_id = 1;
  if (this->_internal_entity_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_entity_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void SyncEntityVelocityS2C::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<SyncEntityVelocityS2C*>(&to_msg);
  auto& from = static_cast<const SyncEntityVelocityS2C&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:SyncEntityVelocityS2C)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.velocity_ != nullptr);
    if (_this->_impl_.velocity_ == nullptr) {
      _this->_impl_.velocity_ =
          ::google::protobuf::Message::CopyConstruct<::Velocity>(arena, *from._impl_.velocity_);
    } else {
      _this->_impl_.velocity_->MergeFrom(*from._impl_.velocity_);
    }
  }
  if (from._internal_entity_id() != 0) {
    _this->_impl_.entity_id_ = from._impl_.entity_id_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void SyncEntityVelocityS2C::CopyFrom(const SyncEntityVelocityS2C& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:SyncEntityVelocityS2C)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool SyncEntityVelocityS2C::IsInitialized() const {
  return true;
}

void SyncEntityVelocityS2C::InternalSwap(SyncEntityVelocityS2C* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.entity_id_)
      + sizeof(SyncEntityVelocityS2C::_impl_.entity_id_)
      - PROTOBUF_FIELD_OFFSET(SyncEntityVelocityS2C, _impl_.velocity_)>(
          reinterpret_cast<char*>(&_impl_.velocity_),
          reinterpret_cast<char*>(&other->_impl_.velocity_));
}

::google::protobuf::Metadata SyncEntityVelocityS2C::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_getter,
                                   &descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto_once,
                                   file_level_metadata_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[0]);
}
// ===================================================================

const ::google::protobuf::ServiceDescriptor* EntityStateSyncService::descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto);
  return file_level_service_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[0];
}

const ::google::protobuf::ServiceDescriptor* EntityStateSyncService::GetDescriptor() {
  return descriptor();
}

void EntityStateSyncService::SyncVelocity(::google::protobuf::RpcController* controller,
                         const ::SyncEntityVelocityS2C*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method SyncVelocity() not implemented.");
  done->Run();
}

void EntityStateSyncService::CallMethod(
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response, ::google::protobuf::Closure* done) {
  ABSL_DCHECK_EQ(method->service(), file_level_service_descriptors_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto[0]);
  switch (method->index()) {
    case 0:
      SyncVelocity(controller,
             ::google::protobuf::internal::DownCast<const ::SyncEntityVelocityS2C*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      break;
  }
}

const ::google::protobuf::Message& EntityStateSyncService::GetRequestPrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::SyncEntityVelocityS2C::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->input_type());
  }
}

const ::google::protobuf::Message& EntityStateSyncService::GetResponsePrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::Empty::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->output_type());
  }
}

EntityStateSyncService_Stub::EntityStateSyncService_Stub(::google::protobuf::RpcChannel* channel)
    : channel_(channel), owns_channel_(false) {}

EntityStateSyncService_Stub::EntityStateSyncService_Stub(
    ::google::protobuf::RpcChannel* channel,
    ::google::protobuf::Service::ChannelOwnership ownership)
    : channel_(channel),
      owns_channel_(ownership ==
                    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {}

EntityStateSyncService_Stub::~EntityStateSyncService_Stub() {
  if (owns_channel_) delete channel_;
}

void EntityStateSyncService_Stub::SyncVelocity(::google::protobuf::RpcController* controller,
                              const ::SyncEntityVelocityS2C* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(0), controller,
                       request, response, done);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_logic_2fclient_5fplayer_2fplayer_5fstate_5fattribute_5fsync_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
