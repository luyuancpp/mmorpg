#include "registration_manager.h"
#include <proto/table/common_error_tip.pb.h>
#include <time/comp/timer_task_comp.h>
#include "node/system/node/node_util.h"
#include <service_info/game_service_service_info.h>
#include <service_info/centre_service_service_info.h>
#include <service_info/gate_service_service_info.h>
#include <network/rpc_client.h>
#include <network/rpc_session.h>
#include "node/system/node/node.h"
#include <network/node_utils.h>
#include "proto/logic/event/server_event.pb.h"
#include <proto_util/proto_util.h>
#include "threading/node_context_manager.h"
#include <threading/registry_manager.h>
#include <threading/entity_manager.h>

static uint32_t kNodeTypeToMessageId[eNodeType_ARRAYSIZE] = {
	0,
	0,
	CentreRegisterNodeSessionMessageId,
	SceneRegisterNodeSessionMessageId,
	GateRegisterNodeSessionMessageId
};

void NodeRegistrationManager::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (!IsSameAddress(client->peer_addr(), conn->peerAddress())) continue;
		LOG_INFO << "Peer address match in " << NodeUtils::GetRegistryName(registry)
			<< ": " << conn->peerAddress().toIpPort();
		registry.emplace<TimerTaskComp>(entity).RunAfter(0.5, [conn, this, nodeType, &client]() {
			RegisterNodeSessionRequest req;
			req.mutable_self_node()->CopyFrom(GetNodeInfo());
			req.mutable_endpoint()->set_ip(conn->localAddress().toIp());
			req.mutable_endpoint()->set_port(conn->localAddress().port());
			client->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
			});
		return;
	}
}


void NodeRegistrationManager::HandleNodeRegistration(
	const RegisterNodeSessionRequest& request,
	RegisterNodeSessionResponse& response
) const {
	auto& peerNode = request.self_node();
	response.mutable_peer_node()->CopyFrom(gNode->GetNodeInfo());
	LOG_TRACE << "Node registration request: " << request.DebugString();

	auto tryRegister = [&, this](const TcpConnectionPtr& conn, uint32_t nodeType) -> bool {
		entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
		const auto& nodeList = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		for (auto& serverNode : nodeList[nodeType].node_list()) {
			if (!NodeUtils::IsSameNode(serverNode.node_uuid(), peerNode.node_uuid())) continue;
			entt::entity nodeEntity = entt::entity{ serverNode.node_id() };
			entt::entity created = ResetEntity(registry, nodeEntity);
			if (created == entt::null) {
				LOG_ERROR << "Create node entity failed in " << NodeUtils::GetRegistryName(registry);
				return false;
			}
			registry.emplace<RpcSession>(created, RpcSession{ conn });
			LOG_INFO << "Node registered, id: " << peerNode.node_id()
				<< " in " << NodeUtils::GetRegistryName(registry);
			return true;
		}
		return false;
		};

	for (const auto& [entity, session] : tlsRegistryManager.sessionRegistry.view<RpcSession>().each()) {
		auto& conn = session.connection;
		if (!IsSameAddress(conn->peerAddress(), muduo::net::InetAddress(request.endpoint().ip(), request.endpoint().port()))) continue;
		for (uint32_t nodeType = eNodeType_MIN; nodeType < eNodeType_ARRAYSIZE; ++nodeType) {
			if (peerNode.node_type() != nodeType || !IsTcpNodeType(nodeType)) continue;
			if (tryRegister(conn, nodeType)) {
				tlsRegistryManager.sessionRegistry.destroy(entity);
				response.mutable_error_message()->set_id(kCommon_errorOK);
				LOG_INFO << "Node registration succeeded: " << peerNode.DebugString();
				return;
			}
		}
	}
	response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
}

void NodeRegistrationManager::HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const {
	LOG_INFO << "Node registration response: " << response.DebugString();
	uint32_t nodeType = response.peer_node().node_type();
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (response.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Registration failed: " << response.DebugString();
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
			if (!IsSameAddress(client->peer_addr(), muduo::net::InetAddress(
				response.peer_node().endpoint().ip(),
				response.peer_node().endpoint().port()))) continue;
			registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, &client, nodeType]() {
				RegisterNodeSessionRequest req;
				*req.mutable_self_node() = GetNodeInfo();
				req.mutable_endpoint()->set_ip(client->local_addr().toIp());
				req.mutable_endpoint()->set_port(client->local_addr().port());
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


void NodeRegistrationManager::TriggerNodeConnectionEvent(entt::registry& registry, const RegisterNodeSessionResponse& response) const {
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
