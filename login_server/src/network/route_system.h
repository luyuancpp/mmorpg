#pragma once

#include "google/protobuf/message.h"

//��send ��ͬ,route �ǻ��ڸ����ڵ�䴫����Ϣ,�����ֵ����

void Route2Db(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
void Route2Controller(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
void Route2Gate(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);