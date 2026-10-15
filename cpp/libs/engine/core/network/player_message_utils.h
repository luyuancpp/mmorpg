#pragma once
#pragma once

#include "type_define/type_define.h"
#include "network/rpc_session.h"

/* Messages from scene to a player are asynchronous -- delivery order is NOT guaranteed.
 * To guarantee order, wait for the node's reply before forwarding to the player.
 */

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, Guid playerId);
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity);
// targetPlayerId 是 gate 写 socket 前的身份栅栏(routing-identity-audit-20260908.md R13):
// gate 的 routing node_id 立刻复用、session_id 低 17 位也会回绕,只带 session_id 的推送
// 会落到"同号 session 上的另一个玩家"。传 0 只允许出现在真的拿不到玩家身份的路径上,
// 收方会放行并记 INFO —— 灰度一版之后 0 将被丢弃。
void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gateSession, SessionId sessionId, Guid targetPlayerId);

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList);
void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList);

void BroadcastMessageToScene(uint32_t messageId, const google::protobuf::Message &message, uint64_t sceneId, const EntityUnorderedSet &scenePlayerList);
void BroadcastMessageToScene(uint32_t messageId, const google::protobuf::Message &message, uint64_t sceneId, const EntityVector &scenePlayerList);

void BroadcastMessageToAll(uint32_t messageId, const google::protobuf::Message &message);

