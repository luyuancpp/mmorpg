#include "proto_util.h"
#include <unordered_set>
#include "proto/common/node.pb.h"

bool IsTcpNodeType(uint32_t nodeType) {
	static const std::unordered_set<uint32_t> validTypes = {
		eNodeType::DeployNodeService,
		eNodeType::CentreNodeService,
		eNodeType::SceneNodeService,
		eNodeType::GateNodeService,
		eNodeType::RedisNodeService,
		eNodeType::EtcdNodeService,
		eNodeType::ActivityNodeService,
		eNodeType::TradeNodeService,
		eNodeType::RankNodeService,
		eNodeType::TaskNodeService,
		eNodeType::GuildNodeService,
		eNodeType::MatchNodeService,
		eNodeType::PaymentNodeService,
		eNodeType::SecurityNodeService,
		eNodeType::CrossServerNodeService,
		eNodeType::AnalyticsNodeService,
		eNodeType::GmNodeService,
		eNodeType::PlayerLocatorNodeService,
	};

	return validTypes.contains(nodeType);
}