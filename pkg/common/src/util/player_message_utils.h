#pragma once
#pragma once

#include <google/protobuf/message.h>
#include "type_define/type_define.h"
#include "network/rpc_session.h"

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity);
void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gateSession, uint64_t sessionId);

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId);

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList);
void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList);

void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);