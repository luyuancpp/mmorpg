#include "type_define/type_define.h"

#include "google/protobuf/message.h"

void SendMessageToPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
}

void SendMessageToPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
}

void SendToGateById(uint32_t message_id, const google::protobuf::Message& messag, NodeId node_id)
{
}

void BroadCastToPlayer(const EntityUnorderedSet& player_list,
                      const uint32_t message_id, 
	const google::protobuf::Message& message)
{
}
