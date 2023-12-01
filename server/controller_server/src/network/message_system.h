#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/type_define/type_define.h"
#include "src/util/game_registry.h"
#include "src/network/gate_node.h"

void Send2Gs(uint32_t message_id, const google::protobuf::Message& message,  uint32_t game_node_id);
void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);//通过gs转发给客户端
void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
/*注意，controller发给player消息和gs发给player的消息是异步的，不能保证ms gs 的消息哪个先到player
* 如果需要保证的话，自己在收到对应的node_server回包以后再同步消息给player
* 如果gs发过来的消息,需要按顺序推送给给客户端，不能直接在gs2controller的微服务里面里面调用这个接口
* 必须得replied返回给gs后再在gs返回给客户。
*/
void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
void Send2Player(uint32_t message_id, const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id);
void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id);
void Send2Gate(uint32_t message_id, const google::protobuf::Message& message, uint32_t gate_id);


void CallGamePlayerMethod(uint32_t message_id, const google::protobuf::Message& message, entt::entity player);
bool CallGameNodeMethod(uint32_t message_id, const google::protobuf::Message& message, NodeId node_id);
