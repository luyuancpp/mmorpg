#ifndef MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
#define MASTER_SERVER_SRC_MESSAGE_SYS_HPP_

#include "entt/src/entt/entt.hpp"

#include "google/protobuf/message.h"

namespace master
{
void Send2Gs(const google::protobuf::Message& message, 
                     const std::string& service,
                     const std::string& method,
				     uint32_t node_id);
void Send2GsPlayer(const google::protobuf::Message& message,
	const std::string& service,
	const std::string& stub,
	entt::entity player_entity);
}//namespace master

#endif//MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
