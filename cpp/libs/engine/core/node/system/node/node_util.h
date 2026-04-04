#pragma once
#include "proto/common/base/node.pb.h"
#include "proto/common/base/common.pb.h"
#include "entt/src/entt/entt.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

// Re-export common::base types at global scope for backward compatibility.
using common::base::eNodeProtocolType;
using common::base::eNodeType;
using common::base::eSceneNodeType;

using common::base::eNodeType_ARRAYSIZE;
using common::base::eNodeType_IsValid;
using common::base::eNodeType_Name;
using common::base::eSceneNodeType_ARRAYSIZE;
using common::base::eSceneNodeType_Name;

// Re-export eNodeType enumerator values.
using common::base::ActivityNodeService;
using common::base::AiNodeService;
using common::base::AnalyticsNodeService;
using common::base::ChatNodeService;
using common::base::CrossServerNodeService;
using common::base::DataServiceNodeService;
using common::base::EtcdNodeService;
using common::base::FriendNodeService;
using common::base::GateNodeService;
using common::base::GmNodeService;
using common::base::GuildNodeService;
using common::base::LoginNodeService;
using common::base::LogNodeService;
using common::base::MailNodeService;
using common::base::MatchNodeService;
using common::base::PaymentNodeService;
using common::base::PlayerLocatorNodeService;
using common::base::RankNodeService;
using common::base::RedisNodeService;
using common::base::SceneManagerNodeService;
using common::base::SceneNodeService;
using common::base::SecurityNodeService;
using common::base::TaskNodeService;
using common::base::TeamNodeService;
using common::base::TradeNodeService;
using common::base::UnknownNodeService;

// Re-export eSceneNodeType enumerator values.
using common::base::kMainSceneCrossNode;
using common::base::kMainSceneNode;
using common::base::kSceneNode;
using common::base::kSceneSceneCrossNode;

// Re-export eNodeProtocolType enumerator values.
using common::base::PROTOCOL_GRPC;
using common::base::PROTOCOL_HTTP;
using common::base::PROTOCOL_TCP;

namespace NodeUtils
{
	// GateNodeService -> "gate", SceneNodeService -> "scene", etc.
	inline std::string NodeTypeToShortName(uint32_t nodeType)
	{
		std::string name = eNodeType_Name(static_cast<eNodeType>(nodeType));
		const std::string suffix = "NodeService";
		if (name.size() > suffix.size() &&
			name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		{
			name.resize(name.size() - suffix.size());
		}
		std::transform(name.begin(), name.end(), name.begin(),
					   [](unsigned char c)
					   { return static_cast<char>(std::tolower(c)); });
		return name;
	}

	eNodeType GetServiceTypeFromPrefix(const std::string &prefix);
	entt::registry &GetRegistryForNodeType(uint32_t nodeType);
	std::string GetRegistryName(const entt::registry &registry);
	eNodeType GetRegistryType(const entt::registry &registry);
	bool IsSameNode(const std::string &uuid1, const std::string &uuid2);
	bool IsNodeConnected(uint32_t nodeType, const NodeInfo &info);
};
