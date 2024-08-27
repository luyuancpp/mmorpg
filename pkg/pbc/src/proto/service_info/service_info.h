#pragma once
#include <memory>
#include <string>
#include <array>
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

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

void InitMessageInfo();

constexpr uint32_t kMaxMessageLen = 76;

extern std::array<RpcService, kMaxMessageLen> g_message_info;

extern std::unordered_set<uint32_t> g_c2s_service_id;
