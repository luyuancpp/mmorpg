#include "message_system.h"

#include "muduo/base/Logging.h"

#include "network/gate_session.h"
#include "network/rpc_client.h"
#include "network/rpc_session.h"
#include "service/centre_service_service.h"
#include "service/gate_service_service.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "comp/session.h"

#include "comp/player.h"
#include "proto/logic/component/player_network_comp.pb.h"

void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	SendToPlayer(message_id, message, tls_cl.get_player(player_id));
}

void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
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
	entt::entity gate_node_id{ GetGateNodeId(player_node_info->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gate_node_id))
	{
		LOG_ERROR << "gate not found " << GetGateNodeId(player_node_info->gate_session_id());
		return;
	}
	const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
	if (nullptr == gate_node)
	{
		LOG_ERROR << "gate not found " << GetGateNodeId(player_node_info->gate_session_id());
		return;
	}
	NodeRouteMessageRequest rq;
	rq.mutable_body()->set_message_id(message_id);
	rq.mutable_body()->set_body(message.SerializeAsString());
	rq.mutable_head()->set_session_id(player_node_info->gate_session_id());
	(*gate_node)->Send(GateServicePlayerMessageMsgId, rq);
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
	if (!tls.centreNodeRegistry.valid(centre_node_id))
	{
		LOG_ERROR << "centre not found" << player_node_info->centre_node_id();
		return;
	}
	const auto centre_node = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
	if (nullptr == centre_node)
	{
		LOG_ERROR << "Send2CentrePlayer centre disconnect" << tls.registry.get<Guid>(player);
		return;
	}
	NodeRouteMessageRequest request;
	request.mutable_body()->set_message_id(message_id);
	request.mutable_body()->set_body(msg.SerializeAsString());
	request.mutable_head()->set_session_id(player_node_info->gate_session_id());
	(*centre_node)->Send(CentreServiceGsPlayerServiceMsgId, request);
}

void Send2Centre(const uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
	entt::entity centre_node_id{ node_id };
    if (!tls.centreNodeRegistry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    const auto centre_node = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->Send(message_id, messag);
}

void SendToGate(uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
	entt::entity gate_node_id{ GetGateNodeId(node_id) };
    if (!tls.gateNodeRegistry.valid(gate_node_id))
    {
		LOG_ERROR << "gate not found " << GetGateNodeId(node_id);
        return;
    }
    const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_ERROR << "gate not found " << GetGateNodeId(node_id);
        return;
    }
	(*gate_node)->Send(GateServicePlayerMessageMsgId, messag);
}

void CallCentreNodeMethod(uint32_t message_id, const google::protobuf::Message& message, const NodeId node_id)
{
	entt::entity centre_node_id{ node_id };
    if (!tls.centreNodeRegistry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    const auto centre_node = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->CallMethod(message_id, message);
}

void BroadCastToCentre(uint32_t message_id, const google::protobuf::Message& message)
{
	for (auto&& [_, node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
	{
		node->CallMethod(message_id, message);
	}
}

void BroadCastToPlayer(const EntityUnorderedSet& player_list,
                      const uint32_t message_id, 
	const google::protobuf::Message& message)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gate_info_list;
	for (auto& player : player_list)
	{
        if (!tls.registry.valid(player))
        {
            continue;
        }
        const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
        if (nullptr == player_node_info)
        {
            LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
			continue;
        }
        entt::entity gate_node_id{ GetGateNodeId(player_node_info->gate_session_id()) };
        if (!tls.gateNodeRegistry.valid(gate_node_id))
        {
            LOG_ERROR << "gate not found " << GetGateNodeId(player_node_info->gate_session_id());
			continue;
        }
		gate_info_list[gate_node_id].emplace(player_node_info->gate_session_id());
	}
  
	BroadCast2PlayerRequest rq;
	for (auto&& [gate_node_id, session_id_list] : gate_info_list)
	{
        const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
        if (nullptr == gate_node)
        {
			LOG_ERROR << "gate not found ";
            continue;
        }
        rq.mutable_body()->set_message_id(message_id);
        rq.mutable_body()->set_body(message.SerializeAsString());
		for (auto&& session_id : session_id_list)
		{
			rq.mutable_session_list()->Add(session_id);
		}
        (*gate_node)->Send(GateServiceBroadCast2PlayerMessageMsgId, rq);
	}
}
