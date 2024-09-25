// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: common/common.proto
// Protobuf C++ Version: 5.26.1

#include "common/common.pb.h"

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

inline constexpr NodeInfo::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : node_id_{0u},
        node_type_{0u},
        launch_time_{::uint64_t{0u}},
        game_node_type_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR NodeInfo::NodeInfo(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NodeInfoDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NodeInfoDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NodeInfoDefaultTypeInternal() {}
  union {
    NodeInfo _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NodeInfoDefaultTypeInternal _NodeInfo_default_instance_;

inline constexpr NetworkAddress::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : ip_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        port_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR NetworkAddress::NetworkAddress(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NetworkAddressDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NetworkAddressDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NetworkAddressDefaultTypeInternal() {}
  union {
    NetworkAddress _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NetworkAddressDefaultTypeInternal _NetworkAddress_default_instance_;
static ::_pb::Metadata file_level_metadata_common_2fcommon_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_common_2fcommon_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_common_2fcommon_2eproto = nullptr;
const ::uint32_t
    TableStruct_common_2fcommon_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::NodeInfo, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::NodeInfo, _impl_.node_id_),
        PROTOBUF_FIELD_OFFSET(::NodeInfo, _impl_.node_type_),
        PROTOBUF_FIELD_OFFSET(::NodeInfo, _impl_.launch_time_),
        PROTOBUF_FIELD_OFFSET(::NodeInfo, _impl_.game_node_type_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::NetworkAddress, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::NetworkAddress, _impl_.ip_),
        PROTOBUF_FIELD_OFFSET(::NetworkAddress, _impl_.port_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::NodeInfo)},
        {12, -1, -1, sizeof(::NetworkAddress)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_NodeInfo_default_instance_._instance,
    &::_NetworkAddress_default_instance_._instance,
};
const char descriptor_table_protodef_common_2fcommon_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\023common/common.proto\032\020common/tip.proto\""
    "[\n\010NodeInfo\022\017\n\007node_id\030\001 \001(\r\022\021\n\tnode_typ"
    "e\030\002 \001(\r\022\023\n\013launch_time\030\003 \001(\004\022\026\n\016game_nod"
    "e_type\030\004 \001(\r\"*\n\016NetworkAddress\022\n\n\002ip\030\001 \001"
    "(\t\022\014\n\004port\030\002 \001(\rB\tZ\007pb/gameb\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_common_2fcommon_2eproto_deps[1] =
    {
        &::descriptor_table_common_2ftip_2eproto,
};
static ::absl::once_flag descriptor_table_common_2fcommon_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_common_2fcommon_2eproto = {
    false,
    false,
    195,
    descriptor_table_protodef_common_2fcommon_2eproto,
    "common/common.proto",
    &descriptor_table_common_2fcommon_2eproto_once,
    descriptor_table_common_2fcommon_2eproto_deps,
    1,
    2,
    schemas,
    file_default_instances,
    TableStruct_common_2fcommon_2eproto::offsets,
    file_level_metadata_common_2fcommon_2eproto,
    file_level_enum_descriptors_common_2fcommon_2eproto,
    file_level_service_descriptors_common_2fcommon_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_common_2fcommon_2eproto_getter() {
  return &descriptor_table_common_2fcommon_2eproto;
}
// ===================================================================

class NodeInfo::_Internal {
 public:
};

NodeInfo::NodeInfo(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:NodeInfo)
}
NodeInfo::NodeInfo(
    ::google::protobuf::Arena* arena, const NodeInfo& from)
    : NodeInfo(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE NodeInfo::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void NodeInfo::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, node_id_),
           0,
           offsetof(Impl_, game_node_type_) -
               offsetof(Impl_, node_id_) +
               sizeof(Impl_::game_node_type_));
}
NodeInfo::~NodeInfo() {
  // @@protoc_insertion_point(destructor:NodeInfo)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void NodeInfo::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NodeInfo::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_._cached_size_),
              false,
          },
          &NodeInfo::MergeImpl,
          &NodeInfo::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void NodeInfo::Clear() {
// @@protoc_insertion_point(message_clear_start:NodeInfo)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.node_id_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.game_node_type_) -
      reinterpret_cast<char*>(&_impl_.node_id_)) + sizeof(_impl_.game_node_type_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* NodeInfo::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 0, 0, 2> NodeInfo::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_NodeInfo_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::NodeInfo>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 game_node_type = 4;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(NodeInfo, _impl_.game_node_type_), 63>(),
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.game_node_type_)}},
    // uint32 node_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(NodeInfo, _impl_.node_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.node_id_)}},
    // uint32 node_type = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(NodeInfo, _impl_.node_type_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.node_type_)}},
    // uint64 launch_time = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(NodeInfo, _impl_.launch_time_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.launch_time_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 node_id = 1;
    {PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.node_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint32 node_type = 2;
    {PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.node_type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint64 launch_time = 3;
    {PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.launch_time_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 game_node_type = 4;
    {PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.game_node_type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* NodeInfo::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:NodeInfo)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 node_id = 1;
  if (this->_internal_node_id() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        1, this->_internal_node_id(), target);
  }

  // uint32 node_type = 2;
  if (this->_internal_node_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_node_type(), target);
  }

  // uint64 launch_time = 3;
  if (this->_internal_launch_time() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        3, this->_internal_launch_time(), target);
  }

  // uint32 game_node_type = 4;
  if (this->_internal_game_node_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        4, this->_internal_game_node_type(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:NodeInfo)
  return target;
}

