#pragma once
#include "google/protobuf/message.h"
#include "entt/src/entt/entt.hpp"
#include "type_define/type_define.h"

void SendMessageToSessionNode(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType);

void SendMessageToClientNode(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType);

void CallRemoteMethodOnSession(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType);
void CallRemoteMethodOnClient(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId, uint32_t nodeType);

void BroadcastToNodes(uint32_t messageId, const google::protobuf::Message& message, uint32_t nodeType);

void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId, uint32_t nodeType, uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToPlayerViaClientNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);

void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);

void SendMessageToPlayerViaSessionNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);


void SendToCentrePlayerByClientNode(
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);
void SendToCentrePlayerByClientNode(
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);