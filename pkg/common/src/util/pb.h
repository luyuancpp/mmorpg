#pragma once

#include "google/protobuf/message.h"

std::string MessageToJsonString(const google::protobuf::Message& message);
std::string MessageToJsonString(const google::protobuf::Message* message);