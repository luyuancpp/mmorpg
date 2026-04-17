#include "node_message_utils.h"
#include "network/rpc_session.h"
#include <network/rpc_client.h>
#include "thread_context/redis_manager.h"
#include "proto/common/component/player_network_comp.pb.h"
#include "proto/common/base/message.pb.h"
#include "thread_context/node_context_manager.h"
#include "thread_context/player_manager.h"

template <typename SessionType>
void SendMessageToNodeInternal(SessionType* session, uint32_t messageId, const google::protobuf::Message& message) {
	if (!session) {
		LOG_ERROR << "Session is null. Cannot send message.";
		return;
	}

	session->SendRequest(messageId, message);
}


void SendMessageToSessionNode(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType) {
	entt::entity entity{ nodeId };
	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);

	if (!registry.valid(entity)) {
		LOG_ERROR << "Session node not found: " << nodeId << " of type: " << static_cast<int>(nodeType);
		return;
	}

	auto session = registry.try_get<RpcSession>(entity);
	if (!session) {
		LOG_ERROR << "RpcSession not found for node: " << nodeId;
		return;
	}

	SendMessageToNodeInternal(session, messageId, message);
}


void SendMessageToClientNode(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType) {
	entt::entity entity{ nodeId };
	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);

	if (!registry.valid(entity)) {
		LOG_ERROR << "Client node not found: " << nodeId << " of type: " << static_cast<int>(nodeType);
		return;
	}

	const auto nodePtr = registry.try_get<RpcClientPtr>(entity);
	if (!nodePtr) {
		LOG_ERROR << "RpcClientPtr not found for node: " << nodeId;
		return;
	}

	SendMessageToNodeInternal(nodePtr->get(), messageId, message);
}

void CallRemoteMethodOnSession(uint32_t messageId, const google::protobuf::Message& message,
	NodeId nodeId, uint32_t nodeType)
{
	entt::entity entity{ nodeId };
	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (!registry.valid(entity))
	{
		LOG_ERROR << "Node not found: " << nodeId << " of type: " << static_cast<int>(nodeType);
		return;
	}

	const auto node = registry.try_get<RpcSession>(entity);
	if (!node)
	{
		LOG_ERROR << "RpcSession not found for node: " << nodeId;
		return;
	}

	node->CallRemoteMethod(messageId, message);
}

void CallRemoteMethodOnClient(uint32_t messageId, const google::protobuf::Message& message,
	NodeId nodeId, uint32_t nodeType)
{
	entt::entity entity{ nodeId };
	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (!registry.valid(entity))
	{
		LOG_ERROR << "Node not found: " << nodeId;
		return;
	}

	const auto node = registry.try_get<RpcClientPtr>(entity);
	if (!node)
	{
		LOG_ERROR << "RpcClientPtr not found for node: " << nodeId;
		return;
	}

	(*node)->CallRemoteMethod(messageId, message);
}


void BroadcastToNodes(uint32_t messageId, const google::protobuf::Message& message,
	uint32_t nodeType)
{
	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);

	const auto byte_size = static_cast<size_t>(message.ByteSizeLong());
	std::string serialized;
	serialized.reserve(byte_size);
	if (!message.SerializeToString(&serialized)) {
		LOG_ERROR << "Failed to serialize broadcast message";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->mutable_serialized_message()->swap(serialized);

	for (auto&& [_, node] : registry.view<RpcClientPtr>().each())
	{
		// Reuse request where possible; SendRequest may copy internally for async handling
		node->SendRequest(messageId, request);
	}
}

void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId, uint32_t nodeType, uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayerViaClientNode(wrappedMessageId, nodeType, messageId, message, tlsEcs.GetPlayer(playerId));
}


void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	if (!tlsEcs.actorRegistry.valid(playerEntity)) {
		LOG_ERROR << "Invalid player entity";
		return;
	}

	const auto* sessionPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(playerEntity);
	if (!sessionPB) {
		LOG_ERROR << "Player session info not found for entity";
		return;
	}

	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(nodeType);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << nodeType
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	entt::entity nodeEntity{ it->second };

	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (!registry.valid(nodeEntity)) {
		LOG_ERROR << "Node not found for player, type = " << static_cast<int>(nodeType);
		return;
	}

	const auto rpcClient = registry.try_get<RpcClientPtr>(nodeEntity);
	if (!rpcClient) {
		LOG_ERROR << "RpcClientPtr not found for node, type = " << static_cast<int>(nodeType);
		return;
	}

	const auto byte_size = static_cast<size_t>(message.ByteSizeLong());
	std::string serialized;
	serialized.reserve(byte_size);
	if (!message.SerializeToString(&serialized)) {
		LOG_ERROR << "Failed to serialize message";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->mutable_serialized_message()->swap(serialized);

	request.mutable_header()->set_session_id(sessionPB->gate_session_id());

	// rpcClient may copy/wrap internally; we avoided extra temporary allocations
	(*rpcClient)->SendRequest(wrappedMessageId, request);
}


void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId)
{
	SendMessageToPlayerViaSessionNode(wrappedMessageId, nodeType, messageId, message, tlsEcs.GetPlayer(playerId));
}


void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	if (!tlsEcs.actorRegistry.valid(playerEntity)) {
		LOG_ERROR << "Invalid player entity";
		return;
	}

	const auto* sessionPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(playerEntity);
	if (!sessionPB) {
		LOG_ERROR << "Player session info not found for entity";
		return;
	}

	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(nodeType);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << nodeType
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	entt::entity nodeEntity{ it->second };

	auto& registry = tlsNodeContextManager.GetRegistry(nodeType);
	if (!registry.valid(nodeEntity)) {
		LOG_ERROR << "Node not found for player, type = " << nodeType;
		return;
	}

	const auto session = registry.try_get<RpcSession>(nodeEntity);
	if (!session) {
		LOG_ERROR << "RpcSession not found for node, type = " << nodeType;
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	auto* serialized = request.mutable_message_content()->mutable_serialized_message();
	const auto size = static_cast<int32_t>(message.ByteSizeLong());
	serialized->resize(size);
	if (!message.SerializeToArray(&(*serialized)[0], size)) {
		LOG_ERROR << "Failed to serialize message";
		return;
	}

	request.mutable_header()->set_session_id(sessionPB->gate_session_id());

	session->SendRequest(wrappedMessageId, request);
}

