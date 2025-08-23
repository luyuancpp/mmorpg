#pragma once
#pragma once

#include <google/protobuf/message.h>
#include "type_define/type_define.h"
#include "network/rpc_session.h"

/*注意，centre发给player消息和gs发给player的消息是异步的，不能保证ms gs 的消息哪个先到player
* 如果需要保证的话，自己在收到对应的node_server回包以后再同步消息给player
* 如果gs发过来的消息,需要按顺序推送给给客户端，不能直接在gs2centre的微服务里面里面调用这个接口
* 必须得replied返回给gs后再在gs返回给客户。
*/

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity);
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gateSession, uint64_t sessionId);

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId);

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList);
void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList);

void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);


void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);
void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);


void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity);
void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);


void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity player	);

void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId);