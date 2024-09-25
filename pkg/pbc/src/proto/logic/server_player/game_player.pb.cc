// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: logic/server_player/game_player.proto
// Protobuf C++ Version: 5.26.1

#include "logic/server_player/game_player.pb.h"

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
      template <typename>
PROTOBUF_CONSTEXPR GameNodeExitGameRequest::GameNodeExitGameRequest(::_pbi::ConstantInitialized) {}
struct GameNodeExitGameRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GameNodeExitGameRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GameNodeExitGameRequestDefaultTypeInternal() {}
  union {
    GameNodeExitGameRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GameNodeExitGameRequestDefaultTypeInternal _GameNodeExitGameRequest_default_instance_;

inline constexpr Centre2GsLoginRequest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : enter_gs_type_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR Centre2GsLoginRequest::Centre2GsLoginRequest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct Centre2GsLoginRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR Centre2GsLoginRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~Centre2GsLoginRequestDefaultTypeInternal() {}
  union {
    Centre2GsLoginRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 Centre2GsLoginRequestDefaultTypeInternal _Centre2GsLoginRequest_default_instance_;
static ::_pb::Metadata file_level_metadata_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto = nullptr;
static const ::_pb::ServiceDescriptor*
    file_level_service_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[1];
const ::uint32_t
    TableStruct_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::Centre2GsLoginRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::Centre2GsLoginRequest, _impl_.enter_gs_type_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::GameNodeExitGameRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::Centre2GsLoginRequest)},
        {9, -1, -1, sizeof(::GameNodeExitGameRequest)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_Centre2GsLoginRequest_default_instance_._instance,
    &::_GameNodeExitGameRequest_default_instance_._instance,
};
const char descriptor_table_protodef_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n%logic/server_player/game_player.proto\032"
    "\033google/protobuf/empty.proto\".\n\025Centre2G"
    "sLoginRequest\022\025\n\renter_gs_type\030\001 \001(\r\"\031\n\027"
    "GameNodeExitGameRequest2\223\001\n\021GamePlayerSe"
    "rvice\022@\n\016Centre2GsLogin\022\026.Centre2GsLogin"
    "Request\032\026.google.protobuf.Empty\022<\n\010ExitG"
    "ame\022\030.GameNodeExitGameRequest\032\026.google.p"
    "rotobuf.EmptyB\014Z\007pb/game\200\001\001b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_deps[1] =
    {
        &::descriptor_table_google_2fprotobuf_2fempty_2eproto,
};
static ::absl::once_flag descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto = {
    false,
    false,
    315,
    descriptor_table_protodef_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto,
    "logic/server_player/game_player.proto",
    &descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_once,
    descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_deps,
    1,
    2,
    schemas,
    file_default_instances,
    TableStruct_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto::offsets,
    file_level_metadata_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto,
    file_level_enum_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto,
    file_level_service_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_getter() {
  return &descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto;
}
// ===================================================================

class Centre2GsLoginRequest::_Internal {
 public:
};

Centre2GsLoginRequest::Centre2GsLoginRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Centre2GsLoginRequest)
}
Centre2GsLoginRequest::Centre2GsLoginRequest(
    ::google::protobuf::Arena* arena, const Centre2GsLoginRequest& from)
    : Centre2GsLoginRequest(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE Centre2GsLoginRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void Centre2GsLoginRequest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.enter_gs_type_ = {};
}
Centre2GsLoginRequest::~Centre2GsLoginRequest() {
  // @@protoc_insertion_point(destructor:Centre2GsLoginRequest)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void Centre2GsLoginRequest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Centre2GsLoginRequest::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(Centre2GsLoginRequest, _impl_._cached_size_),
              false,
          },
          &Centre2GsLoginRequest::MergeImpl,
          &Centre2GsLoginRequest::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void Centre2GsLoginRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:Centre2GsLoginRequest)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.enter_gs_type_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* Centre2GsLoginRequest::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> Centre2GsLoginRequest::_table_ = {
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
    &_Centre2GsLoginRequest_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Centre2GsLoginRequest>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 enter_gs_type = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Centre2GsLoginRequest, _impl_.enter_gs_type_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(Centre2GsLoginRequest, _impl_.enter_gs_type_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 enter_gs_type = 1;
    {PROTOBUF_FIELD_OFFSET(Centre2GsLoginRequest, _impl_.enter_gs_type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* Centre2GsLoginRequest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Centre2GsLoginRequest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 enter_gs_type = 1;
  if (this->_internal_enter_gs_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_enter_gs_type(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Centre2GsLoginRequest)
  return target;
}

::size_t Centre2GsLoginRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Centre2GsLoginRequest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint32 enter_gs_type = 1;
  if (this->_internal_enter_gs_type() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_enter_gs_type());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void Centre2GsLoginRequest::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<Centre2GsLoginRequest*>(&to_msg);
  auto& from = static_cast<const Centre2GsLoginRequest&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:Centre2GsLoginRequest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_enter_gs_type() != 0) {
    _this->_impl_.enter_gs_type_ = from._impl_.enter_gs_type_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void Centre2GsLoginRequest::CopyFrom(const Centre2GsLoginRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Centre2GsLoginRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool Centre2GsLoginRequest::IsInitialized() const {
  return true;
}

void Centre2GsLoginRequest::InternalSwap(Centre2GsLoginRequest* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.enter_gs_type_, other->_impl_.enter_gs_type_);
}

::google::protobuf::Metadata Centre2GsLoginRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_getter,
                                   &descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_once,
                                   file_level_metadata_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[0]);
}
// ===================================================================

class GameNodeExitGameRequest::_Internal {
 public:
};

GameNodeExitGameRequest::GameNodeExitGameRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  // @@protoc_insertion_point(arena_constructor:GameNodeExitGameRequest)
}
GameNodeExitGameRequest::GameNodeExitGameRequest(
    ::google::protobuf::Arena* arena,
    const GameNodeExitGameRequest& from)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  GameNodeExitGameRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);

  // @@protoc_insertion_point(copy_constructor:GameNodeExitGameRequest)
}









