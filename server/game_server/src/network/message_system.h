#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/util/game_registry.h"

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2ControllerPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2ControllerPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2Controller(uint32_t message_id,  const google::protobuf::Message& messag, uint32_t controller_node_id);
void Send2Gate(uint32_t message_id, const google::protobuf::Message& messag, uint32_t gate_node_id);

bool CallControllerNodeMethod(const uint32_t message_id, const google::protobuf::Message& message, NodeId node_id);