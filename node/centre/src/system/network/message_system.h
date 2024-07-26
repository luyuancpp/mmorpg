#pragma once

#include "google/protobuf/message.h"
#include "entt/src/entt/entt.hpp"
#include "network/rpc_session.h"
#include "type_define/type_define.h"

/*注意，centre发给player消息和gs发给player的消息是异步的，不能保证ms gs 的消息哪个先到player
* 如果需要保证的话，自己在收到对应的node_server回包以后再同步消息给player
* 如果gs发过来的消息,需要按顺序推送给给客户端，不能直接在gs2centre的微服务里面里面调用这个接口
* 必须得replied返回给gs后再在gs返回给客户。
*/
// Sends a message to a game server identified by game_node_id
void SendToGs(uint32_t message_id, const google::protobuf::Message& message, NodeId game_node_id);

// Sends a message to a player via their associated game server
void SendToGsPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);

// Overloaded version: Sends a message to a player identified by player_id via their associated game server
void SendToGsPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);

// Sends a message to a player identified by player_id via their associated game server
void SendToPlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);

// Overloaded version: Sends a message to a player via their associated game server
void SendToPlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);

// Sends a message directly to a player
void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);

// Sends a message directly to a player identified by session_id through a specified gate (RPC session)
void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, RpcSessionPtr& gate, uint64_t session_id);

// Overloaded version: Sends a message to a player identified by player_id
void SendToPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);

// Sends a message to a gate (RPC session) identified by gate_node_id
void SendToGate(uint32_t message_id, const google::protobuf::Message& message, NodeId gate_node_id);

// Calls a method on a game server associated with a specific player
void CallGamePlayerMethod(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);

// Calls a method on a specific game server node identified by node_id
void CallGameNodeMethod(uint32_t message_id, const google::protobuf::Message& message, NodeId node_id);

// Broadcasts a message to all game servers
void BroadCastToGame(uint32_t message_id, const google::protobuf::Message& message);
