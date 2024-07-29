#include "type_define/type_define.h"

#include "google/protobuf/message.h"

void SendMessageToPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
}

void SendMessageToPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
}

void SendToCentrePlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
}

void SendToCentrePlayer(uint32_t message_id, const google::protobuf::Message& msg, entt::entity player)
{
}

void Send2Centre(const uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
}

void SendToGate(uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
}

void CallCentreNodeMethod(uint32_t message_id, const google::protobuf::Message& message, const NodeId node_id)
{
}

void BroadCastToCentre(uint32_t message_id, const google::protobuf::Message& message)
{
}

void BroadCastToPlayer(const EntityUnorderedSet& player_list,
                      const uint32_t message_id, 
	const google::protobuf::Message& message)
{
}
