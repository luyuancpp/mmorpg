#include "node_util.h"
#include <muduo/base/Logging.h>

#include "proto/common/common.pb.h"
#include <network/rpc_client.h>
#include "thread_local/node_context_manager.h"

// 静态映射表定义（可放在 .cpp 中）//todo
const std::unordered_map<eNodeType, std::string> nodeTypeNameMap = {
	{ eNodeType::DeployNodeService,  eNodeType_Name(DeployNodeService) },
	{ eNodeType::SceneNodeService,   eNodeType_Name(SceneNodeService) },
	{ eNodeType::GateNodeService,    eNodeType_Name(GateNodeService) },
	{ eNodeType::CentreNodeService,  eNodeType_Name(CentreNodeService) },
	{ eNodeType::LoginNodeService,   eNodeType_Name(LoginNodeService) },
	{ eNodeType::DbNodeService,eNodeType_Name(DbNodeService) }
};

eNodeType NodeUtils::GetServiceTypeFromPrefix(const std::string& prefix) {
	for (const auto& [type, name] : nodeTypeNameMap) {
		if (prefix.find(name) != std::string::npos) {
			return type;
		}
	}

	LOG_ERROR << "Unknown service type for prefix: " << prefix;
	return eNodeType(std::numeric_limits<::int32_t>::max());
}

entt::registry& NodeUtils::GetRegistryForNodeType(uint32_t nodeType) {
	return tlsNodeContextManager.GetRegistry(nodeType);
}

std::string NodeUtils::GetRegistryName(const entt::registry& registry) {
	for (uint32_t i = 0; i < tlsNodeContextManager.GetAllRegistries().size(); ++i){
		if (&tlsNodeContextManager.GetRegistry(i) == &registry) {
			return eNodeType_Name(i);
		}
	}
	return "UnknownRegistry";
}

eNodeType NodeUtils::GetRegistryType(const entt::registry& registry){
	for (uint32_t i = 0; i < tlsNodeContextManager.GetAllRegistries().size(); ++i){
		if (&tlsNodeContextManager.GetRegistry(i) == &registry) {
			return eNodeType(i);
		}
	}

	return eNodeType(std::numeric_limits<::int32_t>::max());
}

bool NodeUtils::IsSameNode(const std::string& uuid1, const std::string& uuid2)
{
	return uuid1 == uuid2;
}

bool NodeUtils::IsNodeConnected(uint32_t nodeType, const NodeInfo& info)  {
	switch (info.protocol_type()) {
	case PROTOCOL_TCP:
	{
		entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
			if (NodeUtils::IsSameNode(info.node_uuid(), nodeInfo.node_uuid())) {
				LOG_INFO << "Node already registered, IP: " << nodeInfo.endpoint().ip()
					<< ", Port: " << nodeInfo.endpoint().port();
				return true;
			}
		}
	}
	break;
	default:
		break;
	}

	return false;
}
