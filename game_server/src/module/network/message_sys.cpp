#include "message_sys.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/module/player_list/player_list.h"
#include "src/module/network/gate_node.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/pb/pbc/msgmap.h"

#include "gs2gw.pb.h"

using namespace common;
using namespace game;

void Send2Player(const google::protobuf::Message& message, common::Guid player_id)
{
	auto it = g_players.find(player_id);
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << player_id;
		return;
	}
	auto player = it->second.entity();
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
		LOG_DEBUG << "player gate not found " << reg.get<common::Guid>(player);
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	auto& gate = (*try_gate).lock();
	if (nullptr == gate)
	{
		LOG_DEBUG << "player gate not found " << reg.get<common::Guid>(player);
		return;
	}
	gs2gw::PlayerMessageRequest gs2gw_messag;
	gs2gw_messag.mutable_msg()->set_msg_id(message_it->second);
	gs2gw_messag.mutable_msg()->set_body(message.SerializeAsString());
	gs2gw_messag.mutable_ex()->set_conn_id(reg.get<GateConnId>(player).conn_id_);
	gate->session_.Send(gs2gw_messag);
}

void Send2MsPlayer(const google::protobuf::Message& message, common::Guid player_id)
{

}

void Send2Ms(const google::protobuf::Message& messag)
{

}

void Send2Gate(const google::protobuf::Message& messag)
{

}

