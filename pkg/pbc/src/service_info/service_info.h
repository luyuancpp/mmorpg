#pragma once
#include <memory>
#include <string>
#include <array>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

struct RpcService
{
	const char* serviceName{nullptr};
	const char* methodName{nullptr};
	const char* request{nullptr};
	const char* response{nullptr};
	std::unique_ptr<::google::protobuf::Service> serviceImplInstance;
};

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

void InitMessageInfo();

constexpr uint32_t kMaxMessageLen = 89;

extern std::array<RpcService, kMaxMessageLen> gMessageInfo;

extern std::unordered_set<uint32_t> gClientToServerMessageId;
