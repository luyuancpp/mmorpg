#pragma once

#include "google/protobuf/message.h"

//��send ��ͬ,route �ǻ��ڸ����ڵ�䴫����Ϣ,�����ֵ����

void Route2Node(uint32_t note_type, const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method);
