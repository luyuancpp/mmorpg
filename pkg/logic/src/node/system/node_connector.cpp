#include "node_connector.h"
#include "proto/common/common.pb.h"
#include "node_util.h"
#include <muduo/base/Logging.h>
#include <thread_local/storage.h>
#include <network/rpc_client.h>
#include <util/node_utils.h>
#include <grpc/generator/grpc_init.h>
#include <thread_local/storage_common_logic.h>
#include "node.h"
#include "util/network_utils.h"

bool IsMyNode(const NodeInfo& node)
{
	return NodeUtils::IsSameNode(node.node_uuid(), GetNodeInfo().node_uuid());
}

void NodeConnector::ConnectToNode(const NodeInfo& info) {
	if (IsMyNode(info)) {
		LOG_INFO << "Skipping connection to self node: " << info.DebugString();
		return;
	}
	switch (info.protocol_type()) {
	case PROTOCOL_GRPC:
		ConnectToGrpcNode(info);
		break;
	case PROTOCOL_TCP:
		ConnectToTcpNode(info);
		break;
	case PROTOCOL_HTTP:
		ConnectToHttpNode(info);
		break;
	default:
		LOG_ERROR << "Unsupported protocol: " << info.protocol_type()
			<< " node: " << info.DebugString();
		break;
	}
}

void NodeConnector::ConnectToGrpcNode(const NodeInfo& info) {
	auto& nodeList = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& registry = NodeUtils::GetRegistryForNodeType(info.node_type());

	const entt::entity entityId{ info.node_id() };
	auto createdId = ResetEntity(registry, entityId);
	if (createdId == entt::null) {
		LOG_ERROR << "Login node not found: " << entt::to_integral(createdId);
		return;
	}

	const auto& channel = registry.emplace<std::shared_ptr<grpc::Channel>>(entityId,
		grpc::CreateChannel(::FormatIpAndPort(info.endpoint().ip(), info.endpoint().port()),
			grpc::InsecureChannelCredentials()));

	InitGrpcNode(channel, registry, entityId);
	registry.emplace<NodeInfo>(entityId, info);

	LOG_INFO << "Connecting to GRPC node, ID: " << info.node_id()
		<< ", IP: " << info.endpoint().ip()
		<< ", Port: " << info.endpoint().port()
		<< ", NodeType: " << info.node_type();

	//todo 如果重连后连上了不同的gate会不会有异步问题
}

void NodeConnector::ConnectToTcpNode(const NodeInfo& info) {
	auto& registry = tls.GetNodeRegistry(info.node_type());
	entt::entity entityId{ info.node_id() };

	if (registry.valid(entityId)) {
		if (auto* existInfo = registry.try_get<NodeInfo>(entityId);
			existInfo) {

			if (NodeUtils::IsSameNode(info.node_uuid(), existInfo->node_uuid())) {
				LOG_INFO << "Node already registered, IP: " << info.endpoint().ip()
					<< ", Port: " << info.endpoint().port();
				return;
			}

			if (auto* client = registry.try_get<RpcClientPtr>(entityId)) {
				gNode->GetZombieClientList().push_back(*client);
			}
		}
	}

	const auto createdId = ResetEntity(registry, entityId);
	if (createdId == entt::null) {
		LOG_ERROR << "Failed to create node entity: " << entt::to_integral(entityId);
		return;
	}

	InetAddress endpoint(info.endpoint().ip(), info.endpoint().port());
	auto& client = registry.emplace<RpcClientPtr>(
		createdId,
		std::make_shared<RpcClientPtr::element_type>(gNode->GetLoop(), endpoint)
	);

	registry.emplace<NodeInfo>(createdId, info);

	client->registerService(gNode->GetNodeReplyService());
	client->connect();

	LOG_INFO << "Connecting to TCP node, Uuid: " << info.node_uuid()
		<< ", IP: " << info.endpoint().ip()
		<< ", Port: " << info.endpoint().port();

	// Step 4: 设置中心节点引用（仅限中心服）
	if (info.node_type() == CentreNodeService &&
		info.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
		gNode->SetZoneCentreNode(client);
	}
}

void NodeConnector::ConnectToHttpNode(const NodeInfo&) {
	// HTTP连接逻辑可扩展
}

void NodeConnector::ConnectAllNodes() {
	auto& nodeRegistry = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());

	for (uint32_t nodeType = 0; nodeType < eNodeType_ARRAYSIZE; ++nodeType)
	{
		if (!gNode->GetTargetNodeTypeWhitelist().contains(nodeType)) continue;

		for (const auto& node : nodeRegistry[nodeType].node_list()) {
			if (NodeUtils::IsNodeConnected(nodeType, node)) continue;

			ConnectToNode(node);
			LOG_INFO << "Connected to node from ConnectAllNodes: " << node.DebugString();
		}
	}
}