#pragma once
#include <cstdint>
#include <muduo/net/TcpConnection.h>

#include <entt/src/entt/entity/registry.hpp>

class NodeHandshakeRequest;
class NodeHandshakeResponse;

class NodeHandshakeManager {
public:
	void TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const;

	void OnHandshakeReplied(const NodeHandshakeResponse& response) const;

	void OnNodeHandshake(const NodeHandshakeRequest& request, NodeHandshakeResponse& response) const;

	void TriggerNodeConnectionEvent(entt::registry& registry, const NodeHandshakeResponse& response) const;
};