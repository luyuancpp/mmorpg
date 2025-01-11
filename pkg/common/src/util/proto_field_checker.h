#ifndef PROTO_FIELD_CHECKER_H
#define PROTO_FIELD_CHECKER_H

#include <google/protobuf/message.h>
#include <string>

class ProtoFieldChecker {
public:
    // 检查字段大小是否超出阈值
    static bool CheckFieldSizes(const google::protobuf::Message& message, size_t threshold, std::string& output);

    static bool CheckFieldSizes(const google::protobuf::Message& message, size_t threshold);

    static bool CheckForNegativeInts(const google::protobuf::Message& message, std::string& output);

    static bool CheckForNegativeInts(const google::protobuf::Message& message);

};

#endif // PROTO_FIELD_CHECKER_H
