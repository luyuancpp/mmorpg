#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/util/game_registry.h"

void Send2Player(const google::protobuf::Message& message, Guid player_id);
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, EntityPtr& player);
void Send2ControllerPlayer(const google::protobuf::Message& message, Guid player_id);
void Send2ControllerPlayer(const google::protobuf::Message& message, entt::entity player);
void Send2ControllerPlayer(const google::protobuf::Message& message, EntityPtr& player);
void Send2Controller(const ::google::protobuf::MethodDescriptor* method, const google::protobuf::Message& messag, uint32_t controller_node_id);
void Send2Gate(const google::protobuf::Message& messag, uint32_t gate_node_id);
