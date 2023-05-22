#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/comp/player_list.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/network/gate_node.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/service.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/pb/pbc/gate_service_service.h"
#include "src/replied_handler/game_service_replied_handler.h"
#include "src/thread_local/controller_thread_local_storage.h"

#include "gate_service.pb.h"
#include "game_service.pb.h"


void Send2Gs(uint32_t message_id, const google::protobuf::Message& message, uint32_t node_id)
{
	auto it = controller_tls.game_node().find(node_id);
	if (it == controller_tls.game_node().end())
	{
        LOG_ERROR << "gs not found ->" << node_id;
		return;
	}
	auto node =  tls.registry.try_get<GsNodePtr>(it->second);
	if (nullptr == node)
	{
		LOG_ERROR << "gs not found ->" << node_id;
		return;
	}

	(*node)->session_.Send(message_id, message);
}

void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	auto try_player_session = tls.registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)
	{
		return;
	}
	auto gs = try_player_session->gs();
	if (nullptr == gs)
	{
		LOG_INFO << "gs not found ";
		return;
	}
	NodeServiceMessageRequest msg;
	msg.mutable_msg()->set_body(message.SerializeAsString());
	msg.mutable_ex()->set_player_id(tls.registry.get<Guid>(player));
	gs->session_.Send(message_id,  message);
}

void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, EntityPtr& player)
{
	Send2GsPlayer(message_id, message, (entt::entity)player);
}

void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
    Send2GsPlayer(message_id, message, ControllerPlayerSystem::GetPlayer(player_id));
}

void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2PlayerViaGs(message_id, message, ControllerPlayerSystem::GetPlayer(player_id));
}

void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, EntityPtr& player)
{
	Send2PlayerViaGs(message_id, message, (entt::entity)player);
}

void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
    if (!tls.registry.valid(player))
    {
        return;
    }
    auto player_session = tls.registry.get<PlayerSession>(player);
    auto gs = player_session.gs();
    if (nullptr == gs)
    {
        LOG_INFO << "gs not found ";
        return;
    }
   
    NodeServiceMessageRequest msg;
    msg.mutable_msg()->set_body(message.SerializeAsString());
    msg.mutable_ex()->set_player_id(tls.registry.get<Guid>(player));
	gs->session_.Send(message_id, message);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	auto player_session = tls.registry.get<PlayerSession>(player);
	auto gate = player_session.gate_.lock();
	if (nullptr == gate)
	{
		return;
	}
	Send2Player(message_id, message, gate, player_session.gate_session_.session_id());
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id)
{
    NodeServiceMessageRequest message_wrapper;
    message_wrapper.mutable_ex()->set_session_id(session_id);
    message_wrapper.mutable_msg()->set_body(message.SerializeAsString());
    gate->session_.Send(message_id, message_wrapper);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	auto player = ControllerPlayerSystem::GetPlayer(player_id);
	Send2Player(message_id, message, player);
}

void Send2Gate(uint32_t message_id, const google::protobuf::Message& message, uint32_t gate_id)
{
	auto gate_it = controller_tls.gate_nodes().find(gate_id);
	if (gate_it == controller_tls.gate_nodes().end())
	{
		return;
	}
    gate_it->second->session_.Send(message_id, message);
}

void CallGsPlayerMethod(uint32_t message_id, const google::protobuf::Message& msg, entt::entity player)
{
    if (!tls.registry.valid(player))
    {
        return;
    }
    auto try_player_session = tls.registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        return;
    }
    auto gs = try_player_session->gs();
    if (nullptr == gs)
    {
        LOG_INFO << "gs not found ";
        return;
    }
    auto gs_it = controller_tls.game_node().find(try_player_session->gs_node_id());
    if (gs_it == controller_tls.game_node().end())
    {
        return;
    }
    NodeServiceMessageRequest rq;
    rq.mutable_msg()->set_body(msg.SerializeAsString());
	rq.mutable_msg()->set_message_id(message_id);
    rq.mutable_ex()->set_player_id(tls.registry.get<Guid>(player));
    tls.registry.get<GsNodePtr>(gs_it->second)->session_.Send(message_id, rq);
}
