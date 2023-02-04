#pragma once

#include "google/protobuf/message.h"

//与send 不同,route 是会在各个节点间传播消息,有最大值上限

void Route2Node(uint32_t note_type, const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
