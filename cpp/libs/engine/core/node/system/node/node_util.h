#pragma once
#include "proto/common/base/node.pb.h"
#include "entt/src/entt/entt.hpp"
#include <cstdint>

class NodeInfo;

namespace NodeUtils
{
	eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	std::string GetRegistryName(const entt::registry& registry);
	eNodeType GetRegistryType(const entt::registry& registry);
	bool IsSameNode(const std::string& uuid1, const std::string& uuid2);
	bool IsNodeConnected(uint32_t nodeType, const NodeInfo& info);
};

