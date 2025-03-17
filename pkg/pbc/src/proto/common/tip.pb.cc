// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: proto/common/tip.proto
// Protobuf C++ Version: 6.31.0-dev

#include "proto/common/tip.pb.h"

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

inline constexpr TipInfoMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        parameters_{},
        id_{0u} {}

template <typename>
PROTOBUF_CONSTEXPR TipInfoMessage::TipInfoMessage(::_pbi::ConstantInitialized)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(TipInfoMessage_class_data_.base()),
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(),
#endif  // PROTOBUF_CUSTOM_VTABLE
      _impl_(::_pbi::ConstantInitialized()) {
}
struct TipInfoMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR TipInfoMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~TipInfoMessageDefaultTypeInternal() {}
  union {
    TipInfoMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 TipInfoMessageDefaultTypeInternal _TipInfoMessage_default_instance_;
static constexpr const ::_pb::EnumDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_enum_descriptors_proto_2fcommon_2ftip_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor *PROTOBUF_NONNULL *PROTOBUF_NULLABLE
    file_level_service_descriptors_proto_2fcommon_2ftip_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2fcommon_2ftip_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        0x081, // bitmap
        PROTOBUF_FIELD_OFFSET(::TipInfoMessage, _impl_._has_bits_),
        5, // hasbit index offset
        PROTOBUF_FIELD_OFFSET(::TipInfoMessage, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::TipInfoMessage, _impl_.parameters_),
        0,
        ~0u,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, sizeof(::TipInfoMessage)},
};
static const ::_pb::Message* PROTOBUF_NONNULL const file_default_instances[] = {
    &::_TipInfoMessage_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2fcommon_2ftip_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\026proto/common/tip.proto\"0\n\016TipInfoMessa"
    "ge\022\n\n\002id\030\001 \001(\r\022\022\n\nparameters\030\002 \003(\tB\tZ\007pb"
    "/gameb\006proto3"
};
static ::absl::once_flag descriptor_table_proto_2fcommon_2ftip_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_proto_2fcommon_2ftip_2eproto = {
    false,
    false,
    93,
    descriptor_table_protodef_proto_2fcommon_2ftip_2eproto,
    "proto/common/tip.proto",
    &descriptor_table_proto_2fcommon_2ftip_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_proto_2fcommon_2ftip_2eproto::offsets,
    file_level_enum_descriptors_proto_2fcommon_2ftip_2eproto,
    file_level_service_descriptors_proto_2fcommon_2ftip_2eproto,
};
// ===================================================================

class TipInfoMessage::_Internal {
 public:
  using HasBits =
      decltype(std::declval<TipInfoMessage>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_._has_bits_);
};

TipInfoMessage::TipInfoMessage(::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, TipInfoMessage_class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:TipInfoMessage)
}
PROTOBUF_NDEBUG_INLINE TipInfoMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena, const Impl_& from,
    const ::TipInfoMessage& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        parameters_{visibility, arena, from.parameters_} {}

TipInfoMessage::TipInfoMessage(
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena,
    const TipInfoMessage& from)
#if defined(PROTOBUF_CUSTOM_VTABLE)
    : ::google::protobuf::Message(arena, TipInfoMessage_class_data_.base()) {
#else   // PROTOBUF_CUSTOM_VTABLE
    : ::google::protobuf::Message(arena) {
#endif  // PROTOBUF_CUSTOM_VTABLE
  TipInfoMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.id_ = from._impl_.id_;

  // @@protoc_insertion_point(copy_constructor:TipInfoMessage)
}
PROTOBUF_NDEBUG_INLINE TipInfoMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena)
      : _cached_size_{0},
        parameters_{visibility, arena} {}

inline void TipInfoMessage::SharedCtor(::_pb::Arena* PROTOBUF_NULLABLE arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.id_ = {};
}
TipInfoMessage::~TipInfoMessage() {
  // @@protoc_insertion_point(destructor:TipInfoMessage)
  SharedDtor(*this);
}
inline void TipInfoMessage::SharedDtor(MessageLite& self) {
  TipInfoMessage& this_ = static_cast<TipInfoMessage&>(self);
  this_._internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  ABSL_DCHECK(this_.GetArena() == nullptr);
  this_._impl_.~Impl_();
}

inline void* PROTOBUF_NONNULL TipInfoMessage::PlacementNew_(
    const void* PROTOBUF_NONNULL, void* PROTOBUF_NONNULL mem,
    ::google::protobuf::Arena* PROTOBUF_NULLABLE arena) {
  return ::new (mem) TipInfoMessage(arena);
}
constexpr auto TipInfoMessage::InternalNewImpl_() {
  constexpr auto arena_bits = ::google::protobuf::internal::EncodePlacementArenaOffsets({
      PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_.parameters_) +
          decltype(TipInfoMessage::_impl_.parameters_)::
              InternalGetArenaOffset(
                  ::google::protobuf::Message::internal_visibility()),
  });
  if (arena_bits.has_value()) {
    return ::google::protobuf::internal::MessageCreator::ZeroInit(
        sizeof(TipInfoMessage), alignof(TipInfoMessage), *arena_bits);
  } else {
    return ::google::protobuf::internal::MessageCreator(&TipInfoMessage::PlacementNew_,
                                 sizeof(TipInfoMessage),
                                 alignof(TipInfoMessage));
  }
}
constexpr auto TipInfoMessage::InternalGenerateClassData_() {
  return ::google::protobuf::internal::ClassDataFull{
      ::google::protobuf::internal::ClassData{
          &_TipInfoMessage_default_instance_._instance,
          &_table_.header,
          nullptr,  // OnDemandRegisterArenaDtor
          nullptr,  // IsInitialized
          &TipInfoMessage::MergeImpl,
          ::google::protobuf::Message::GetNewImpl<TipInfoMessage>(),
#if defined(PROTOBUF_CUSTOM_VTABLE)
          &TipInfoMessage::SharedDtor,
          ::google::protobuf::Message::GetClearImpl<TipInfoMessage>(), &TipInfoMessage::ByteSizeLong,
              &TipInfoMessage::_InternalSerialize,
#endif  // PROTOBUF_CUSTOM_VTABLE
          PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_._cached_size_),
          false,
      },
      &TipInfoMessage::kDescriptorMethods,
      &descriptor_table_proto_2fcommon_2ftip_2eproto,
      nullptr,  // tracker
  };
}

PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 const ::google::protobuf::internal::ClassDataFull
        TipInfoMessage_class_data_ =
            TipInfoMessage::InternalGenerateClassData_();

const ::google::protobuf::internal::ClassData* PROTOBUF_NONNULL TipInfoMessage::GetClassData() const {
  ::google::protobuf::internal::PrefetchToLocalCache(&TipInfoMessage_class_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(TipInfoMessage_class_data_.tc_table);
  return TipInfoMessage_class_data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 33, 2>
TipInfoMessage::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    TipInfoMessage_class_data_.base(),
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::TipInfoMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated string parameters = 2;
    {::_pbi::TcParser::FastUR1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_.parameters_)}},
    // uint32 id = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(TipInfoMessage, _impl_.id_), 0>(),
     {8, 0, 0, PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // uint32 id = 1;
    {PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_.id_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kUInt32)},
    // repeated string parameters = 2;
    {PROTOBUF_FIELD_OFFSET(TipInfoMessage, _impl_.parameters_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
  }},
  // no aux_entries
  {{
    "\16\0\12\0\0\0\0\0"
    "TipInfoMessage"
    "parameters"
  }},
};
PROTOBUF_NOINLINE void TipInfoMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:TipInfoMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.parameters_.Clear();
  _impl_.id_ = 0u;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::uint8_t* PROTOBUF_NONNULL TipInfoMessage::_InternalSerialize(
    const ::google::protobuf::MessageLite& base, ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) {
  const TipInfoMessage& this_ = static_cast<const TipInfoMessage&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::uint8_t* PROTOBUF_NONNULL TipInfoMessage::_InternalSerialize(
    ::uint8_t* PROTOBUF_NONNULL target,
    ::google::protobuf::io::EpsCopyOutputStream* PROTOBUF_NONNULL stream) const {
  const TipInfoMessage& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(serialize_to_array_start:TipInfoMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // uint32 id = 1;
  if ((this_._impl_._has_bits_[0] & 0x00000001u) != 0) {
    if (this_._internal_id() != 0) {
      target = stream->EnsureSpace(target);
      target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
          1, this_._internal_id(), target);
    }
  }

  // repeated string parameters = 2;
  for (int i = 0, n = this_._internal_parameters_size(); i < n; ++i) {
    const auto& s = this_._internal_parameters().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "TipInfoMessage.parameters");
    target = stream->WriteString(2, s, target);
  }

  if (ABSL_PREDICT_FALSE(this_._internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            this_._internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:TipInfoMessage)
  return target;
}

#if defined(PROTOBUF_CUSTOM_VTABLE)
::size_t TipInfoMessage::ByteSizeLong(const MessageLite& base) {
  const TipInfoMessage& this_ = static_cast<const TipInfoMessage&>(base);
#else   // PROTOBUF_CUSTOM_VTABLE
::size_t TipInfoMessage::ByteSizeLong() const {
  const TipInfoMessage& this_ = *this;
#endif  // PROTOBUF_CUSTOM_VTABLE
  // @@protoc_insertion_point(message_byte_size_start:TipInfoMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void)cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(&this_);
   {
    // repeated string parameters = 2;
    {
      total_size +=
          1 * ::google::protobuf::internal::FromIntSize(this_._internal_parameters().size());
      for (int i = 0, n = this_._internal_parameters().size(); i < n; ++i) {
        total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
            this_._internal_parameters().Get(i));
      }
    }
  }
   {
    // uint32 id = 1;
    cached_has_bits = this_._impl_._has_bits_[0];
    if ((cached_has_bits & 0x00000001u) != 0) {
      if (this_._internal_id() != 0) {
        total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
            this_._internal_id());
      }
    }
  }
  return this_.MaybeComputeUnknownFieldsSize(total_size,
                                             &this_._impl_._cached_size_);
}

void TipInfoMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<TipInfoMessage*>(&to_msg);
  auto& from = static_cast<const TipInfoMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:TipInfoMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_parameters()->MergeFrom(from._internal_parameters());
  cached_has_bits = from._impl_._has_bits_[0];
  if ((cached_has_bits & 0x00000001u) != 0) {
    if (from._internal_id() != 0) {
      _this->_impl_.id_ = from._impl_.id_;
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void TipInfoMessage::CopyFrom(const TipInfoMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:TipInfoMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void TipInfoMessage::InternalSwap(TipInfoMessage* PROTOBUF_RESTRICT PROTOBUF_NONNULL other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.parameters_.InternalSwap(&other->_impl_.parameters_);
  swap(_impl_.id_, other->_impl_.id_);
}

::google::protobuf::Metadata TipInfoMessage::GetMetadata() const {
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
        (::_pbi::AddDescriptors(&descriptor_table_proto_2fcommon_2ftip_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
