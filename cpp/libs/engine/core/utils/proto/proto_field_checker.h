#ifndef PROTO_FIELD_CHECKER_H
#define PROTO_FIELD_CHECKER_H

#include <google/protobuf/message.h>
#include <string>

class ProtoFieldChecker {
public:
    // Check if repeated field sizes exceed threshold
    static bool CheckFieldSizes(const google::protobuf::Message& message, size_t threshold, std::string& output);

    static bool CheckFieldSizes(const google::protobuf::Message& message, size_t threshold);

    static bool CheckForNegativeInts(const google::protobuf::Message& message, std::string& output);

    static bool CheckForNegativeInts(const google::protobuf::Message& message);

};

#endif // PROTO_FIELD_CHECKER_H
