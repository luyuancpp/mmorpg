#ifndef MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
#define MASTER_SERVER_SRC_MESSAGE_SYS_HPP_

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"

namespace master
{
void Send2Gs(const google::protobuf::Message& message,   uint32_t gs_node_id);
void Send2GsPlayer(const google::protobuf::Message& message,	entt::entity player_entity);
void Send2GsPlayer(const google::protobuf::Message& message, common::Guid player_id);
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, common::Guid player_id);
void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id);

}//namespace master

#endif//MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
