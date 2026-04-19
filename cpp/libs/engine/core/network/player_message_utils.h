#pragma once
#pragma once

#include "type_define/type_define.h"
#include "network/rpc_session.h"

/* Messages from scene to a player are asynchronous -- delivery order is NOT guaranteed.
 * To guarantee order, wait for the node's reply before forwarding to the player.
 */

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity);
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gateSession, SessionId sessionId);

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId);

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList);
void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList);

void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);


void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);
void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);


void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);
void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);


void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity player	);

void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);