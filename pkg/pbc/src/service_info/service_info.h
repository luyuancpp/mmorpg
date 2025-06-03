#pragma once
#include <memory>
#include <string>
#include <array>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

struct RpcService {
	const char* serviceName{nullptr};
	const char* methodName{nullptr};
	std::unique_ptr<::google::protobuf::Message> request;
	std::unique_ptr<::google::protobuf::Message> response;
	std::unique_ptr<::google::protobuf::Service> serviceImplInstance;
	uint32_t protocolType{0};
	uint32_t nodeType{0};
};

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

void InitMessageInfo();

constexpr uint32_t kMaxMessageLen = 85;

extern std::array<RpcService, kMaxMessageLen> gRpcServiceByMessageId;

extern std::unordered_set<uint32_t> gAllowedClientMessageIds;
