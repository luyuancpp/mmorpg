#include "registration_manager.h"
#include <table/proto/tip/common_error_tip.pb.h>
#include <time/comp/timer_task_comp.h>
#include "node/system/node/node_util.h"
#include <rpc/service_metadata/scene_service_metadata.h>
#include <rpc/service_metadata/gate_service_service_metadata.h>
#include <network/rpc_client.h>
#include <network/rpc_session.h>
#include "node/system/node/node.h"
#include <network/node_utils.h>
#include "proto/common/event/server_event.pb.h"
#include <proto_helpers/proto_util.h>
#include "thread_context/node_context_manager.h"
#include "proto/common/base/message.pb.h"
#include "proto/common/event/node_event.pb.h"
#include <thread_context/rpc_request_context.h>

static uint32_t kNodeTypeToMessageId[eNodeType_ARRAYSIZE] = {
	0,
	0,
	0,
	SceneNodeHandshakeMessageId,
	GateNodeHandshakeMessageId
};

void NodeHandshakeManager::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->GetConnection() == nullptr || client->GetConnection().get() != conn.get()) continue;
		LOG_INFO << "Peer address match in " << NodeUtils::GetRegistryName(registry)
			<< ": " << conn->peerAddress().toIpPort();
		auto clientCopy = client;
		registry.get_or_emplace<TimerTaskComp>(entity).RunAfter(0.5, [conn, this, nodeType, clientCopy]() {
			NodeHandshakeRequest req;
			req.mutable_self_node()->CopyFrom(GetNodeInfo());
			clientCopy->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
			});
		return;
	}
}


void NodeHandshakeManager::OnNodeHandshake(
	const NodeHandshakeRequest& request,
	NodeHandshakeResponse& response
) const {
	if (!tlsRpc.conn)
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
		const auto& nodeList = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
		for (auto& serverNode : nodeList[nodeType].node_list()) {
			if (!NodeUtils::IsSameNode(serverNode.node_uuid(), peerNode.node_uuid())) continue;

			// uuid is the only globally-unique handle for a remote node, so we
			// look up (or freshly allocate) the entity by uuid. Previously the
			// entity slot was keyed by node_id, which silently aliased nodes
			// from different zones onto the same slot.
			entt::entity nodeEntity = entt::null;
			if (const auto found = NodeUtils::FindNodeEntityByUuid(nodeType, peerNode.node_uuid()); found) {
				nodeEntity = *found;
				// Reset session-bound components on re-registration so any
				// stale RpcSession from a previous connection is cleared.
				registry.remove<RpcSession>(nodeEntity);
			} else {
				nodeEntity = registry.create();
				LOG_TRACE << "Allocated new entity " << entt::to_integral(nodeEntity)
					<< " for handshaking peer uuid=" << peerNode.node_uuid();
			}

			registry.emplace<RpcSession>(nodeEntity, RpcSession{ conn , request.self_node().node_uuid()});
			LOG_INFO << "Node registered, id: " << peerNode.node_id()
				<< " uuid: " << peerNode.node_uuid()
				<< " entity: " << entt::to_integral(nodeEntity)
				<< " in " << NodeUtils::GetRegistryName(registry);
			return true;
		}
		return false;
		};

	auto& conn = tlsRpc.conn;
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
	LOG_TRACE << "Node registration response: " << response.DebugString();
	uint32_t nodeType = response.peer_node().node_type();
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (response.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Registration failed: " << response.DebugString();
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
			if (!NodeUtils::IsSameNode(nodeInfo.node_uuid(), response.peer_node().node_uuid())) continue;
			auto clientCopy = client;
			registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, clientCopy, nodeType]() {
				NodeHandshakeRequest req;
				*req.mutable_self_node() = GetNodeInfo();
				clientCopy->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
				});
			return;
		}
		return;
	}

	// uuid is the canonical handle for the remote peer; node_id alone is
	// not unique across zones and would mis-resolve the entity slot.
	const auto peerEntityOpt = NodeUtils::FindNodeEntityByUuid(nodeType, response.peer_node().node_uuid());
	if (!peerEntityOpt) {
		LOG_WARN << "OnHandshakeReplied: peer entity not found for uuid="
			<< response.peer_node().node_uuid()
			<< " in " << NodeUtils::GetRegistryName(registry);
		return;
	}
	registry.remove<TimerTaskComp>(*peerEntityOpt);
	TriggerNodeConnectionEvent(registry, response);
	LOG_INFO << "Node registration success.";
}


void NodeHandshakeManager::TriggerNodeConnectionEvent(entt::registry& registry, const NodeHandshakeResponse& response) const {
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->peer_addr().toIp() != response.peer_node().endpoint().ip() ||
			client->peer_addr().port() != response.peer_node().endpoint().port()) {
			continue;
		}
		ConnectToNodeEvent connectEvent;
		connectEvent.set_entity(entt::to_integral(entity));
		connectEvent.set_node_type(nodeInfo.node_type());
		tlsEcs.dispatcher.trigger(connectEvent);
		registry.remove<TimerTaskComp>(entity);
		break;
	}
}

