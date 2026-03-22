#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>
#include "node/system/node/node_util.h"
#include "proto/common/base/session.pb.h"


NodeId GetGateNodeId(Guid session_id)
{
	return static_cast<NodeId>(session_id >> SessionIdGenerator::node_bit());
}

void ParseIpPort(const std::string& input, std::string& ip, uint16_t& port) {
	std::istringstream ss(input);
	std::string host;

	std::getline(ss, host, ':');
	ip = host;

	std::getline(ss, host);
	port = std::stoi(host);
}


std::string FormatIpAndPort(const std::string& ip, uint32_t port)
{
	return ip + ":" + std::to_string(port);
}


bool IsZoneSingletonNodeType(uint32_t nodeType) {
	switch (nodeType) {
	case RedisNodeService:
	case EtcdNodeService:
	case MailNodeService:
	case ChatNodeService:
	case TeamNodeService:
	case ActivityNodeService:
	case TradeNodeService:
	case RankNodeService:
	case TaskNodeService:
	case GuildNodeService:
	case MatchNodeService:
	case AiNodeService:
	case LogNodeService:
	case PaymentNodeService:
	case SecurityNodeService:
	case CrossServerNodeService:
	case AnalyticsNodeService:
	case GmNodeService:
		return true;

		// Non zone-singleton node types:
	case LoginNodeService:
	case SceneNodeService:
	case GateNodeService:
		return false;

	default:
		// Default false for unlisted types to avoid false positives from future additions
		return false;
	}
}

std::vector<std::string> SerializeSessionDetails(const SessionDetails& sessionDetails) {
	std::vector<std::string> result;
	std::string serialized;
	if (sessionDetails.SerializeToString(&serialized)) {
		result.push_back(std::move(serialized));
	}
	return result;
}
