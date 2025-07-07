#include "node_system.h"
#include <muduo/base/Logging.h>
#include "thread_local/storage.h"

// 静态映射表定义（可放在 .cpp 中）//todo
const std::unordered_map<eNodeType, std::string> nodeTypeNameMap = {
	{ eNodeType::DeployNodeService,  eNodeType_Name(DeployNodeService) },
	{ eNodeType::SceneNodeService,   eNodeType_Name(SceneNodeService) },
	{ eNodeType::GateNodeService,    eNodeType_Name(GateNodeService) },
	{ eNodeType::CentreNodeService,  eNodeType_Name(CentreNodeService) },
	{ eNodeType::LoginNodeService,   eNodeType_Name(LoginNodeService) },
	{ eNodeType::DbNodeService,eNodeType_Name(DbNodeService) }
};

eNodeType NodeSystem::GetServiceTypeFromPrefix(const std::string& prefix) {
	for (const auto& [type, name] : nodeTypeNameMap) {
		if (prefix.find(name) != std::string::npos) {
			return type;
		}
	}

	LOG_ERROR << "Unknown service type for prefix: " << prefix;
	return eNodeType(std::numeric_limits<::int32_t>::max());
}

entt::registry& NodeSystem::GetRegistryForNodeType(uint32_t nodeType) {
	return tls.GetNodeRegistry(nodeType);
}

std::string NodeSystem::GetRegistryName(const entt::registry& registry) {
	for (uint32_t i = 0; i < tls.GetNodeRegistry().size(); ++i){
		if (&tls.GetNodeRegistry(i) == &registry) {
			return eNodeType_Name(i);
		}
	}
	return "UnknownRegistry";
}

eNodeType NodeSystem::GetRegistryType(const entt::registry& registry){
	for (uint32_t i = 0; i < tls.GetNodeRegistry().size(); ++i){
		if (&tls.GetNodeRegistry(i) == &registry) {
			return eNodeType(i);
		}
	}

	return eNodeType(std::numeric_limits<::int32_t>::max());
}

