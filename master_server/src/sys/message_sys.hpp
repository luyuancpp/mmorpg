#ifndef MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
#define MASTER_SERVER_SRC_MESSAGE_SYS_HPP_

#include "muduo/base/Logging.h"

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/factories/server_global_entity.hpp"
#include "src/network/gs_node.h"
#include "src/game_logic/game_registry.h"

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

 template<typename Request, typename Response, typename StubMethod, typename Stub>
 void CallMethod(const Request& request,
        void (method)(Response*),
        StubMethod stub_method,
	    uint32_t node_id,
	    Stub s)
{
	auto& gs_nodes = common::reg.get<master::GsNodes>(master::global_entity());
	auto it = gs_nodes.find(node_id);
	if (it == gs_nodes.end())
	{
		LOG_INFO << "gs not found ->" << node_id;
		return;
	}
	auto node = common::reg.try_get<master::GsNodePtr>(it->second);
	if (nullptr == node)
	{
		LOG_INFO << "gs not found ->" << node_id;
		return;
	}
	auto& stub = *common::reg.get<Stub>(it->second);
	Response* presponse = new Response;
	((*stub.stub()).*stub_method)(nullptr,
	&request,
	presponse,
	NewCallback(method, presponse));
}

 void Send2Player(const google::protobuf::Message& message, common::Guid player_id);
 void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id);

}//namespace master

#endif//MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
