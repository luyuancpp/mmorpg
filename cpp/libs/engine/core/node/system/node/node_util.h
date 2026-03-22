#pragma once
#include "proto/common/base/node.pb.h"
#include "proto/common/base/common.pb.h"
#include "entt/src/entt/entt.hpp"
#include <cstdint>

// Re-export common::base types at global scope for backward compatibility.
using common::base::eNodeType;
using common::base::eSceneNodeType;
using common::base::eNodeProtocolType;
using common::base::NodeInfo;
using common::base::NodeInfoListPBComponent;
using common::base::eNodeType_ARRAYSIZE;
using common::base::eNodeType_Name;
using common::base::eNodeType_IsValid;
using common::base::eSceneNodeType_ARRAYSIZE;
using common::base::eSceneNodeType_Name;

namespace NodeUtils
{
	eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
	entt::registry& GetRegistryForNodeType(uint32_t nodeType);
	std::string GetRegistryName(const entt::registry& registry);
	eNodeType GetRegistryType(const entt::registry& registry);
	bool IsSameNode(const std::string& uuid1, const std::string& uuid2);
	bool IsNodeConnected(uint32_t nodeType, const NodeInfo& info);
};

