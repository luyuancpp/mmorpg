#pragma once

#include "google/protobuf/message.h"

//与send 不同,route 是会在各个节点间传播消息,有最大值上限

void Route2Db(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
void Route2Controller(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
void Route2Gate(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);