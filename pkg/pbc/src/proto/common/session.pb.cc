// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/common/session.proto
// Protobuf C++ Version: 5.29.0

#include "proto/common/session.pb.h"

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

inline constexpr SessionDetails::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : session_id_{::uint64_t{0u}},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR SessionDetails::SessionDetails(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct SessionDetailsDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SessionDetailsDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SessionDetailsDefaultTypeInternal() {}
  union {
    SessionDetails _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SessionDetailsDefaultTypeInternal _SessionDetails_default_instance_;
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2fcommon_2fsession_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2fcommon_2fsession_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2fcommon_2fsession_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::SessionDetails, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::SessionDetails, _impl_.session_id_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::SessionDetails)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_SessionDetails_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2fcommon_2fsession_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\032proto/common/session.proto\"$\n\016SessionD"
    "etails\022\022\n\nsession_id\030\001 \001(\004B\tZ\007pb/gameb\006p"
    "roto3"
};
static ::absl::once_flag descriptor_table_proto_2fcommon_2fsession_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2fcommon_2fsession_2eproto = {
    false,
    false,
    85,
    descriptor_table_protodef_proto_2fcommon_2fsession_2eproto,
    "proto/common/session.proto",
    &descriptor_table_proto_2fcommon_2fsession_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2fcommon_2fsession_2eproto::offsets,
    file_level_enum_descriptors_proto_2fcommon_2fsession_2eproto,
    file_level_service_descriptors_proto_2fcommon_2fsession_2eproto,
};
// ===================================================================

class SessionDetails::_Internal {
 public:
};

SessionDetails::SessionDetails(::google::protobuf::Arena* arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, _class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:SessionDetails)
}
SessionDetails::SessionDetails(
    ::google::protobuf::Arena* arena, const SessionDetails& from)
    : SessionDetails(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE SessionDetails::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void SessionDetails::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.session_id_ = {};
}
SessionDetails::~SessionDetails() {
  // @@protoc_insertion_point(destructor:SessionDetails)
  SharedDtor(*this);
}
inline void SessionDetails::SharedDtor(MessageLite& self) {
  SessionDetails& this_ = static_cast<SessionDetails&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* SessionDetails::PlacementNew_(const void*, void* mem,
                                        ::google::protobuf::Arena* arena) {
  return ::new (mem) SessionDetails(arena);
}
constexpr auto SessionDetails::InternalNewImpl_() {
  return ::google::protobuf::internal::MessageCreator::ZeroInit(sizeof(SessionDetails),
                                            alignof(SessionDetails));
}
PROTOBUF_CONSTINIT
PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::google::protobuf::internal::ClassDataFull SessionDetails::_class_data_ = {
    ::google::protobuf::internal::ClassData{
        &_SessionDetails_default_instance_._instance,
        &_table_.header,
        nullptr,  // OnDemandRegisterArenaDtor
        nullptr,  // IsInitialized
        &SessionDetails::MergeImpl,
        ::google::protobuf::Message::GetNewImpl<SessionDetails>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
        &SessionDetails::SharedDtor,
        ::google::protobuf::Message::GetClearImpl<SessionDetails>(), &SessionDetails::ByteSizeLong,
            &SessionDetails::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
        PROTOBUF_FIELD_OFFSET(SessionDetails, _impl_._cached_size_),
        false,
    },
    &SessionDetails::kDescriptorMethods,
    &descriptor_table_proto_2fcommon_2fsession_2eproto,
    nullptr,  // tracker
};
const ::google::protobuf::internal::ClassData* SessionDetails::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_class_data_.tc_table);
  return _class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> SessionDetails::_table_ = {
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
    _class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::SessionDetails>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint64 session_id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(SessionDetails, _impl_.session_id_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(SessionDetails, _impl_.session_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint64 session_id = 1;
    {PROTOBUF_FIELD_OFFSET(SessionDetails, _impl_.session_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
  }},
  // no aux_entries
  {{
  }},
};

PROTOBUF_NOINLINE void SessionDetails::Clear() {
// @@protoc_insertion_point(message_clear_start:SessionDetails)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.session_id_ = ::uint64_t{0u};
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::uint8_t* SessionDetails::_InternalSerialize(
            const MessageLite& base, ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) {
          const SessionDetails& this_ = static_cast<const SessionDetails&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::uint8_t* SessionDetails::_InternalSerialize(
            ::uint8_t* target,
            ::google::protobuf::io::EpsCopyOutputStream* stream) const {
          const SessionDetails& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(serialize_to_array_start:SessionDetails)
          ::uint32_t cached_has_bits = 0;
          (void)cached_has_bits;

          // uint64 session_id = 1;
          if (this_._internal_session_id() != 0) {
            target = stream->EnsureSpace(target);
            target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
                1, this_._internal_session_id(), target);
          }

          if (PROTOBUF_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
            target =
                ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
                    this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
          }
          // @@protoc_insertion_point(serialize_to_array_end:SessionDetails)
          return target;
        }

#if defined(PROTOBUF_CUSTOM_VTABLE)
        ::size_t SessionDetails::ByteSizeLong(const MessageLite& base) {
          const SessionDetails& this_ = static_cast<const SessionDetails&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
        ::size_t SessionDetails::ByteSizeLong() const {
          const SessionDetails& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
          // @@protoc_insertion_point(message_byte_size_start:SessionDetails)
          ::size_t total_size = 0;

          ::uint32_t cached_has_bits = 0;
          // Prevent compiler warnings about cached_has_bits being unused
          (void)cached_has_bits;

           {
            // uint64 session_id = 1;
            if (this_._internal_session_id() != 0) {
              total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
                  this_._internal_session_id());
            }
          }
          return this_.MaybeComputeUnknownFieldsSize(total_size,
                                                     &this_._impl_._cached_size_);
        }

void SessionDetails::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<SessionDetails*>(&to_msg);
  auto& from = static_cast<const SessionDetails&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:SessionDetails)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_session_id() != 0) {
    _this->_impl_.session_id_ = from._impl_.session_id_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void SessionDetails::CopyFrom(const SessionDetails& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:SessionDetails)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void SessionDetails::InternalSwap(SessionDetails* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
        swap(_impl_.session_id_, other->_impl_.session_id_);
}

::google::protobuf::Metadata SessionDetails::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2fcommon_2fsession_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
