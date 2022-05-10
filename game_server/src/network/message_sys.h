#ifndef GAME_SERVER_MODULE_NETWORK_MESSAGE_SYS_H_
#define GAME_SERVER_MODULE_NETWORK_MESSAGE_SYS_H_

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

void Send2Player(const google::protobuf::Message& message, Guid player_id);
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, EntityPtr& player);
void Send2MsPlayer(const google::protobuf::Message& message, Guid player_id);
void Send2MsPlayer(const google::protobuf::Message& message, entt::entity player);
void Send2MsPlayer(const google::protobuf::Message& message, EntityPtr& player);
void Send2Ms(const google::protobuf::Message& messag);
void Send2Gate(const google::protobuf::Message& messag);

#endif // !GAME_SERVER_MODULE_NETWORK_MESSAGE_SYS_H_
