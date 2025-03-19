// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/common/gate_service.proto
// Protobuf C++ Version: 5.26.1

#include "proto/common/gate_service.pb.h"

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

inline constexpr KickSessionRequest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : session_id_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR KickSessionRequest::KickSessionRequest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct KickSessionRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR KickSessionRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~KickSessionRequestDefaultTypeInternal() {}
  union {
    KickSessionRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 KickSessionRequestDefaultTypeInternal _KickSessionRequest_default_instance_;

inline constexpr BroadcastToPlayersRequest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        session_list_{},
        _session_list_cached_byte_size_{0},
        message_content_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR BroadcastToPlayersRequest::BroadcastToPlayersRequest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct BroadcastToPlayersRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR BroadcastToPlayersRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~BroadcastToPlayersRequestDefaultTypeInternal() {}
  union {
    BroadcastToPlayersRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 BroadcastToPlayersRequestDefaultTypeInternal _BroadcastToPlayersRequest_default_instance_;
static ::_pb::Metadata file_level_metadata_proto_2fcommon_2fgate_5fservice_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2fcommon_2fgate_5fservice_2eproto = nullptr;
static const ::_pb::ServiceDescriptor*
    file_level_service_descriptors_proto_2fcommon_2fgate_5fservice_2eproto[1];
const ::uint32_t
    TableStruct_proto_2fcommon_2fgate_5fservice_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::KickSessionRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::KickSessionRequest, _impl_.session_id_),
        PROTOBUF_FIELD_OFFSET(::BroadcastToPlayersRequest, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::BroadcastToPlayersRequest, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::BroadcastToPlayersRequest, _impl_.session_list_),
        PROTOBUF_FIELD_OFFSET(::BroadcastToPlayersRequest, _impl_.message_content_),
        ~0u,
        0,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::KickSessionRequest)},
        {9, 19, -1, sizeof(::BroadcastToPlayersRequest)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_KickSessionRequest_default_instance_._instance,
    &::_BroadcastToPlayersRequest_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2fcommon_2fgate_5fservice_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\037proto/common/gate_service.proto\032\030proto"
    "/common/empty.proto\032\031proto/common/common"
    ".proto\032!proto/common/common_message.prot"
    "o\"(\n\022KickSessionRequest\022\022\n\nsession_id\030\001 "
    "\001(\004\"[\n\031BroadcastToPlayersRequest\022\024\n\014sess"
    "ion_list\030\001 \003(\004\022(\n\017message_content\030\002 \001(\0132"
    "\017.MessageContent2\206\004\n\013GateService\0220\n\014Regi"
    "sterGame\022\030.RegisterGameNodeRequest\032\006.Emp"
    "ty\0224\n\016UnRegisterGame\022\032.UnregisterGameNod"
    "eRequest\032\006.Empty\022X\n\023PlayerEnterGameNode\022"
    "\037.RegisterGameNodeSessionRequest\032 .Regis"
    "terGameNodeSessionResponse\0227\n\023SendMessag"
    "eToPlayer\022\030.NodeRouteMessageRequest\032\006.Em"
    "pty\0222\n\023KickSessionByCentre\022\023.KickSession"
    "Request\032\006.Empty\022\?\n\020RouteNodeMessage\022\024.Ro"
    "uteMessageRequest\032\025.RouteMessageResponse"
    "\022M\n\022RoutePlayerMessage\022\032.RoutePlayerMess"
    "ageRequest\032\033.RoutePlayerMessageResponse\022"
    "8\n\022BroadcastToPlayers\022\032.BroadcastToPlaye"
    "rsRequest\032\006.EmptyB\014Z\007pb/game\200\001\001b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_deps[3] =
    {
        &::descriptor_table_proto_2fcommon_2fcommon_2eproto,
        &::descriptor_table_proto_2fcommon_2fcommon_5fmessage_2eproto,
        &::descriptor_table_proto_2fcommon_2fempty_2eproto,
};
static ::absl::once_flag descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto = {
    false,
    false,
    799,
    descriptor_table_protodef_proto_2fcommon_2fgate_5fservice_2eproto,
    "proto/common/gate_service.proto",
    &descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_once,
    descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_deps,
    3,
    2,
    schemas,
    file_default_instances,
    TableStruct_proto_2fcommon_2fgate_5fservice_2eproto::offsets,
    file_level_metadata_proto_2fcommon_2fgate_5fservice_2eproto,
    file_level_enum_descriptors_proto_2fcommon_2fgate_5fservice_2eproto,
    file_level_service_descriptors_proto_2fcommon_2fgate_5fservice_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_getter() {
  return &descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto;
}
// ===================================================================

class KickSessionRequest::_Internal {
 public:
};

KickSessionRequest::KickSessionRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:KickSessionRequest)
}
KickSessionRequest::KickSessionRequest(
    ::google::protobuf::Arena* arena, const KickSessionRequest& from)
    : KickSessionRequest(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE KickSessionRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void KickSessionRequest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.session_id_ = {};
}
KickSessionRequest::~KickSessionRequest() {
  // @@protoc_insertion_point(destructor:KickSessionRequest)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void KickSessionRequest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
KickSessionRequest::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(KickSessionRequest, _impl_._cached_size_),
              false,
          },
          &KickSessionRequest::MergeImpl,
          &KickSessionRequest::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void KickSessionRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:KickSessionRequest)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.session_id_ = ::uint64_t{0u};
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* KickSessionRequest::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> KickSessionRequest::_table_ = {
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
    &_KickSessionRequest_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::KickSessionRequest>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 session_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(KickSessionRequest, _impl_.session_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(KickSessionRequest, _impl_.session_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 session_id = 1;
    {PROTOBUF_FIELD_OFFSET(KickSessionRequest, _impl_.session_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* KickSessionRequest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:KickSessionRequest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint64 session_id = 1;
  if (this->_internal_session_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        1, this->_internal_session_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:KickSessionRequest)
  return target;
}

::size_t KickSessionRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:KickSessionRequest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint64 session_id = 1;
  if (this->_internal_session_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_session_id());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void KickSessionRequest::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<KickSessionRequest*>(&to_msg);
  auto& from = static_cast<const KickSessionRequest&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:KickSessionRequest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_session_id() != 0) {
    _this->_impl_.session_id_ = from._impl_.session_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void KickSessionRequest::CopyFrom(const KickSessionRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:KickSessionRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool KickSessionRequest::IsInitialized() const {
  return true;
}

void KickSessionRequest::InternalSwap(KickSessionRequest* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.session_id_, other->_impl_.session_id_);
}

::google::protobuf::Metadata KickSessionRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_getter,
                                   &descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_once,
                                   file_level_metadata_proto_2fcommon_2fgate_5fservice_2eproto[0]);
}
// ===================================================================

class BroadcastToPlayersRequest::_Internal {
 public:
  using HasBits = decltype(std::declval<BroadcastToPlayersRequest>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_._has_bits_);
};

void BroadcastToPlayersRequest::clear_message_content() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.message_content_ != nullptr) _impl_.message_content_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
BroadcastToPlayersRequest::BroadcastToPlayersRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:BroadcastToPlayersRequest)
}
inline PROTOBUF_NDEBUG_INLINE BroadcastToPlayersRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        session_list_{visibility, arena, from.session_list_},
        _session_list_cached_byte_size_{0} {}

