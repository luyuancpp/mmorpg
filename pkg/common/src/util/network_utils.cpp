#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>
#include "proto/common/node.pb.h"
#include "proto/common/session.pb.h"
#include "thread_local/storage.h"

NodeId GetGateNodeId(Guid session_id)
{
	return static_cast<NodeId>(session_id >> SessionIdGenerator::node_bit());
}

uint16_t get_available_port(uint16_t start_port, uint16_t max_port) {
	boost::asio::io_context io_context;  // 使用 io_context 替代 io_service
	boost::asio::ip::tcp::acceptor acceptor(io_context);

	for (int port = start_port; port <= max_port; ++port) {
		try {
			boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
			acceptor.open(endpoint.protocol());
			acceptor.bind(endpoint);
			acceptor.listen();

			acceptor.close();
			return port;
		}
		catch (const boost::system::system_error& e) {
			std::cerr << "Error: " << e.what() << std::endl;  // 使用 e 来打印异常的描述信息
			continue;
		}
	}

	std::cerr << "No available port found in the given range!" << std::endl;
	return -1;  // 返回 -1 表示没有找到可用端口
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
	case DeployNodeService:
	case DbNodeService:
	case CentreNodeService:
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
