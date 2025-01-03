// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/server/game_scene.proto
// Protobuf C++ Version: 5.26.1

#include "proto/logic/server/game_scene.pb.h"

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
PROTOBUF_CONSTEXPR GameSceneTest::GameSceneTest(::_pbi::ConstantInitialized) {}
struct GameSceneTestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR GameSceneTestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~GameSceneTestDefaultTypeInternal() {}
  union {
    GameSceneTest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 GameSceneTestDefaultTypeInternal _GameSceneTest_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2flogic_2fserver_2fgame_5fscene_2eproto[1];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto = nullptr;
static const ::_pb::ServiceDescriptor*
    file_level_service_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto[1];
const ::uint32_t
    TableStruct_proto_2flogic_2fserver_2fgame_5fscene_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::GameSceneTest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::GameSceneTest)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_GameSceneTest_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fserver_2fgame_5fscene_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n#proto/logic/server/game_scene.proto\032\030p"
    "roto/common/empty.proto\"\017\n\rGameSceneTest"
    "22\n\020GameSceneService\022\036\n\004Test\022\016.GameScene"
    "Test\032\006.EmptyB\014Z\007pb/game\200\001\001b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_deps[1] =
    {
        &::descriptor_table_proto_2fcommon_2fempty_2eproto,
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto = {
    false,
    false,
    154,
    descriptor_table_protodef_proto_2flogic_2fserver_2fgame_5fscene_2eproto,
    "proto/logic/server/game_scene.proto",
    &descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_once,
    descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_deps,
    1,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fserver_2fgame_5fscene_2eproto::offsets,
    file_level_metadata_proto_2flogic_2fserver_2fgame_5fscene_2eproto,
    file_level_enum_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto,
    file_level_service_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_getter() {
  return &descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto;
}
// ===================================================================

class GameSceneTest::_Internal {
 public:
};

GameSceneTest::GameSceneTest(::google::protobuf::Arena* arena)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  // @@protoc_insertion_point(arena_constructor:GameSceneTest)
}
GameSceneTest::GameSceneTest(
    ::google::protobuf::Arena* arena,
    const GameSceneTest& from)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  GameSceneTest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);

  // @@protoc_insertion_point(copy_constructor:GameSceneTest)
}









::google::protobuf::Metadata GameSceneTest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fserver_2fgame_5fscene_2eproto[0]);
}
// ===================================================================

const ::google::protobuf::ServiceDescriptor* GameSceneService::descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto);
  return file_level_service_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto[0];
}

const ::google::protobuf::ServiceDescriptor* GameSceneService::GetDescriptor() {
  return descriptor();
}

void GameSceneService::Test(::google::protobuf::RpcController* controller,
                         const ::GameSceneTest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method Test() not implemented.");
  done->Run();
}

void GameSceneService::CallMethod(
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response, ::google::protobuf::Closure* done) {
  ABSL_DCHECK_EQ(method->service(), file_level_service_descriptors_proto_2flogic_2fserver_2fgame_5fscene_2eproto[0]);
  switch (method->index()) {
    case 0:
      Test(controller,
             ::google::protobuf::internal::DownCast<const ::GameSceneTest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      break;
  }
}

const ::google::protobuf::Message& GameSceneService::GetRequestPrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::GameSceneTest::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->input_type());
  }
}

const ::google::protobuf::Message& GameSceneService::GetResponsePrototype(
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

GameSceneService_Stub::GameSceneService_Stub(::google::protobuf::RpcChannel* channel)
    : channel_(channel), owns_channel_(false) {}

GameSceneService_Stub::GameSceneService_Stub(
    ::google::protobuf::RpcChannel* channel,
    ::google::protobuf::Service::ChannelOwnership ownership)
    : channel_(channel),
      owns_channel_(ownership ==
                    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {}

GameSceneService_Stub::~GameSceneService_Stub() {
  if (owns_channel_) delete channel_;
}

void GameSceneService_Stub::Test(::google::protobuf::RpcController* controller,
                              const ::GameSceneTest* request,
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
    (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fserver_2fgame_5fscene_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
