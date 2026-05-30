#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>
#include "muduo/base/Logging.h"
#include "network/node_utils.h"
#include "node/system/node/node_util.h"
#include "proto/common/base/session.pb.h"
#include "thread_context/node_context_manager.h"

NodeId GetGateNodeId(SessionId session_id)
{
	return static_cast<NodeId>(session_id >> SessionIdGenerator::node_bit());
}

std::optional<entt::entity> ResolveLocalZoneGateEntity(SessionId sessionId)
{
	const auto zoneId = GetZoneId();
	const auto gateNodeId = GetGateNodeId(sessionId);
	if (auto entity = NodeUtils::FindNodeEntityByZoneAndNodeId(
		eNodeType::GateNodeService,
		zoneId,
		gateNodeId))
	{
		return entity;
	}

	// Defensive fallback for local single-zone/dev launches where the caller's
	// thread-local zone context can lag node discovery. Only accept an
	// unscoped match when it is unique, otherwise cross-zone ambiguity remains
	// a hard miss.
	auto &registry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	std::optional<entt::entity> uniqueMatch;
	size_t matches = 0;
	for (const auto &[entity, nodeInfo] : registry.view<NodeInfo>().each())
	{
		if (nodeInfo.node_id() == gateNodeId)
		{
			uniqueMatch = entity;
			++matches;
		}
	}
	if (matches == 1)
	{
		LOG_WARN << "ResolveLocalZoneGateEntity fallback by node_id, session_id="
				 << sessionId << ", gate_node_id=" << gateNodeId
				 << ", zone_id=" << zoneId;
		return uniqueMatch;
	}
	return std::nullopt;
}

void ParseIpPort(const std::string &input, std::string &ip, uint16_t &port)
{
	std::istringstream ss(input);
	std::string host;

	std::getline(ss, host, ':');
	ip = host;

	std::getline(ss, host);
	port = std::stoi(host);
}

std::string FormatIpAndPort(const std::string &ip, uint32_t port)
{
	return ip + ":" + std::to_string(port);
}

std::vector<std::string> SerializeSessionDetails(const SessionDetails &sessionDetails)
{
	std::vector<std::string> result;
	std::string serialized;
	if (sessionDetails.SerializeToString(&serialized))
	{
		result.push_back(std::move(serialized));
	}
	return result;
}
