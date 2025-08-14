#pragma once
#include <cstdint>
#include <muduo/net/TcpConnection.h>

#include <entt/src/entt/entity/registry.hpp>

class RegisterNodeSessionRequest;
class RegisterNodeSessionResponse;

class NodeRegistrationManager {
public:
	void TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const;

	void HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const;

	void HandleNodeRegistration(const RegisterNodeSessionRequest& request, RegisterNodeSessionResponse& response) const;

	void TriggerNodeConnectionEvent(entt::registry& registry, const RegisterNodeSessionResponse& response) const;
};