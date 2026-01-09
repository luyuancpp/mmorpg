#include "registration_manager.h"
#include <table/proto/tip/common_error_tip.pb.h>
#include <time/comp/timer_task_comp.h>
#include "node/system/node/node_util.h"
#include <rpc/service_metadata/scene_service_metadata.h>
#include <rpc/service_metadata/centre_service_service_metadata.h>
#include <rpc/service_metadata/gate_service_service_metadata.h>
#include <network/rpc_client.h>
#include <network/rpc_session.h>
#include "node/system/node/node.h"
#include <network/node_utils.h>
#include "proto/logic/event/server_event.pb.h"
#include <proto_helpers/proto_util.h>
#include "threading/node_context_manager.h"
#include <threading/registry_manager.h>
#include <threading/entity_manager.h>
#include <threading/rpc_manager.h>
#include "proto/common/message.pb.h"

static uint32_t kNodeTypeToMessageId[eNodeType_ARRAYSIZE] = {
	0,
	0,
	CentreNodeHandshakeMessageId,
	SceneNodeHandshakeMessageId,
	GateNodeHandshakeMessageId
};

void NodeHandshakeManager::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->GetConnection() == nullptr || client->GetConnection().get() != conn.get()) continue;
		LOG_INFO << "Peer address match in " << NodeUtils::GetRegistryName(registry)
			<< ": " << conn->peerAddress().toIpPort();
		registry.get_or_emplace<TimerTaskComp>(entity).RunAfter(0.5, [conn, this, nodeType, &client]() {
			NodeHandshakeRequest req;
			req.mutable_self_node()->CopyFrom(GetNodeInfo());
			client->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
			});
		return;
	}
}


void NodeHandshakeManager::OnNodeHandshake(
	const NodeHandshakeRequest& request,
	NodeHandshakeResponse& response
) const {
	if (!RpcThreadContext::tls_current_conn)
	{
		response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
		LOG_ERROR << "No current connection in thread-local storage.";
		return;
	}

	auto& peerNode = request.self_node();
	response.mutable_peer_node()->CopyFrom(gNode->GetNodeInfo());
	LOG_TRACE << "Node registration request: " << request.DebugString();

	auto tryRegister = [&, this](const TcpConnectionPtr& conn, uint32_t nodeType) -> bool {
		entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
		const auto& nodeList = tlsRegistryManager.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		for (auto& serverNode : nodeList[nodeType].node_list()) {
			if (!NodeUtils::IsSameNode(serverNode.node_uuid(), peerNode.node_uuid())) continue;
			entt::entity nodeEntity = entt::entity{ serverNode.node_id() };
			entt::entity created = RecreateEntity(registry, nodeEntity);
			if (created == entt::null) {
				LOG_ERROR << "Create node entity failed in " << NodeUtils::GetRegistryName(registry);
				return false;
			}
			registry.emplace<RpcSession>(created, RpcSession{ conn , request.self_node().node_uuid()});
			LOG_INFO << "Node registered, id: " << peerNode.node_id()
				<< " in " << NodeUtils::GetRegistryName(registry);
			return true;
		}
		return false;
		};

	auto& conn = RpcThreadContext::tls_current_conn;
	if (!IsTcpNodeType(peerNode.node_type()))
	{
		response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
		LOG_ERROR << "Invalid node type for registration: " << peerNode.node_type();
		return;
	}
	if (tryRegister(conn, peerNode.node_type())) {
		response.mutable_error_message()->set_id(kCommon_errorOK);
		LOG_INFO << "Node registration succeeded: " << peerNode.DebugString();
		return;
	}
	response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
}

void NodeHandshakeManager::OnHandshakeReplied(const NodeHandshakeResponse& response) const {
	LOG_INFO << "Node registration response: " << response.DebugString();
	uint32_t nodeType = response.peer_node().node_type();
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (response.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Registration failed: " << response.DebugString();
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
			if (!NodeUtils::IsSameNode(nodeInfo.node_uuid(), response.peer_node().node_uuid())) continue;
			registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, &client, nodeType]() {
				NodeHandshakeRequest req;
				*req.mutable_self_node() = GetNodeInfo();
				client->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
				});
			return;
		}
		return;
	}

	entt::entity peerEntity{ response.peer_node().node_id() };
	registry.remove<TimerTaskComp>(peerEntity);
	TriggerNodeConnectionEvent(registry, response);
	LOG_INFO << "Node registration success.";
}


void NodeHandshakeManager::TriggerNodeConnectionEvent(entt::registry& registry, const NodeHandshakeResponse& response) const {
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->peer_addr().toIp() != response.peer_node().endpoint().ip() ||
			client->peer_addr().port() != response.peer_node().endpoint().port()) {
			continue;
		}
		ConnectToNodePbEvent connectEvent;
		connectEvent.set_entity(entt::to_integral(entity));
		connectEvent.set_node_type(nodeInfo.node_type());
		dispatcher.trigger(connectEvent);
		if (nodeInfo.node_type() == CentreNodeService) {
			OnConnect2CentrePbEvent centreEvent;
			centreEvent.set_entity(entt::to_integral(entity));
			dispatcher.trigger(centreEvent);
			LOG_INFO << "CentreNode connected, entity: " << entt::to_integral(entity);
		}
		registry.remove<TimerTaskComp>(entity);
		break;
	}
}
