#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/centre_node.h"
#include "src/network/session.h"
#include "service/centre_service_service.h"
#include "service/gate_service_service.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_network_comp.pb.h"
#include "src/comp/player_comp.h"

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2Player(message_id, message, entity{player_id});
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
	entity gate_node_id{ get_gate_node_id(player_node_info->gate_session_id()) };
	if (tls.gate_node_registry.valid(gate_node_id))
	{
		LOG_INFO << "gate not found " << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	auto gate_node = tls.gate_node_registry.try_get<GateNodePtr>(gate_node_id);
	if (nullptr == gate_node)
	{
        LOG_INFO << "gate not found " << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	NodeRouteMessageRequest message_wrapper;
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	message_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	(*gate_node)->session_.Send(GateServicePlayerMessageMsgId, message_wrapper);
}

void Send2CentrePlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2CentrePlayer(message_id, message, entity{player_id});
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
	entity centre_node_id{ player_node_info->centre_node_id() };
	if (tls.centre_node_registry.valid(centre_node_id))
	{
		LOG_ERROR << "centre not found" << player_node_info->centre_node_id();
		return;
	}
	auto centre_node = tls.centre_node_registry.try_get<CentreNodePtr>(centre_node_id);
	if (nullptr == centre_node)
	{
		LOG_ERROR << "Send2CentrePlayer centre disconnect" << tls.registry.get<Guid>(player);
		return;
	}
	NodeRouteMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_message_id(message_id);
	msg_wrapper.mutable_msg()->set_body(msg.SerializeAsString());
	msg_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	(*centre_node)->session_->Send(CentreServiceGsPlayerServiceMsgId, msg_wrapper);
}

void Send2Centre(const uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
    entity centre_node_id{ node_id };
    if (tls.centre_node_registry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    auto centre_node = tls.centre_node_registry.try_get<CentreNodePtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->session_->Send(message_id, messag);
}

void Send2Gate(uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
    entity gate_node_id{ get_gate_node_id(node_id) };
    if (tls.gate_node_registry.valid(gate_node_id))
    {
        LOG_INFO << "gate not found " << get_gate_node_id(node_id);
        return;
    }
    auto gate_node = tls.gate_node_registry.try_get<GateNodePtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_INFO << "gate not found " << get_gate_node_id(node_id);
        return;
    }
	(*gate_node)->session_.Send(GateServicePlayerMessageMsgId, messag);
}

void CallCentreNodeMethod(const uint32_t message_id, const google::protobuf::Message& message, const NodeId node_id)
{
    entity centre_node_id{ node_id };
    if (tls.centre_node_registry.valid(centre_node_id))
    {
        LOG_ERROR << "centre not found" << node_id;
        return;
    }
    auto centre_node = tls.centre_node_registry.try_get<CentreNodePtr>(centre_node_id);
    if (nullptr == centre_node)
    {
        LOG_ERROR << "Send2CentrePlayer centre disconnect" << node_id;
        return;
    }
	(*centre_node)->session_->CallMethod(message_id, message);
}