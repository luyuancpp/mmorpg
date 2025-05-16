#include "node_system.h"
#include <muduo/base/Logging.h>
#include "thread_local/storage.h"

// 静态映射表定义（可放在 .cpp 中）
const std::unordered_map<eNodeType, std::string> nodeTypeNameMap = {
	{ eNodeType::DeployNodeService,  eNodeType_Name(DeployNodeService) },
	{ eNodeType::SceneNodeService,   eNodeType_Name(SceneNodeService) },
	{ eNodeType::GateNodeService,    eNodeType_Name(GateNodeService) },
	{ eNodeType::CentreNodeService,  eNodeType_Name(CentreNodeService) },
	{ eNodeType::LoginNodeService,   eNodeType_Name(LoginNodeService) },
	{ eNodeType::DatabaseNodeService,eNodeType_Name(DatabaseNodeService) }
};

eNodeType NodeSystem::GetServiceTypeFromPrefix(const std::string& prefix) {
	for (const auto& [type, name] : nodeTypeNameMap) {
		if (prefix.find(name) != std::string::npos) {
			return type;
		}
	}

	LOG_ERROR << "Unknown service type for prefix: " << prefix;
	return eNodeType::CentreNodeService; // 默认返回，可以根据需求改成 Invalid 或抛异常
}


entt::registry& NodeSystem::GetRegistryForNodeType(uint32_t nodeType) {
	switch (nodeType) {
	case CentreNodeService:
		return tls.centreNodeRegistry;
	case SceneNodeService:
		return tls.sceneNodeRegistry;
	case GateNodeService:
		return tls.gateNodeRegistry;
	case LoginNodeService:
		return tls.loginNodeRegistry;
	default:
		LOG_ERROR << "Unknown NodeType: " << static_cast<uint32_t>(nodeType);
	}

	return tls.invalidRegistry;
}
