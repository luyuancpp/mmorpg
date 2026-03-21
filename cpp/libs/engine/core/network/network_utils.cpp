#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>
#include "proto/common/base/node.pb.h"
#include "proto/common/base/session.pb.h"


NodeId GetGateNodeId(Guid session_id)
{
	return static_cast<NodeId>(session_id >> SessionIdGenerator::node_bit());
}

void ParseIpPort(const std::string& input, std::string& ip, uint16_t& port) {
	// 将输入字符串解析成 IP 地址和端口
	std::istringstream ss(input);
	std::string host;

	// 使用 ':' 分隔 IP 和端口
	std::getline(ss, host, ':');
	ip = host;  // 提取 IP 地址部分

	// 获取端口
	std::getline(ss, host);
	port = std::stoi(host);  // 转换为整数端口
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

		// 非 zone-singleton 的 nodeType：
	case LoginNodeService:
	case SceneNodeService:
	case GateNodeService:
		return false;

	default:
		// 明确没有列出的类型，统一默认 false，防止未来添加类型误判
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
