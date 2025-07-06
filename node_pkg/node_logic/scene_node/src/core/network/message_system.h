#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "type_define/type_define.h"

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);
void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);
void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid player_id);
void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);
void SendToCentre(uint32_t messageId,  const google::protobuf::Message& messag, NodeId centre_node_id);
void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& messag, NodeId gate_node_id);

void CallCentreNodeMethod(uint32_t messageId, const google::protobuf::Message& message, NodeId node_id);

void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message);

void BroadCastToPlayer(const uint32_t messageId, 
    const google::protobuf::Message& message,
    const EntityUnorderedSet& playerList);

void BroadCastToPlayer(const uint32_t messageId, 
    const google::protobuf::Message& message,
    const EntityVector& playerList);