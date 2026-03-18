#include "node_connector.h"
#include "proto/common/base/common.pb.h"
#include "node_util.h"
#include <muduo/base/Logging.h>
#include <network/rpc_client.h>
#include <network/node_utils.h>
#include <grpc_client/grpc_init_client.h>
#include <threading/redis_manager.h>
#include "node.h"
#include "network/network_utils.h"
#include "threading/node_context_manager.h"
#include <node_config_manager.h>
#include <threading/registry_manager.h>
#include <threading/entity_manager.h>
#include "node/system/grpc_channel_cache.h"

void NodeConnector::ConnectToNode(const NodeInfo& nodeInfo) {
	if (gNode->IsCurrentNode(nodeInfo)) {
		LOG_INFO << "Skipping connection to self node: " << nodeInfo.DebugString();
		return;
	}
	switch (nodeInfo.protocol_type()) {
	case PROTOCOL_GRPC:
		ConnectToGrpcNode(nodeInfo);
		break;
	case PROTOCOL_TCP:
		ConnectToTcpNode(nodeInfo);
		break;
	case PROTOCOL_HTTP:
		ConnectToHttpNode(nodeInfo);
		break;
	default:
		LOG_ERROR << "Unsupported protocol: " << nodeInfo.protocol_type()
			<< " node: " << nodeInfo.DebugString();
		break;
	}
}

void NodeConnector::ConnectToGrpcNode(const NodeInfo& nodeInfo) {
	auto& targetRegistry = NodeUtils::GetRegistryForNodeType(nodeInfo.node_type());

	const entt::entity nodeEntity{ nodeInfo.node_id() };
	if (targetRegistry.valid(nodeEntity)) {
		if (auto* existingNodeInfo = targetRegistry.try_get<NodeInfo>(nodeEntity);
			existingNodeInfo != nullptr) {
			if (NodeUtils::IsSameNode(nodeInfo.node_uuid(), existingNodeInfo->node_uuid())) {
				LOG_INFO << "GRPC node already registered, IP: " << nodeInfo.endpoint().ip()
					<< ", Port: " << nodeInfo.endpoint().port();
				return;
			}

			LOG_WARN << "GRPC node id reused by new uuid, recreate entity. node_id=" << nodeInfo.node_id()
				<< ", old_uuid=" << existingNodeInfo->node_uuid()
				<< ", new_uuid=" << nodeInfo.node_uuid();
		}
	}

	auto createdId = RecreateEntity(targetRegistry, nodeEntity);
	if (createdId == entt::null) {
		LOG_ERROR << "Login node not found: " << entt::to_integral(createdId);
		return;
	}

	const auto target = ::FormatIpAndPort(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
	auto cachedChannel = gNode->GetGrpcChannelCache().GetOrCreateChannel(target);
	const auto& grpcChannel = targetRegistry.emplace<std::shared_ptr<grpc::Channel>>(createdId, std::move(cachedChannel));

	InitGrpcNode(grpcChannel, targetRegistry, createdId);
	targetRegistry.emplace<NodeInfo>(createdId, nodeInfo);

	LOG_INFO << "Connecting to GRPC node, ID: " << nodeInfo.node_id()
		<< ", IP: " << nodeInfo.endpoint().ip()
		<< ", Port: " << nodeInfo.endpoint().port()
		<< ", NodeType: " << nodeInfo.node_type();

	//todo 如果重连后连上了不同的gate会不会有异步问题
}

void NodeConnector::ConnectToTcpNode(const NodeInfo& nodeInfo) {
	auto& targetRegistry = tlsNodeContextManager.GetRegistry(nodeInfo.node_type());
	entt::entity nodeEntity{ nodeInfo.node_id() };

	if (targetRegistry.valid(nodeEntity)) {
		if (auto* existingNodeInfo = targetRegistry.try_get<NodeInfo>(nodeEntity);
			existingNodeInfo) {

			if (NodeUtils::IsSameNode(nodeInfo.node_uuid(), existingNodeInfo->node_uuid())) {
				LOG_INFO << "Node already registered, IP: " << nodeInfo.endpoint().ip()
					<< ", Port: " << nodeInfo.endpoint().port();
				return;
			}

			if (auto* existingClient = targetRegistry.try_get<RpcClientPtr>(nodeEntity)) {
				gNode->GetZombieClientList().push_back(*existingClient);
			}
		}
	}

	const auto createdId = RecreateEntity(targetRegistry, nodeEntity);
	if (createdId == entt::null) {
		LOG_ERROR << "Failed to create node entity: " << entt::to_integral(nodeEntity);
		return;
	}

	InetAddress remoteEndpoint(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
	auto& client = targetRegistry.emplace<RpcClientPtr>(
		createdId,
		std::make_shared<RpcClientPtr::element_type>(gNode->GetLoop(), remoteEndpoint)
	);

	targetRegistry.emplace<NodeInfo>(createdId, nodeInfo);

	auto* replyService = gNode->GetNodeReplyService();
	if (replyService != nullptr) {
		client->registerService(replyService);
	}
	else {
		LOG_WARN << "Node reply service is null, skip client registerService. node_type=" << gNode->GetNodeType();
	}
	client->connect();

	LOG_INFO << "Connecting to TCP node, Uuid: " << nodeInfo.node_uuid()
		<< ", IP: " << nodeInfo.endpoint().ip()
		<< ", Port: " << nodeInfo.endpoint().port();

}

void NodeConnector::ConnectToHttpNode(const NodeInfo&) {
	// HTTP连接逻辑可扩展
}

void NodeConnector::ConnectAllNodes() {
	auto& serviceNodesByType = tlsRegistryManager.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());

	for (uint32_t nodeType = 0; nodeType < eNodeType_ARRAYSIZE; ++nodeType)
	{
		if (!gNode->GetTargetNodeTypeWhitelist().contains(nodeType)) continue;

		for (const auto& serviceNode : serviceNodesByType[nodeType].node_list()) {
			if (NodeUtils::IsNodeConnected(nodeType, serviceNode)) continue;

			ConnectToNode(serviceNode);
			LOG_INFO << "Connected to node from ConnectAllNodes: " << serviceNode.DebugString();
		}
	}
}