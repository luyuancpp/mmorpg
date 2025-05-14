#include "node_system.h"
#include <muduo/base/Logging.h>
#include "thread_local/storage.h"

eNodeType NodeSystem::GetServiceTypeFromPrefix(const std::string& prefix) {
	if (prefix.find("deploy") != std::string::npos) {
		return eNodeType::DeployNodeService;
	}
	else if (prefix.find("scene") != std::string::npos) {
		return  eNodeType::SceneNodeService;
	}
	else if (prefix.find("gate") != std::string::npos) {
		return eNodeType::GateNodeService;
	}
	else if (prefix.find("centre") != std::string::npos) {
		return eNodeType::CentreNodeService;
	}
	else if (prefix.find("login") != std::string::npos) {
		return eNodeType::LoginNodeServcie;
	}
	else if (prefix.find("database") != std::string::npos) {
		return eNodeType::DatabaseNodeService;
	}
	else {
		LOG_ERROR << "Unknown service type for prefix: " << prefix;
	}

	return eNodeType::CentreNodeService;
}


entt::registry& NodeSystem::GetRegistryForNodeType(uint32_t nodeType) {
	switch (nodeType) {
	case CentreNodeService:
		return tls.centreNodeRegistry;
	case SceneNodeService:
		return tls.sceneNodeRegistry;
	case GateNodeService:
		return tls.gateNodeRegistry;
	default:
		LOG_ERROR << "Unknown NodeType: " << static_cast<uint32_t>(nodeType);
	}

	return tls.invalidRegistry;
}
