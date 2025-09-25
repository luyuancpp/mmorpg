#pragma once
#include <memory>
#include <string>
#include <array>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include "entt/src/entt/entity/registry.hpp"

struct RpcService {
	const char* serviceName{nullptr};
	const char* methodName{nullptr};
	std::unique_ptr<::google::protobuf::Message> requestPrototype;
	std::unique_ptr<::google::protobuf::Message> responsePrototype;
	std::unique_ptr<::google::protobuf::Service> handlerInstance;
	uint32_t protocolType{0};
	uint32_t targetNodeType{0};
	std::function<void (
		entt::registry& registry,
		entt::entity nodeEntity,
		const google::protobuf::Message& message,
		const std::vector<std::string>& metaKeys,
		const std::vector<std::string>& metaValues
	)> messageSender;
};

constexpr uint32_t kMaxMessageLen = 84;

extern std::array<RpcService, kMaxMessageLen> gRpcServiceRegistry;
extern std::unordered_set<uint32_t> gClientMessageIdWhitelist;

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

void InitMessageInfo();