BroadcastToPlayersRequest::BroadcastToPlayersRequest(
    ::google::protobuf::Arena* arena,
    const BroadcastToPlayersRequest& from)
    : ::google::protobuf::Message(arena) {
  BroadcastToPlayersRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.message_content_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::MessageContent>(
                              arena, *from._impl_.message_content_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:BroadcastToPlayersRequest)
}
inline PROTOBUF_NDEBUG_INLINE BroadcastToPlayersRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        session_list_{visibility, arena},
        _session_list_cached_byte_size_{0} {}

inline void BroadcastToPlayersRequest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.message_content_ = {};
}
BroadcastToPlayersRequest::~BroadcastToPlayersRequest() {
  // @@protoc_insertion_point(destructor:BroadcastToPlayersRequest)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void BroadcastToPlayersRequest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.message_content_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
BroadcastToPlayersRequest::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_._cached_size_),
              false,
          },
          &BroadcastToPlayersRequest::MergeImpl,
          &BroadcastToPlayersRequest::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void BroadcastToPlayersRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:BroadcastToPlayersRequest)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.session_list_.Clear();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.message_content_ != nullptr);
    _impl_.message_content_->Clear();
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* BroadcastToPlayersRequest::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 1, 0, 2> BroadcastToPlayersRequest::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_BroadcastToPlayersRequest_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::BroadcastToPlayersRequest>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .MessageContent message_content = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_.message_content_)}},
    // repeated uint64 session_list = 1;
    {::_pbi::TcParser::FastV64P1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_.session_list_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated uint64 session_list = 1;
    {PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_.session_list_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt64)},
    // .MessageContent message_content = 2;
    {PROTOBUF_FIELD_OFFSET(BroadcastToPlayersRequest, _impl_.message_content_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::MessageContent>()},
  }}, {{
  }},
};

::uint8_t* BroadcastToPlayersRequest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:BroadcastToPlayersRequest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated uint64 session_list = 1;
  {
    int byte_size = _impl_._session_list_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt64Packed(
          1, _internal_session_list(), byte_size, target);
    }
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .MessageContent message_content = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.message_content_, _impl_.message_content_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:BroadcastToPlayersRequest)
  return target;
}

