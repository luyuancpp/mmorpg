#pragma once
#include "proto/common/node.pb.h"
#include "entt/src/entt/entt.hpp"

class NodeInfo;

class NodeSystem
{
public:
	static eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	static entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	static std::string GetRegistryName(const entt::registry& registry);
	static eNodeType GetRegistryType(const entt::registry& registry);
	static NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType);

};

