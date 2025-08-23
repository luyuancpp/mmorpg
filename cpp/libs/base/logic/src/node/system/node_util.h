#pragma once
#include "proto/common/node.pb.h"
#include "entt/src/entt/entt.hpp"
#include <cstdint>

class NodeInfo;

class NodeUtils
{
public:
	static eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	static entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	static std::string GetRegistryName(const entt::registry& registry);
	static eNodeType GetRegistryType(const entt::registry& registry);
	static bool IsSameNode(const std::string& uuid1, const std::string& uuid2);
	static bool IsNodeConnected(uint32_t nodeType, const NodeInfo& info);
};

