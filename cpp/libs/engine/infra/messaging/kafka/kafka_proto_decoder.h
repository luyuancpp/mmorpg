#pragma once

#include <muduo/base/Logging.h>

#include <memory>
#include <string>

template <typename ProtoT>
std::shared_ptr<ProtoT> DecodeKafkaProtoPayload(const std::string& topic, const std::string& payload)
{
    if (payload.empty()) {
        LOG_WARN << "Empty Kafka payload from topic " << topic;
        return nullptr;
    }

    auto message = std::make_shared<ProtoT>();
    if (!message->ParseFromString(payload)) {
        LOG_ERROR << "Failed to parse Kafka payload as " << std::string(message->GetTypeName())
                  << " from topic " << topic;
        return nullptr;
    }
    return message;
}