::size_t BroadcastToPlayersRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:BroadcastToPlayersRequest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated uint64 session_list = 1;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt64Size(
        this->_internal_session_list())
    ;
    _impl_._session_list_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // .MessageContent message_content = 2;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.message_content_);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void BroadcastToPlayersRequest::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<BroadcastToPlayersRequest*>(&to_msg);
  auto& from = static_cast<const BroadcastToPlayersRequest&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:BroadcastToPlayersRequest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_session_list()->MergeFrom(from._internal_session_list());
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.message_content_ != nullptr);
    if (_this->_impl_.message_content_ == nullptr) {
      _this->_impl_.message_content_ =
          ::google::protobuf::Message::CopyConstruct<::MessageContent>(arena, *from._impl_.message_content_);
    } else {
      _this->_impl_.message_content_->MergeFrom(*from._impl_.message_content_);
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void BroadcastToPlayersRequest::CopyFrom(const BroadcastToPlayersRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:BroadcastToPlayersRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool BroadcastToPlayersRequest::IsInitialized() const {
  return true;
}

void BroadcastToPlayersRequest::InternalSwap(BroadcastToPlayersRequest* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.session_list_.InternalSwap(&other->_impl_.session_list_);
  swap(_impl_.message_content_, other->_impl_.message_content_);
}

::google::protobuf::Metadata BroadcastToPlayersRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_getter,
                                   &descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto_once,
                                   file_level_metadata_proto_2fcommon_2fgate_5fservice_2eproto[1]);
}
// ===================================================================

const ::google::protobuf::ServiceDescriptor* GateService::descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto);
  return file_level_service_descriptors_proto_2fcommon_2fgate_5fservice_2eproto[0];
}

const ::google::protobuf::ServiceDescriptor* GateService::GetDescriptor() {
  return descriptor();
}

void GateService::RegisterGame(::google::protobuf::RpcController* controller,
                         const ::RegisterGameNodeRequest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method RegisterGame() not implemented.");
  done->Run();
}
void GateService::UnRegisterGame(::google::protobuf::RpcController* controller,
                         const ::UnregisterGameNodeRequest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method UnRegisterGame() not implemented.");
  done->Run();
}
void GateService::PlayerEnterGameNode(::google::protobuf::RpcController* controller,
                         const ::RegisterGameNodeSessionRequest*, ::RegisterGameNodeSessionResponse*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method PlayerEnterGameNode() not implemented.");
  done->Run();
}
void GateService::SendMessageToPlayer(::google::protobuf::RpcController* controller,
                         const ::NodeRouteMessageRequest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method SendMessageToPlayer() not implemented.");
  done->Run();
}
void GateService::KickSessionByCentre(::google::protobuf::RpcController* controller,
                         const ::KickSessionRequest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method KickSessionByCentre() not implemented.");
  done->Run();
}
void GateService::RouteNodeMessage(::google::protobuf::RpcController* controller,
                         const ::RouteMessageRequest*, ::RouteMessageResponse*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method RouteNodeMessage() not implemented.");
  done->Run();
}
void GateService::RoutePlayerMessage(::google::protobuf::RpcController* controller,
                         const ::RoutePlayerMessageRequest*, ::RoutePlayerMessageResponse*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method RoutePlayerMessage() not implemented.");
  done->Run();
}
void GateService::BroadcastToPlayers(::google::protobuf::RpcController* controller,
                         const ::BroadcastToPlayersRequest*, ::Empty*, ::google::protobuf::Closure* done) {
  controller->SetFailed("Method BroadcastToPlayers() not implemented.");
  done->Run();
}

