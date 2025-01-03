// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/logic/server_player/centre_player_scene.proto
// Protobuf C++ Version: 5.26.1

#include "proto/logic/server_player/centre_player_scene.pb.h"

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
PROTOBUF_CONSTEXPR CentreLeaveSceneRequest::CentreLeaveSceneRequest(::_pbi::ConstantInitialized) {}
struct CentreLeaveSceneRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CentreLeaveSceneRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CentreLeaveSceneRequestDefaultTypeInternal() {}
  union {
    CentreLeaveSceneRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CentreLeaveSceneRequestDefaultTypeInternal _CentreLeaveSceneRequest_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR CentreLeaveSceneAsyncSavePlayerCompleteRequest::CentreLeaveSceneAsyncSavePlayerCompleteRequest(::_pbi::ConstantInitialized) {}
struct CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal() {}
  union {
    CentreLeaveSceneAsyncSavePlayerCompleteRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CentreLeaveSceneAsyncSavePlayerCompleteRequestDefaultTypeInternal _CentreLeaveSceneAsyncSavePlayerCompleteRequest_default_instance_;

inline constexpr CentreEnterSceneRequest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        scene_info_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR CentreEnterSceneRequest::CentreEnterSceneRequest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CentreEnterSceneRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CentreEnterSceneRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CentreEnterSceneRequestDefaultTypeInternal() {}
  union {
    CentreEnterSceneRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CentreEnterSceneRequestDefaultTypeInternal _CentreEnterSceneRequest_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[3];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto = nullptr;
static const ::_pb::ServiceDescriptor*
    file_level_service_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[1];
const ::uint32_t
    TableStruct_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        PROTOBUF_FIELD_OFFSET(::CentreEnterSceneRequest, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::CentreEnterSceneRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::CentreEnterSceneRequest, _impl_.scene_info_),
        0,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CentreLeaveSceneRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::CentreLeaveSceneAsyncSavePlayerCompleteRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, 9, -1, sizeof(::CentreEnterSceneRequest)},
        {10, -1, -1, sizeof(::CentreLeaveSceneRequest)},
        {18, -1, -1, sizeof(::CentreLeaveSceneAsyncSavePlayerCompleteRequest)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_CentreEnterSceneRequest_default_instance_._instance,
    &::_CentreLeaveSceneRequest_default_instance_._instance,
    &::_CentreLeaveSceneAsyncSavePlayerCompleteRequest_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n3proto/logic/server_player/centre_playe"
    "r_scene.proto\032\033google/protobuf/empty.pro"
    "to\032\031proto/common/common.proto\032,proto/log"
    "ic/client_player/player_scene.proto\"D\n\027C"
    "entreEnterSceneRequest\022)\n\nscene_info\030\001 \001"
    "(\0132\025.SceneInfoPBComponent\"\031\n\027CentreLeave"
    "SceneRequest\"0\n.CentreLeaveSceneAsyncSav"
    "ePlayerCompleteRequest2\303\002\n\030CentrePlayerS"
    "ceneService\022>\n\nEnterScene\022\030.CentreEnterS"
    "ceneRequest\032\026.google.protobuf.Empty\022>\n\nL"
    "eaveScene\022\030.CentreLeaveSceneRequest\032\026.go"
    "ogle.protobuf.Empty\022l\n!LeaveSceneAsyncSa"
    "vePlayerComplete\022/.CentreLeaveSceneAsync"
    "SavePlayerCompleteRequest\032\026.google.proto"
    "buf.Empty\0229\n\014SceneInfoC2S\022\021.SceneInfoReq"
    "uest\032\026.google.protobuf.EmptyB\014Z\007pb/game\200"
    "\001\001b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_deps[3] =
    {
        &::descriptor_table_google_2fprotobuf_2fempty_2eproto,
        &::descriptor_table_proto_2fcommon_2fcommon_2eproto,
        &::descriptor_table_proto_2flogic_2fclient_5fplayer_2fplayer_5fscene_2eproto,
};
static ::absl::once_flag descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto = {
    false,
    false,
    650,
    descriptor_table_protodef_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto,
    "proto/logic/server_player/centre_player_scene.proto",
    &descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_once,
    descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_deps,
    3,
    3,
    schemas,
    file_default_instances,
    TableStruct_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto::offsets,
    file_level_metadata_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto,
    file_level_enum_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto,
    file_level_service_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_getter() {
  return &descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto;
}
// ===================================================================

class CentreEnterSceneRequest::_Internal {
 public:
  using HasBits = decltype(std::declval<CentreEnterSceneRequest>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(CentreEnterSceneRequest, _impl_._has_bits_);
};

void CentreEnterSceneRequest::clear_scene_info() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.scene_info_ != nullptr) _impl_.scene_info_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
CentreEnterSceneRequest::CentreEnterSceneRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:CentreEnterSceneRequest)
}
inline PROTOBUF_NDEBUG_INLINE CentreEnterSceneRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

CentreEnterSceneRequest::CentreEnterSceneRequest(
    ::google::protobuf::Arena* arena,
    const CentreEnterSceneRequest& from)
    : ::google::protobuf::Message(arena) {
  CentreEnterSceneRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.scene_info_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::SceneInfoPBComponent>(
                              arena, *from._impl_.scene_info_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:CentreEnterSceneRequest)
}
inline PROTOBUF_NDEBUG_INLINE CentreEnterSceneRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void CentreEnterSceneRequest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.scene_info_ = {};
}
CentreEnterSceneRequest::~CentreEnterSceneRequest() {
  // @@protoc_insertion_point(destructor:CentreEnterSceneRequest)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void CentreEnterSceneRequest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.scene_info_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CentreEnterSceneRequest::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(CentreEnterSceneRequest, _impl_._cached_size_),
              false,
          },
          &CentreEnterSceneRequest::MergeImpl,
          &CentreEnterSceneRequest::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void CentreEnterSceneRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:CentreEnterSceneRequest)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.scene_info_ != nullptr);
    _impl_.scene_info_->Clear();
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* CentreEnterSceneRequest::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 1, 0, 2> CentreEnterSceneRequest::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(CentreEnterSceneRequest, _impl_._has_bits_),
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_CentreEnterSceneRequest_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::CentreEnterSceneRequest>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .SceneInfoPBComponent scene_info = 1;
    {::_pbi::TcParser::FastMtS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(CentreEnterSceneRequest, _impl_.scene_info_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .SceneInfoPBComponent scene_info = 1;
    {PROTOBUF_FIELD_OFFSET(CentreEnterSceneRequest, _impl_.scene_info_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::SceneInfoPBComponent>()},
  }}, {{
  }},
};

