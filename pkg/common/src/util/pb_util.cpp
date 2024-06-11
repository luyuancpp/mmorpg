#include "pb_util.h"

#include "google/protobuf/util/json_util.h"

#include "muduo/base/Logging.h"

std::string MessageToJsonString(const google::protobuf::Message& message)
{
    std::string json_str;
    if (const auto result = google::protobuf::util::MessageToJsonString(message,&json_str);
        !result.ok())
    {
        LOG_ERROR << "parse error" << message.DebugString();
    }
    return json_str;
}

std::string MessageToJsonString(const google::protobuf::Message* message)
{
    return MessageToJsonString(*message);
}