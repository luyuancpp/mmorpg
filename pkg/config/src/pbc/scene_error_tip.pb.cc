// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: scene_error_tip.proto
// Protobuf C++ Version: 5.29.0

#include "scene_error_tip.pb.h"

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
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_scene_5ferror_5ftip_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_scene_5ferror_5ftip_2eproto = nullptr;
const ::uint32_t TableStruct_scene_5ferror_5ftip_2eproto::offsets[1] = {};
static constexpr ::_pbi::MigrationSchema* schemas = nullptr;
static constexpr ::_pb::Message* const* file_default_instances = nullptr;
const char descriptor_table_protodef_scene_5ferror_5ftip_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\025scene_error_tip.proto*\344\005\n\013scene_error\022"
    "\022\n\016kScene_errorOK\020\000\022\027\n\023kEnterSceneNotFou"
    "nd\020\036\022\026\n\022kEnterSceneNotFull\020\037\022\027\n\023kEnterSc"
    "eneMainFull\020 \022\027\n\023kEnterGameNodeCrash\020!\022\031"
    "\n\025kEnterSceneServerType\020\"\022\031\n\025kEnterScene"
    "ParamError\020#\022\030\n\024kEnterSceneSceneFull\020$\022\034"
    "\n\030kEnterSceneSceneNotFound\020%\022 \n\034kEnterSc"
    "eneYouInCurrentScene\020&\022\"\n\036kEnterSceneEnt"
    "erCrossRoomScene\020\'\022\031\n\025kEnterSceneGsInfoN"
    "ull\020(\022\025\n\021kEnterSceneGsFull\020)\022\036\n\032kEnterSc"
    "eneYourSceneIsNull\020*\022!\n\035kEnterSceneChang"
    "eSceneOffLine\020+\022\034\n\030kEnterSceneChangingSc"
    "ene\020,\022\031\n\025kEnterSceneChangingGs\020-\022#\n\037kCha"
    "ngeScenePlayerQueueNotFound\020.\022\037\n\033kChange"
    "ScenePlayerQueueFull\020/\022*\n&kChangeScenePl"
    "ayerQueueComponentGsNull\0200\022)\n%kChangeSce"
    "nePlayerQueueComponentEmpty\0201\022 \n\034kChange"
    "SceneEnQueueNotSameGs\0202\022 \n\034kInvalidEnter"
    "SceneParameters\0203\022\033\n\027kCheckEnterSceneCre"
    "ator\0204B\tZ\007pb/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_scene_5ferror_5ftip_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_scene_5ferror_5ftip_2eproto = {
    false,
    false,
    785,
    descriptor_table_protodef_scene_5ferror_5ftip_2eproto,
    "scene_error_tip.proto",
    &descriptor_table_scene_5ferror_5ftip_2eproto_once,
    nullptr,
    0,
    0,
    schemas,
    file_default_instances,
    TableStruct_scene_5ferror_5ftip_2eproto::offsets,
    file_level_enum_descriptors_scene_5ferror_5ftip_2eproto,
    file_level_service_descriptors_scene_5ferror_5ftip_2eproto,
};
const ::google::protobuf::EnumDescriptor* scene_error_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_scene_5ferror_5ftip_2eproto);
  return file_level_enum_descriptors_scene_5ferror_5ftip_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t scene_error_internal_data_[] = {
    65536u, 64u, 3758096384u, 1048575u, };
bool scene_error_IsValid(int value) {
  return 0 <= value && value <= 52 && ((9007198180999169u >> value) & 1) != 0;
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ PROTOBUF_UNUSED =
        (::_pbi::AddDescriptors(&descriptor_table_scene_5ferror_5ftip_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