::uint8_t* CentreEnterSceneRequest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:CentreEnterSceneRequest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // .SceneInfoPBComponent scene_info = 1;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        1, *_impl_.scene_info_, _impl_.scene_info_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:CentreEnterSceneRequest)
  return target;
}

::size_t CentreEnterSceneRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:CentreEnterSceneRequest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // .SceneInfoPBComponent scene_info = 1;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.scene_info_);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void CentreEnterSceneRequest::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<CentreEnterSceneRequest*>(&to_msg);
  auto& from = static_cast<const CentreEnterSceneRequest&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:CentreEnterSceneRequest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.scene_info_ != nullptr);
    if (_this->_impl_.scene_info_ == nullptr) {
      _this->_impl_.scene_info_ =
          ::google::protobuf::Message::CopyConstruct<::SceneInfoPBComponent>(arena, *from._impl_.scene_info_);
    } else {
      _this->_impl_.scene_info_->MergeFrom(*from._impl_.scene_info_);
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void CentreEnterSceneRequest::CopyFrom(const CentreEnterSceneRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:CentreEnterSceneRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool CentreEnterSceneRequest::IsInitialized() const {
  return true;
}

void CentreEnterSceneRequest::InternalSwap(CentreEnterSceneRequest* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  swap(_impl_.scene_info_, other->_impl_.scene_info_);
}

::google::protobuf::Metadata CentreEnterSceneRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[0]);
}
// ===================================================================

class CentreLeaveSceneRequest::_Internal {
 public:
};

CentreLeaveSceneRequest::CentreLeaveSceneRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  // @@protoc_insertion_point(arena_constructor:CentreLeaveSceneRequest)
}
CentreLeaveSceneRequest::CentreLeaveSceneRequest(
    ::google::protobuf::Arena* arena,
    const CentreLeaveSceneRequest& from)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  CentreLeaveSceneRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);

  // @@protoc_insertion_point(copy_constructor:CentreLeaveSceneRequest)
}









::google::protobuf::Metadata CentreLeaveSceneRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[1]);
}
// ===================================================================

class CentreLeaveSceneAsyncSavePlayerCompleteRequest::_Internal {
 public:
};

CentreLeaveSceneAsyncSavePlayerCompleteRequest::CentreLeaveSceneAsyncSavePlayerCompleteRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  // @@protoc_insertion_point(arena_constructor:CentreLeaveSceneAsyncSavePlayerCompleteRequest)
}
CentreLeaveSceneAsyncSavePlayerCompleteRequest::CentreLeaveSceneAsyncSavePlayerCompleteRequest(
    ::google::protobuf::Arena* arena,
    const CentreLeaveSceneAsyncSavePlayerCompleteRequest& from)
    : ::google::protobuf::internal::ZeroFieldsBase(arena) {
  CentreLeaveSceneAsyncSavePlayerCompleteRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);

  // @@protoc_insertion_point(copy_constructor:CentreLeaveSceneAsyncSavePlayerCompleteRequest)
}