::google::protobuf::Metadata GameNodeExitGameRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_getter,
                                   &descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto_once,
                                   file_level_metadata_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[1]);
}
// ===================================================================

const ::google::protobuf::ServiceDescriptor* GamePlayerService::descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto);
  return file_level_service_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[0];
}

const ::google::protobuf::ServiceDescriptor* GamePlayerService::GetDescriptor() {
  return descriptor();
}

void GamePlayerService::Centre2GsLogin(::google::protobuf::RpcController* controller,
                         const ::Centre2GsLoginRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method Centre2GsLogin() not implemented.");
  done->Run();
}
void GamePlayerService::ExitGame(::google::protobuf::RpcController* controller,
                         const ::GameNodeExitGameRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method ExitGame() not implemented.");
  done->Run();
}

void GamePlayerService::CallMethod(
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response, ::google::protobuf::Closure* done) {
  ABSL_DCHECK_EQ(method->service(), file_level_service_descriptors_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto[0]);
  switch (method->index()) {
    case 0:
      Centre2GsLogin(controller,
             ::google::protobuf::internal::DownCast<const ::Centre2GsLoginRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;
    case 1:
      ExitGame(controller,
             ::google::protobuf::internal::DownCast<const ::GameNodeExitGameRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      break;
  }
}

const ::google::protobuf::Message& GamePlayerService::GetRequestPrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::Centre2GsLoginRequest::default_instance();
    case 1:
      return ::GameNodeExitGameRequest::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->input_type());
  }
}

const ::google::protobuf::Message& GamePlayerService::GetResponsePrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::google::protobuf::Empty::default_instance();
    case 1:
      return ::google::protobuf::Empty::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->output_type());
  }
}

GamePlayerService_Stub::GamePlayerService_Stub(::google::protobuf::RpcChannel* channel)
    : channel_(channel), owns_channel_(false) {}

GamePlayerService_Stub::GamePlayerService_Stub(
    ::google::protobuf::RpcChannel* channel,
    ::google::protobuf::Service::ChannelOwnership ownership)
    : channel_(channel),
      owns_channel_(ownership ==
                    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {}

GamePlayerService_Stub::~GamePlayerService_Stub() {
  if (owns_channel_) delete channel_;
}

void GamePlayerService_Stub::Centre2GsLogin(::google::protobuf::RpcController* controller,
                              const ::Centre2GsLoginRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(0), controller,
                       request, response, done);
}
void GamePlayerService_Stub::ExitGame(::google::protobuf::RpcController* controller,
                              const ::GameNodeExitGameRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(1), controller,
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
    (::_pbi::AddDescriptors(&descriptor_table_logic_2fserver_5fplayer_2fgame_5fplayer_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"