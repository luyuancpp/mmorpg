#include "node_system.h"
#include <muduo/base/Logging.h>
#include "thread_local/storage.h"

eNodeType NodeSystem::GetServiceTypeFromPrefix(const std::string& prefix) {
	if (prefix.find("deploy") != std::string::npos) {
		return eNodeType::kDeployNode;
	}
	else if (prefix.find("scene") != std::string::npos) {
		return  eNodeType::kSceneNode;
	}
	else if (prefix.find("gate") != std::string::npos) {
		return eNodeType::kGateNode;
	}
	else if (prefix.find("centre") != std::string::npos) {
		return eNodeType::kCentreNode;
	}
	else if (prefix.find("login") != std::string::npos) {
		return eNodeType::kLoginNode;
	}
	else if (prefix.find("database") != std::string::npos) {
		return eNodeType::kDatabaseNode;
	}
	else {
		LOG_ERROR << "Unknown service type for prefix: " << prefix;
	}

	return eNodeType::kCentreNode;
}


entt::registry& NodeSystem::GetRegistryForNodeType(uint32_t nodeType) {
	switch (nodeType) {
	case kCentreNode:
		return tls.centreNodeRegistry;
	case kSceneNode:
		return tls.sceneNodeRegistry;
	case kGateNode:
		return tls.gateNodeRegistry;
	default:
		LOG_ERROR << "Unknown NodeType: " << static_cast<uint32_t>(nodeType);
	}

	return tls.invalidRegistry;
}
