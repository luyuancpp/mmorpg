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

void SendToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == playerNodeInfo)
	{
		LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
		return;
	}
	entt::entity gateNodeId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "gate not found " << GetGateNodeId(playerNodeInfo->gate_session_id());
		return;
	}
	const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateNodeId);
	if (nullptr == gate_node)
	{
		LOG_ERROR << "gate not found " << GetGateNodeId(playerNodeInfo->gate_session_id());
		return;
	}
	NodeRouteMessageRequest request;
	request.mutable_body()->set_message_id(messageId);
	request.mutable_body()->set_body(message.SerializeAsString());
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*gate_node)->Send(GateServicePlayerMessageMsgId, request);
}

void Send2CentrePlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	Send2CentrePlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void Send2CentrePlayer(uint32_t messageId, const google::protobuf::Message& msg, entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity))
	{
		return;
	}
	const auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
	if (nullptr == playerNodeInfo)
	{
		LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(playerEntity);
		return;
	}
	entt::entity centre_node_id{ playerNodeInfo->centre_node_id() };
	if (!tls.centreNodeRegistry.valid(centre_node_id))
	{
		LOG_ERROR << "centre not found" << playerNodeInfo->centre_node_id();
		return;
	}
	const auto centreNode = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
	if (nullptr == centreNode)
	{
		LOG_ERROR << "Send2CentrePlayer centre disconnect" << tls.registry.get<Guid>(playerEntity);
		return;
	}
	NodeRouteMessageRequest request;
	request.mutable_body()->set_message_id(messageId);
	request.mutable_body()->set_body(msg.SerializeAsString());
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*centreNode)->Send(CentreServiceGsPlayerServiceMsgId, request);
}

void Send2Centre(const uint32_t messageId, const google::protobuf::Message& messag, NodeId nodeId)
{
	entt::entity centre_node_id{ nodeId };
    if (!tls.centreNodeRegistry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << nodeId;
        return;
    }
    const auto centre_node = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << nodeId;
        return;
    }
	(*centre_node)->Send(messageId, messag);
}

void SendToGate(uint32_t messageId, const google::protobuf::Message& messag, NodeId nodeId)
{
	entt::entity gate_node_id{ GetGateNodeId(nodeId) };
    if (!tls.gateNodeRegistry.valid(gate_node_id))
    {
		LOG_ERROR << "gate not found " << GetGateNodeId(nodeId);
        return;
    }
    const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_ERROR << "gate not found " << GetGateNodeId(nodeId);
        return;
    }
	(*gate_node)->Send(GateServicePlayerMessageMsgId, messag);
}

void CallCentreNodeMethod(uint32_t messageId, const google::protobuf::Message& message, const NodeId nodeId)
{
	entt::entity centre_node_id{ nodeId };
    if (!tls.centreNodeRegistry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << nodeId;
        return;
    }
    const auto centre_node = tls.centreNodeRegistry.try_get<RpcClientPtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << nodeId;
        return;
    }
	(*centre_node)->CallMethod(messageId, message);
}

void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message)
{
	for (auto&& [_, node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
	{
		node->CallMethod(messageId, message);
	}
}

void BroadCastToPlayer(const EntityUnorderedSet& playerList,
                      const uint32_t messageId, 
	const google::protobuf::Message& message)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;
	for (auto& player : playerList)
	{
        if (!tls.registry.valid(player))
        {
            continue;
        }
        const auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
        if (nullptr == playerNodeInfo)
        {
            LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
			continue;
        }
        entt::entity gate_node_id{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
        if (!tls.gateNodeRegistry.valid(gate_node_id))
        {
            LOG_ERROR << "gate not found " << GetGateNodeId(playerNodeInfo->gate_session_id());
			continue;
        }
		gateList[gate_node_id].emplace(playerNodeInfo->gate_session_id());
	}
  
	BroadCast2PlayerRequest request;
	for (auto&& [gate_node_id, session_id_list] : gateList)
	{
        const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
        if (nullptr == gateNode)
        {
			LOG_ERROR << "gate not found ";
            continue;
        }
        request.mutable_body()->set_message_id(messageId);
        request.mutable_body()->set_body(message.SerializeAsString());
		for (auto&& session_id : session_id_list)
		{
			request.mutable_session_list()->Add(session_id);
		}
        (*gateNode)->Send(GateServiceBroadCast2PlayerMessageMsgId, request);
	}
}
