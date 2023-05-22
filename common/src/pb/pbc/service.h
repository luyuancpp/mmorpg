#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

struct RpcService
{
    const char* service{nullptr};
    const char* method{nullptr};
    const char* request{nullptr};
    const char* response{nullptr};
    std::unique_ptr<::google::protobuf::Service> service_impl_instance_;
};
extern std::unordered_map<uint32_t, RpcService> g_message_info;

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

void InitService();