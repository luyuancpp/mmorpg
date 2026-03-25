#pragma once

#include <google/protobuf/message.h>

/// Copies matching singular fields from src to dst by field name.
/// Nested message fields are deep-copied via CopyFrom.
void CopyCommonFields(const google::protobuf::Message& src, google::protobuf::Message& dest);