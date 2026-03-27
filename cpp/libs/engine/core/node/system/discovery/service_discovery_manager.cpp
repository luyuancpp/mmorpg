#include "service_discovery_manager.h"
#include <thread_context/redis_manager.h>
#include "node/system/etcd/etcd_helper.h"
#include <muduo/base/Logging.h>
#include "node/system/node/node_util.h"
#include "node/system/node/node.h"
#include "node/system/node/node_connector.h"
#include "google/protobuf/util/json_util.h"
#include <network/node_utils.h>
#include <node_config_manager.h>
#include <thread_context/registry_manager.h>
#include <thread_context/entity_manager.h>

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

	NodeInfo discoveredNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &discoveredNode);
	if (!parseResult.ok()) {
		LOG_ERROR << "Parse node JSON failed, type: " << nodeType
			<< ", JSON: " << nodeJson
			<< ", Error: " << parseResult.message().data();
		return;
	}

	auto& serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
	auto& cachedNodesOfType = *serviceNodesByType[nodeType].mutable_node_list();

	bool hasExistingSnapshot = false;
	for (auto& cachedNode : cachedNodesOfType) {
		if (!NodeUtils::IsSameNode(cachedNode.node_uuid(), discoveredNode.node_uuid())) {
			continue;
		}

		cachedNode.CopyFrom(discoveredNode);
		hasExistingSnapshot = true;
		break;
	}

	if (!hasExistingSnapshot) {
		*cachedNodesOfType.Add() = discoveredNode;
		LOG_INFO << "Node added, type: " << nodeType << ", info: " << discoveredNode.DebugString();
	}
	else {
		LOG_TRACE << "Node updated from service discovery event, uuid=" << discoveredNode.node_uuid();
	}

	if (gNode->IsCurrentNode(discoveredNode)) {
		LOG_TRACE << "Node has same lease_id as self, skip adding node. Self uuid: " << discoveredNode.node_uuid();
		return;
	}

	if (!gNode->GetTargetNodeTypeWhitelist().contains(nodeType)) return;
	if (NodeUtils::IsNodeConnected(nodeType, discoveredNode)) {
		LOG_TRACE << "Node already connected, skip reconnect. uuid=" << discoveredNode.node_uuid();
		return;
	}

	if (gNode->IsServiceStarted()) {
		NodeConnector::ConnectToNode(discoveredNode);
		LOG_INFO << "Connected to node: " << discoveredNode.DebugString();
	}
	else {
		LOG_INFO << "Service not started or node already connected. Skipping connection for now: " << discoveredNode.DebugString();
	}
}

void ServiceDiscoveryManager::HandleServiceNodeStart(const std::string& key, const std::string& value) {
	LOG_TRACE << "Service node start, key: " << key << ", value: " << value;
	if (const auto nodeType = NodeUtils::GetServiceTypeFromPrefix(key); eNodeType_IsValid(nodeType)) {
		AddServiceNode(value, nodeType);
	}
}

