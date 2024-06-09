#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/type_define/type_define.h"
#include "src/util/game_registry.h"

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2CentrePlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2CentrePlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2Controller(uint32_t message_id,  const google::protobuf::Message& messag, NodeId centre_node_id);
void Send2Gate(uint32_t message_id, const google::protobuf::Message& messag, NodeId gate_node_id);

void CallCentreNodeMethod(const uint32_t message_id, const google::protobuf::Message& message, NodeId node_id);