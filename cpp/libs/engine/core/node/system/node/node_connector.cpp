#include "node_connector.h"
#include "proto/common/base/common.pb.h"
#include "node_util.h"
#include <muduo/base/Logging.h>
#include <network/rpc_client.h>
#include <network/node_utils.h>
#include <grpc_client/grpc_init_client.h>
#include <thread_context/redis_manager.h>
#include "node.h"
#include "network/network_utils.h"
#include "thread_context/node_context_manager.h"
#include <node_config_manager.h>
#include <thread_context/registry_manager.h>
#include <thread_context/entity_manager.h>
#include "node/system/grpc_channel_cache.h"

void NodeConnector::ConnectToNode(const NodeInfo &nodeInfo)
{
	if (gNode->IsCurrentNode(nodeInfo))
	{
		LOG_INFO << "Skipping connection to self node: " << nodeInfo.DebugString();
		return;
	}
	switch (nodeInfo.protocol_type())
	{
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

void NodeConnector::ConnectToGrpcNode(const NodeInfo &nodeInfo)
{
	auto &targetRegistry = NodeUtils::GetRegistryForNodeType(nodeInfo.node_type());

	// GRPC node_id is only unique within a zone, not globally across zones.
	// Use uuid-based dedup and auto-generated entity IDs to avoid collisions
	// when the gate discovers GRPC nodes from multiple zones (e.g. login nodes
	// with node_id 1..3 in each of 10 zones sharing the same etcd).
	for (const auto &[entity, existing] : targetRegistry.view<NodeInfo>().each())
	{
		if (NodeUtils::IsSameNode(nodeInfo.node_uuid(), existing.node_uuid()))
		{
			LOG_INFO << "GRPC node already registered, IP: " << nodeInfo.grpc_endpoint().ip()
					 << ", Port: " << nodeInfo.grpc_endpoint().port();
			return;
		}
	}

	auto createdId = targetRegistry.create();

	// Prefer grpc_endpoint; fall back to endpoint if grpc_endpoint is empty
	const auto &ep = (!nodeInfo.grpc_endpoint().ip().empty() && nodeInfo.grpc_endpoint().port() > 0)
		? nodeInfo.grpc_endpoint()
		: nodeInfo.endpoint();
	if (ep.ip().empty() || ep.port() == 0)
	{
		LOG_ERROR << "Cannot connect to GRPC node with empty endpoint, ID: " << nodeInfo.node_id()
				  << ", NodeType: " << nodeInfo.node_type();
		targetRegistry.destroy(createdId);
		return;
	}
	const auto target = ::FormatIpAndPort(ep.ip(), ep.port());
	auto cachedChannel = gNode->GetGrpcChannelCache().GetOrCreateChannel(target);
	const auto &grpcChannel = targetRegistry.emplace<std::shared_ptr<grpc::Channel>>(createdId, std::move(cachedChannel));

	InitGrpcNode(grpcChannel, targetRegistry, createdId);
	targetRegistry.emplace<NodeInfo>(createdId, nodeInfo);

	LOG_INFO << "Connecting to GRPC node, ID: " << nodeInfo.node_id()
			 << ", IP: " << ep.ip()
			 << ", Port: " << ep.port()
			 << ", NodeType: " << nodeInfo.node_type()
			 << ", Entity: " << entt::to_integral(createdId);
}

void NodeConnector::ConnectToTcpNode(const NodeInfo &nodeInfo)
{
	auto &targetRegistry = tlsNodeContextManager.GetRegistry(nodeInfo.node_type());
	entt::entity nodeEntity{nodeInfo.node_id()};

	if (targetRegistry.valid(nodeEntity))
	{
		if (auto *existingNodeInfo = targetRegistry.try_get<NodeInfo>(nodeEntity);
			existingNodeInfo)
		{

			if (NodeUtils::IsSameNode(nodeInfo.node_uuid(), existingNodeInfo->node_uuid()))
			{
				LOG_INFO << "Node already registered, IP: " << nodeInfo.endpoint().ip()
						 << ", Port: " << nodeInfo.endpoint().port();
				return;
			}

			if (auto *existingClient = targetRegistry.try_get<RpcClientPtr>(nodeEntity))
			{
				gNode->GetDisconnectedClientList().push_back(*existingClient);
			}
		}
	}

	const auto createdId = RecreateEntity(targetRegistry, nodeEntity);
	if (createdId == entt::null)
	{
		LOG_ERROR << "Failed to create node entity: " << entt::to_integral(nodeEntity);
		return;
	}

	muduo::net::InetAddress remoteEndpoint(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
	auto &client = targetRegistry.emplace<RpcClientPtr>(
		createdId,
		std::make_shared<RpcClientPtr::element_type>(gNode->GetLoop(), remoteEndpoint));

	targetRegistry.emplace<NodeInfo>(createdId, nodeInfo);

	auto *replyService = gNode->GetNodeReplyService();
	if (replyService != nullptr)
	{
		client->registerService(replyService);
	}
	else
	{
		LOG_WARN << "Node reply service is null, skip client registerService. node_type=" << gNode->GetNodeType();
	}
	client->connect();

	LOG_INFO << "Connecting to TCP node, Uuid: " << nodeInfo.node_uuid()
			 << ", IP: " << nodeInfo.endpoint().ip()
			 << ", Port: " << nodeInfo.endpoint().port();
}

void NodeConnector::ConnectToHttpNode(const NodeInfo &)
{
	// HTTP connection — extensible
}

void NodeConnector::ConnectAllNodes()
{
	auto &serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());

	const auto &whitelist = gNode->GetTargetNodeTypeWhitelist();
	for (uint32_t nodeType = 0; nodeType < eNodeType_ARRAYSIZE; ++nodeType)
	{
		if (!whitelist.empty() && !whitelist.contains(nodeType))
			continue;

		for (const auto &serviceNode : serviceNodesByType[nodeType].node_list())
		{
			if (NodeUtils::IsNodeConnected(nodeType, serviceNode))
				continue;

			ConnectToNode(serviceNode);
			LOG_INFO << "Connected to node from ConnectAllNodes: " << serviceNode.DebugString();
		}
	}
}
