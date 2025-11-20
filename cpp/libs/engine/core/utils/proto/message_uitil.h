#pragma once

#include <google/protobuf/message.h>
void copyCommonFields(const google::protobuf::Message& src, google::protobuf::Message& dest);