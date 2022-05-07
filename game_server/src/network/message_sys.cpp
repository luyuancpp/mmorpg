#include "message_sys.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/sys/player_list/player_list.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"
#include "src/game_logic/comp/player_comp.h"
#include "src/pb/pbc/msgmap.h"

#include "gw_service.pb.h"

void Send2Player(const google::protobuf::Message& message, Guid player_id)
{
	auto it = g_players.find(player_id);
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << player_id;
		return;
	}
	Send2Player(message, player_id);
}

void Send2Player(const google::protobuf::Message& message, entt::entity player)
{
	if (!reg.valid(player))
	{
		return;
	}
	auto try_gate = reg.try_get<GateNodeWPtr>(player);
	if (nullptr == try_gate)
	{
		LOG_DEBUG << "player gate not found " << reg.get<Guid>(player);
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	auto gate = (*try_gate).lock();
	if (nullptr == gate)
	{
		LOG_DEBUG << "player gate not found " << reg.get<Guid>(player);
		return;
	}
	gwservice::GsPlayerMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
	msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	msg_wrapper.mutable_ex()->set_conn_id(reg.get<GateConnId>(player).conn_id_);
	gate->session_.Send(msg_wrapper);
}

void Send2MsPlayer(const google::protobuf::Message& message, Guid player_id)
{
	auto it = g_players.find(player_id);
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << player_id;
		return;
	}
	auto player = it->second.entity();
	Send2MsPlayer(message, player);
}

void Send2MsPlayer(const google::protobuf::Message& message, entt::entity player)
{
	if (!reg.valid(player))
	{
		return;
	}
	auto try_ms = reg.try_get<MsNodeWPtr>(player);
	if (nullptr == try_ms)
	{
		LOG_DEBUG << "player gate not found " << reg.get<Guid>(player);
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	auto ms = (*try_ms).lock();
	if (nullptr == ms)
	{
		LOG_DEBUG << "player gate not found " << reg.get<Guid>(player);
		return;
	}
	msservice::PlayerNodeServiceRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
	msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	msg_wrapper.mutable_ex()->set_player_id(reg.get<Guid>(player));
	ms->session_->Send(msg_wrapper);
}

void Send2Player(const google::protobuf::Message& message, EntityPtr& entity)
{
	Send2Player(message, entity.entity());
}

void Send2Ms(const google::protobuf::Message& messag)
{

}

void Send2Gate(const google::protobuf::Message& messag)
{

}

