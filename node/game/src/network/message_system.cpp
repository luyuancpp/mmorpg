#include "message_system.h"

#include "muduo/base/Logging.h"

#include "thread_local/thread_local_storage_common_logic.h"
#include "thread_local/thread_local_storage.h"
#include "network/rpc_session.h"
#include "network/rpc_client.h"
#include "network/gate_session.h"
#include "service/centre_service_service.h"
#include "service/gate_service_service.h"
#include "thread_local/thread_local_storage_game.h"
#include "thread_local/thread_local_storage_common_logic.h"

#include "component_proto/player_network_comp.pb.h"
#include "comp/player_comp.h"

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2Player(message_id, message, tls_cl.get_player(player_id));
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
		return;
	}
	entt::entity gate_node_id{ get_gate_node_id(player_node_info->gate_session_id()) };
	if (!tls.gate_node_registry.valid(gate_node_id))
	{
		LOG_INFO << "gate not found " << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_node_id);
	if (nullptr == gate_node)
	{
        LOG_INFO << "gate not found " << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	NodeRouteMessageRequest message_wrapper;
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	message_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	(*gate_node)->Send(GateServicePlayerMessageMsgId, message_wrapper);
}

void Send2CentrePlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2CentrePlayer(message_id, message, tls_cl.get_player(player_id));
}

void Send2CentrePlayer(uint32_t message_id, const google::protobuf::Message& msg, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
		return;
	}
	entt::entity centre_node_id{ player_node_info->centre_node_id() };
	if (tls.centre_node_registry.valid(centre_node_id))
	{
		LOG_ERROR << "centre not found" << player_node_info->centre_node_id();
		return;
	}
	auto centre_node = tls.centre_node_registry.try_get<RpcClientPtr>(centre_node_id);
	if (nullptr == centre_node)
	{
		LOG_ERROR << "Send2CentrePlayer centre disconnect" << tls.registry.get<Guid>(player);
		return;
	}
	NodeRouteMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_message_id(message_id);
	msg_wrapper.mutable_msg()->set_body(msg.SerializeAsString());
	msg_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	(*centre_node)->Send(CentreServiceGsPlayerServiceMsgId, msg_wrapper);
}

void Send2Centre(const uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
	entt::entity centre_node_id{ node_id };
    if (!tls.centre_node_registry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    auto centre_node = tls.centre_node_registry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->Send(message_id, messag);
}

void Send2Gate(uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
	entt::entity gate_node_id{ get_gate_node_id(node_id) };
    if (!tls.gate_node_registry.valid(gate_node_id))
    {
        LOG_INFO << "gate not found " << get_gate_node_id(node_id);
        return;
    }
    auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_INFO << "gate not found " << get_gate_node_id(node_id);
        return;
    }
	(*gate_node)->Send(GateServicePlayerMessageMsgId, messag);
}

void CallCentreNodeMethod(uint32_t message_id, const google::protobuf::Message& message, const NodeId node_id)
{
	entt::entity centre_node_id{ node_id };
    if (!tls.centre_node_registry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    auto centre_node = tls.centre_node_registry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->CallMethod(message_id, message);
}

void BroadCastToCentre(uint32_t message_id, const google::protobuf::Message& message)
{
	for (auto&& [_, node] : tls.centre_node_registry.view<RpcClientPtr>().each())
	{
		node->CallMethod(message_id, message);
	}
}