::google::protobuf::Metadata CentreLeaveSceneAsyncSavePlayerCompleteRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_getter,
                                   &descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto_once,
                                   file_level_metadata_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[2]);
}
// ===================================================================

const ::google::protobuf::ServiceDescriptor* CentrePlayerSceneService::descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto);
  return file_level_service_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[0];
}

const ::google::protobuf::ServiceDescriptor* CentrePlayerSceneService::GetDescriptor() {
  return descriptor();
}

void CentrePlayerSceneService::EnterScene(::google::protobuf::RpcController* controller,
                         const ::CentreEnterSceneRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method EnterScene() not implemented.");
  done->Run();
}
void CentrePlayerSceneService::LeaveScene(::google::protobuf::RpcController* controller,
                         const ::CentreLeaveSceneRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method LeaveScene() not implemented.");
  done->Run();
}
void CentrePlayerSceneService::LeaveSceneAsyncSavePlayerComplete(::google::protobuf::RpcController* controller,
                         const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method LeaveSceneAsyncSavePlayerComplete() not implemented.");
  done->Run();
}
void CentrePlayerSceneService::SceneInfoC2S(::google::protobuf::RpcController* controller,
                         const ::SceneInfoRequest*, ::google::protobuf::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method SceneInfoC2S() not implemented.");
  done->Run();
}

void CentrePlayerSceneService::CallMethod(
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response, ::google::protobuf::Closure* done) {
  ABSL_DCHECK_EQ(method->service(), file_level_service_descriptors_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto[0]);
  switch (method->index()) {
    case 0:
      EnterScene(controller,
             ::google::protobuf::internal::DownCast<const ::CentreEnterSceneRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;
    case 1:
      LeaveScene(controller,
             ::google::protobuf::internal::DownCast<const ::CentreLeaveSceneRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;
    case 2:
      LeaveSceneAsyncSavePlayerComplete(controller,
             ::google::protobuf::internal::DownCast<const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;
    case 3:
      SceneInfoC2S(controller,
             ::google::protobuf::internal::DownCast<const ::SceneInfoRequest*>(request),
             ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response), done);
      break;

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      break;
  }
}

const ::google::protobuf::Message& CentrePlayerSceneService::GetRequestPrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::CentreEnterSceneRequest::default_instance();
    case 1:
      return ::CentreLeaveSceneRequest::default_instance();
    case 2:
      return ::CentreLeaveSceneAsyncSavePlayerCompleteRequest::default_instance();
    case 3:
      return ::SceneInfoRequest::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->input_type());
  }
}

const ::google::protobuf::Message& CentrePlayerSceneService::GetResponsePrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::google::protobuf::Empty::default_instance();
    case 1:
      return ::google::protobuf::Empty::default_instance();
    case 2:
      return ::google::protobuf::Empty::default_instance();
    case 3:
      return ::google::protobuf::Empty::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->output_type());
  }
}

CentrePlayerSceneService_Stub::CentrePlayerSceneService_Stub(::google::protobuf::RpcChannel* channel)
    : channel_(channel), owns_channel_(false) {}

CentrePlayerSceneService_Stub::CentrePlayerSceneService_Stub(
    ::google::protobuf::RpcChannel* channel,
    ::google::protobuf::Service::ChannelOwnership ownership)
    : channel_(channel),
      owns_channel_(ownership ==
                    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {}

CentrePlayerSceneService_Stub::~CentrePlayerSceneService_Stub() {
  if (owns_channel_) delete channel_;
}

void CentrePlayerSceneService_Stub::EnterScene(::google::protobuf::RpcController* controller,
                              const ::CentreEnterSceneRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(0), controller,
                       request, response, done);
}
void CentrePlayerSceneService_Stub::LeaveScene(::google::protobuf::RpcController* controller,
                              const ::CentreLeaveSceneRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(1), controller,
                       request, response, done);
}
void CentrePlayerSceneService_Stub::LeaveSceneAsyncSavePlayerComplete(::google::protobuf::RpcController* controller,
                              const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(2), controller,
                       request, response, done);
}
void CentrePlayerSceneService_Stub::SceneInfoC2S(::google::protobuf::RpcController* controller,
                              const ::SceneInfoRequest* request,
                              ::google::protobuf::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(3), controller,
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
    (::_pbi::AddDescriptors(&descriptor_table_proto_2flogic_2fserver_5fplayer_2fcentre_5fplayer_5fscene_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