void GateService::CallMethod(
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response, ::google::protobuf::Closure* done) {
  ABSL_DCHECK_EQ(method->service(), file_level_service_descriptors_proto_2fcommon_2fgate_5fservice_2eproto[0]);
  switch (method->index()) {
    case 0:
      RegisterGame(controller,
             ::google::protobuf::internal::DownCast<const ::RegisterGameNodeRequest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;
    case 1:
      UnRegisterGame(controller,
             ::google::protobuf::internal::DownCast<const ::UnregisterGameNodeRequest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;
    case 2:
      PlayerEnterGameNode(controller,
             ::google::protobuf::internal::DownCast<const ::RegisterGameNodeSessionRequest*>(request),
             ::google::protobuf::internal::DownCast<::RegisterGameNodeSessionResponse*>(response), done);
      break;
    case 3:
      SendMessageToPlayer(controller,
             ::google::protobuf::internal::DownCast<const ::NodeRouteMessageRequest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;
    case 4:
      KickSessionByCentre(controller,
             ::google::protobuf::internal::DownCast<const ::KickSessionRequest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;
    case 5:
      RouteNodeMessage(controller,
             ::google::protobuf::internal::DownCast<const ::RouteMessageRequest*>(request),
             ::google::protobuf::internal::DownCast<::RouteMessageResponse*>(response), done);
      break;
    case 6:
      RoutePlayerMessage(controller,
             ::google::protobuf::internal::DownCast<const ::RoutePlayerMessageRequest*>(request),
             ::google::protobuf::internal::DownCast<::RoutePlayerMessageResponse*>(response), done);
      break;
    case 7:
      BroadcastToPlayers(controller,
             ::google::protobuf::internal::DownCast<const ::BroadcastToPlayersRequest*>(request),
             ::google::protobuf::internal::DownCast<::Empty*>(response), done);
      break;

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      break;
  }
}

const ::google::protobuf::Message& GateService::GetRequestPrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::RegisterGameNodeRequest::default_instance();
    case 1:
      return ::UnregisterGameNodeRequest::default_instance();
    case 2:
      return ::RegisterGameNodeSessionRequest::default_instance();
    case 3:
      return ::NodeRouteMessageRequest::default_instance();
    case 4:
      return ::KickSessionRequest::default_instance();
    case 5:
      return ::RouteMessageRequest::default_instance();
    case 6:
      return ::RoutePlayerMessageRequest::default_instance();
    case 7:
      return ::BroadcastToPlayersRequest::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->input_type());
  }
}

const ::google::protobuf::Message& GateService::GetResponsePrototype(
    const ::google::protobuf::MethodDescriptor* method) const {
  ABSL_DCHECK_EQ(method->service(), descriptor());
  switch (method->index()) {
    case 0:
      return ::Empty::default_instance();
    case 1:
      return ::Empty::default_instance();
    case 2:
      return ::RegisterGameNodeSessionResponse::default_instance();
    case 3:
      return ::Empty::default_instance();
    case 4:
      return ::Empty::default_instance();
    case 5:
      return ::RouteMessageResponse::default_instance();
    case 6:
      return ::RoutePlayerMessageResponse::default_instance();
    case 7:
      return ::Empty::default_instance();

    default:
      ABSL_LOG(FATAL) << "Bad method index; this should never happen.";
      return *::google::protobuf::MessageFactory::generated_factory()
                  ->GetPrototype(method->output_type());
  }
}

GateService_Stub::GateService_Stub(::google::protobuf::RpcChannel* channel)
    : channel_(channel), owns_channel_(false) {}

GateService_Stub::GateService_Stub(
    ::google::protobuf::RpcChannel* channel,
    ::google::protobuf::Service::ChannelOwnership ownership)
    : channel_(channel),
      owns_channel_(ownership ==
                    ::google::protobuf::Service::STUB_OWNS_CHANNEL) {}

GateService_Stub::~GateService_Stub() {
  if (owns_channel_) delete channel_;
}

void GateService_Stub::RegisterGame(::google::protobuf::RpcController* controller,
                              const ::RegisterGameNodeRequest* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(0), controller,
                       request, response, done);
}
void GateService_Stub::UnRegisterGame(::google::protobuf::RpcController* controller,
                              const ::UnregisterGameNodeRequest* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(1), controller,
                       request, response, done);
}
void GateService_Stub::PlayerEnterGameNode(::google::protobuf::RpcController* controller,
                              const ::RegisterGameNodeSessionRequest* request,
                              ::RegisterGameNodeSessionResponse* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(2), controller,
                       request, response, done);
}
void GateService_Stub::SendMessageToPlayer(::google::protobuf::RpcController* controller,
                              const ::NodeRouteMessageRequest* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(3), controller,
                       request, response, done);
}
void GateService_Stub::KickSessionByCentre(::google::protobuf::RpcController* controller,
                              const ::KickSessionRequest* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(4), controller,
                       request, response, done);
}
void GateService_Stub::RouteNodeMessage(::google::protobuf::RpcController* controller,
                              const ::RouteMessageRequest* request,
                              ::RouteMessageResponse* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(5), controller,
                       request, response, done);
}
void GateService_Stub::RoutePlayerMessage(::google::protobuf::RpcController* controller,
                              const ::RoutePlayerMessageRequest* request,
                              ::RoutePlayerMessageResponse* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(6), controller,
                       request, response, done);
}
void GateService_Stub::BroadcastToPlayers(::google::protobuf::RpcController* controller,
                              const ::BroadcastToPlayersRequest* request,
                              ::Empty* response, ::google::protobuf::Closure* done) {
  channel_->CallMethod(descriptor()->method(7), controller,
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
    (::_pbi::AddDescriptors(&descriptor_table_proto_2fcommon_2fgate_5fservice_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