::size_t NodeInfo::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:NodeInfo)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // uint32 node_id = 1;
  if (this->_internal_node_id() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_node_id());
  }

  // uint32 node_type = 2;
  if (this->_internal_node_type() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_node_type());
  }

  // uint64 launch_time = 3;
  if (this->_internal_launch_time() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_launch_time());
  }

  // uint32 game_node_type = 4;
  if (this->_internal_game_node_type() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_game_node_type());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void NodeInfo::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<NodeInfo*>(&to_msg);
  auto& from = static_cast<const NodeInfo&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:NodeInfo)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_node_id() != 0) {
    _this->_impl_.node_id_ = from._impl_.node_id_;
  }
  if (from._internal_node_type() != 0) {
    _this->_impl_.node_type_ = from._impl_.node_type_;
  }
  if (from._internal_launch_time() != 0) {
    _this->_impl_.launch_time_ = from._impl_.launch_time_;
  }
  if (from._internal_game_node_type() != 0) {
    _this->_impl_.game_node_type_ = from._impl_.game_node_type_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void NodeInfo::CopyFrom(const NodeInfo& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:NodeInfo)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool NodeInfo::IsInitialized() const {
  return true;
}

void NodeInfo::InternalSwap(NodeInfo* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.game_node_type_)
      + sizeof(NodeInfo::_impl_.game_node_type_)
      - PROTOBUF_FIELD_OFFSET(NodeInfo, _impl_.node_id_)>(
          reinterpret_cast<char*>(&_impl_.node_id_),
          reinterpret_cast<char*>(&other->_impl_.node_id_));
}

::google::protobuf::Metadata NodeInfo::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_common_2fcommon_2eproto_getter,
                                   &descriptor_table_common_2fcommon_2eproto_once,
                                   file_level_metadata_common_2fcommon_2eproto[0]);
}
// ===================================================================

class NetworkAddress::_Internal {
 public:
};

NetworkAddress::NetworkAddress(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:NetworkAddress)
}
inline PROTOBUF_NDEBUG_INLINE NetworkAddress::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : ip_(arena, from.ip_),
        _cached_size_{0} {}

NetworkAddress::NetworkAddress(
    ::google::protobuf::Arena* arena,
    const NetworkAddress& from)
    : ::google::protobuf::Message(arena) {
  NetworkAddress* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  _impl_.port_ = from._impl_.port_;

  // @@protoc_insertion_point(copy_constructor:NetworkAddress)
}
inline PROTOBUF_NDEBUG_INLINE NetworkAddress::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : ip_(arena),
        _cached_size_{0} {}

inline void NetworkAddress::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.port_ = {};
}
NetworkAddress::~NetworkAddress() {
  // @@protoc_insertion_point(destructor:NetworkAddress)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void NetworkAddress::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.ip_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NetworkAddress::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(NetworkAddress, _impl_._cached_size_),
              false,
          },
          &NetworkAddress::MergeImpl,
          &NetworkAddress::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void NetworkAddress::Clear() {
// @@protoc_insertion_point(message_clear_start:NetworkAddress)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.ip_.ClearToEmpty();
  _impl_.port_ = 0u;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* NetworkAddress::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 25, 2> NetworkAddress::_table_ = {
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
    &_NetworkAddress_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::NetworkAddress>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 port = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(NetworkAddress, _impl_.port_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(NetworkAddress, _impl_.port_)}},
    // string ip = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(NetworkAddress, _impl_.ip_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string ip = 1;
    {PROTOBUF_FIELD_OFFSET(NetworkAddress, _impl_.ip_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // uint32 port = 2;
    {PROTOBUF_FIELD_OFFSET(NetworkAddress, _impl_.port_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
    "\16\2\0\0\0\0\0\0"
    "NetworkAddress"
    "ip"
  }},
};

::uint8_t* NetworkAddress::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:NetworkAddress)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string ip = 1;
  if (!this->_internal_ip().empty()) {
    const std::string& _s = this->_internal_ip();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "NetworkAddress.ip");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // uint32 port = 2;
  if (this->_internal_port() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_port(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:NetworkAddress)
  return target;
}

::size_t NetworkAddress::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:NetworkAddress)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string ip = 1;
  if (!this->_internal_ip().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_ip());
  }

  // uint32 port = 2;
  if (this->_internal_port() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_port());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void NetworkAddress::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<NetworkAddress*>(&to_msg);
  auto& from = static_cast<const NetworkAddress&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:NetworkAddress)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_ip().empty()) {
    _this->_internal_set_ip(from._internal_ip());
  }
  if (from._internal_port() != 0) {
    _this->_impl_.port_ = from._impl_.port_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void NetworkAddress::CopyFrom(const NetworkAddress& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:NetworkAddress)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool NetworkAddress::IsInitialized() const {
  return true;
}

void NetworkAddress::InternalSwap(NetworkAddress* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.ip_, &other->_impl_.ip_, arena);
        swap(_impl_.port_, other->_impl_.port_);
}

::google::protobuf::Metadata NetworkAddress::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_common_2fcommon_2eproto_getter,
                                   &descriptor_table_common_2fcommon_2eproto_once,
                                   file_level_metadata_common_2fcommon_2eproto[1]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_common_2fcommon_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"