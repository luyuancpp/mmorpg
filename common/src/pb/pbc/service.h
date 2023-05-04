#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include <google/protobuf/message.h>

struct RpcService
{
    const char* service{nullptr};
    const char* method{nullptr};
    const char* request{nullptr};
    const char* response{nullptr};
};
extern std::unordered_map<uint32_t, RpcService> g_service_method_info;

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

void InitService();