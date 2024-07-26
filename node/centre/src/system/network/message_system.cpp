#include "message_system.h"

#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "service/game_service_service.h"
#include "service/gate_service_service.h"
#include "service/service.h"
#include "thread_local/storage.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_define/type_define.h"

void SendToGs(uint32_t message_id, const google::protobuf::Message& message, NodeId node_id)
{
	entt::entity game_node_id{ node_id };
	if (!tls.gameNodeRegistry.valid(game_node_id))
	{
        LOG_ERROR << "gs not found ->" << node_id;
		return;
	}
	const auto node =  tls.gameNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
	if (nullptr == node)
	{
		LOG_ERROR << "gs not found ->" << node_id;
		return;
	}
	(*node)->Send(message_id, message);
}

void SendToGsPlayer(const uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	entt::entity game_node_id{ player_node_info->game_node_id() };
	if (!tls.gameNodeRegistry.valid(game_node_id))
	{
		LOG_ERROR << "game node not found" << player_node_info->game_node_id();
		return;
	}
	auto game_node = tls.gameNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
	if (nullptr == game_node)
	{
        LOG_ERROR << "game node not found" << player_node_info->game_node_id();
		return;
	}
	NodeRouteMessageRequest request;
	request.mutable_body()->mutable_body()->resize(message.ByteSizeLong());
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), static_cast<int32_t>(message.ByteSizeLong()));
	request.mutable_body()->set_message_id(message_id);
	request.mutable_head()->set_session_id(player_node_info->gate_session_id());
	(*game_node)->Send(GameServiceSend2PlayerMsgId, request);
}


void SendToGsPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	SendToGsPlayer(message_id, message, tls_cl.get_player(player_id));
}

void SendToPlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	SendToPlayerViaGs(message_id, message, tls_cl.get_player(player_id));
}

void SendToPlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	entt::entity game_node_id{ player_node_info->game_node_id() };
    if (tls.gameNodeRegistry.valid(game_node_id))
    {
        LOG_ERROR << "game node not found" << player_node_info->game_node_id();
        return;
    }
    auto game_node = tls.gameNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
    if (nullptr == game_node)
    {
        LOG_ERROR << "game node not found" << player_node_info->game_node_id();
        return;
    }
	NodeRouteMessageRequest request;
	const auto  byte_size = static_cast < int32_t > ( message . ByteSizeLong ( ) );
	request.mutable_body()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byte_size);
	request.mutable_head()->set_session_id(player_node_info->gate_session_id());
	(*game_node)->Send(message_id, request);
}

void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	entt::entity gate_id{ get_gate_node_id(player_node_info->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gate_id))
	{
		LOG_ERROR << "gate not found " << player_node_info->gate_session_id();
		return;
	}
	const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_id);
	if (nullptr == gate_node)
	{
        LOG_ERROR << "gate not found " << player_node_info->gate_session_id();
        return;
	}
	SendToPlayer(message_id, message, *gate_node, player_node_info->gate_session_id());
}

void SendToPlayer(uint32_t message_id, 
	const google::protobuf::Message& message, 
	RpcSessionPtr& gate, uint64_t session_id)
{
	NodeRouteMessageRequest request;
	const auto byte_size = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byte_size);
	request.mutable_head()->set_session_id(session_id);
	request.mutable_body()->set_message_id(message_id);
	gate->Send(GateServicePlayerMessageMsgId, request);
}

void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	SendToPlayer(message_id, message, tls_cl.get_player(player_id));
}

void SendToGate(const uint32_t message_id, 
	const google::protobuf::Message& message, 
	NodeId gate_node_id)
{
	entt::entity gate_id{ gate_node_id };
    if (!tls.gateNodeRegistry.valid(gate_id))
    {
        LOG_ERROR << "gate not found " << gate_node_id;
        return;
    }
	const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_id);
	if (nullptr == gate_node)
	{
        LOG_ERROR << "gate not found " << gate_node_id;
        return;
	}
	(*gate_node)->Send(message_id, message);
}

void CallGamePlayerMethod(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	entt::entity game_node_id{ player_node_info->game_node_id() };
	if (tls.gameNodeRegistry.valid(game_node_id))
	{
		return;
	}
	const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
    if (nullptr == gate_node)
    {
        LOG_ERROR << "gate not found " << player_node_info->game_node_id();
        return;
    }
	NodeRouteMessageRequest request;
	const auto byte_size = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byte_size);
	request.mutable_body()->set_message_id(message_id);
	request.mutable_head()->set_session_id(player_node_info->gate_session_id());
	(*gate_node)->CallMethod(GameServiceCallPlayerMsgId, request);
}

void CallGameNodeMethod(uint32_t message_id, const google::protobuf::Message& message, NodeId node_id)
{
	const entt::entity game_node_id{ node_id };
    if (!tls.gameNodeRegistry.valid(game_node_id))
    {
        return;
    }
	const auto game_node = tls.gameNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
    if (nullptr == game_node)
    {
        LOG_ERROR << "gate not found " << node_id;
        return;
    }
	(*game_node)->CallMethod(message_id, message);
}

void BroadCastToGame(uint32_t message_id, const google::protobuf::Message& message)
{
}
