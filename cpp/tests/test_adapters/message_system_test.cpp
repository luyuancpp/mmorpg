#include "type_define/type_define.h"

#include "google/protobuf/message.h"

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid player_id)
{
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid player_id)
{
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& msg, entt::entity player)
{
}

void SendToCentre(const uint32_t messageId, const google::protobuf::Message& messag, NodeId node_id)
{
}

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& messag, NodeId node_id)
{
}

void CallCentreNodeMethod(uint32_t messageId, const google::protobuf::Message& message, const NodeId node_id)
{
}

void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message)
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
