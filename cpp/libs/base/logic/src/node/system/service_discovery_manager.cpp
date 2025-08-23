#include "service_discovery_manager.h"
#include <thread_local/redis_manager.h>
#include "etcd_helper.h"
#include <muduo/base/Logging.h>
#include "node_util.h"
#include "node.h"
#include "node_connector.h"
#include "google/protobuf/util/json_util.h"
#include <util/node_utils.h>
#include <thread_local/node_config_manager.h>
#include <thread_local/registry_manager.h>

void ServiceDiscoveryManager::Init()
{
	etcdService.Init();
	FetchServiceNodes();
}

void ServiceDiscoveryManager::Shutdown()
{
	etcdService.Shutdown();
}

void ServiceDiscoveryManager::FetchServiceNodes() {
	for (const auto& prefix : tlsNodeConfigManager.GetBaseDeployConfig().service_discovery_prefixes()) {
		EtcdHelper::RangeQuery(prefix);
	}
}

void ServiceDiscoveryManager::AddServiceNode(const std::string& nodeJson, uint32_t nodeType) {
	LOG_INFO << "Add service node type " << nodeType << " JSON: " << nodeJson;

	if (!eNodeType_IsValid(static_cast<int32_t>(nodeType))) {
		LOG_ERROR << "Invalid node type: " << nodeType;
		return;
	}

	NodeInfo newNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &newNode);
	if (!parseResult.ok()) {
		LOG_ERROR << "Parse node JSON failed, type: " << nodeType
			<< ", JSON: " << nodeJson
			<< ", Error: " << parseResult.message().data();
		return;
	}

	auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();

	*nodeList.Add() = newNode;
	LOG_INFO << "Node added, type: " << nodeType << ", info: " << newNode.DebugString();

	if (gNode->IsMyNode(newNode)) {
		LOG_TRACE << "Node has same lease_id as self, skip adding node. Self uuid: " << newNode.node_uuid();
		return;
	}

	if (!gNode->GetTargetNodeTypeWhitelist().contains(nodeType)) return;

	if (gNode->IsServiceStarted()) {
		NodeConnector::ConnectToNode(newNode);
		LOG_INFO << "Connected to node: " << newNode.DebugString();
	}
	else {
		LOG_INFO << "Service not started or node already connected. Skipping connection for now: " << newNode.DebugString();
	}
}

void ServiceDiscoveryManager::HandleServiceNodeStart(const std::string& key, const std::string& value) {
	LOG_TRACE << "Service node start, key: " << key << ", value: " << value;
	if (const auto nodeType = NodeUtils::GetServiceTypeFromPrefix(key); eNodeType_IsValid(nodeType)) {
		AddServiceNode(value, nodeType);
	}
}
