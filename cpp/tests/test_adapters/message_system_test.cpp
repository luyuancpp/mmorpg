#include "type_define/type_define.h"

#include "google/protobuf/message.h"

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid player_id)
{
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
}

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& messag, NodeId node_id)
{
}

void BroadCastToPlayer(const uint32_t messageId, 
	const google::protobuf::Message& message,
	const EntityUnorderedSet& playerList)
{
}

void BroadCastToPlayer(const uint32_t messageId, 
	const google::protobuf::Message& message,
	const EntityVector& playerList)
{
}
