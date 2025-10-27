#include <cstdint>

#include "node_message_utils.h"
#include "network/rpc_session.h"
#include <network/rpc_client.h>
#include "threading/redis_manager.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "threading/node_context_manager.h"
#include "threading/player_manager.h"
#include <threading/registry_manager.h>

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

	SendMessageToNodeInternal(nodePtr->get(), messageId, message); // 使用 get() 提取裸指针
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
		LOG_ERROR << "RpcClientPtr not found for node: " << nodeId;
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
		// 尽可能将 request reuse（如果 rpc 客户端的 SendRequest 对 request 做了拷贝/异步处理，则可能需要复制）
		node->SendRequest(messageId, request);
	}
}

void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId, uint32_t nodeType, uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayerViaClientNode(wrappedMessageId, nodeType, messageId, message, GetPlayer(playerId));
}


void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity)) {
		LOG_ERROR << "Invalid player entity";
		return;
	}

	const auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
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
	request.mutable_message_content()->mutable_serialized_message()->swap(serialized); // 直接赋值（一次拷贝到目标 protobuf 字符串里）

	request.mutable_header()->set_session_id(sessionPB->gate_session_id());

	// 发送（rpcClient 内部可能会再次复制/包装，但我们避免了临时分配）
	(*rpcClient)->SendRequest(wrappedMessageId, request);
}


void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId)
{
	SendMessageToPlayerViaSessionNode(wrappedMessageId, nodeType, messageId, message, GetPlayer(playerId));
}


void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity)) {
		LOG_ERROR << "Invalid player entity";
		return;
	}

	const auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
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

void SendToCentrePlayerByClientNode(
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId)
{
	SendMessageToPlayerViaClientNode(CentrePlayerServiceMessageId, eNodeType::CentreNodeService, messageId, message, playerId);
}

void SendToCentrePlayerByClientNode(
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	SendMessageToPlayerViaClientNode(CentrePlayerServiceMessageId, eNodeType::CentreNodeService, messageId, message, playerEntity);
}
