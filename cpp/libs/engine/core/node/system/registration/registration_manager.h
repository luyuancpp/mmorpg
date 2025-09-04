#pragma once
#include <cstdint>
#include <muduo/net/TcpConnection.h>

#include <entt/src/entt/entity/registry.hpp>

class HandshakeRequest;
class HandshakeResponse;

class NodeRegistrationManager {
public:
	void TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const;

	void OnHandshakeReplied(const HandshakeResponse& response) const;

	void OnNodeHandshake(const HandshakeRequest& request, HandshakeResponse& response) const;

	void TriggerNodeConnectionEvent(entt::registry& registry, const HandshakeResponse& response) const;
};