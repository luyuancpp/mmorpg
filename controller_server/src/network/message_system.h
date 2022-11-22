#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_node.h"

void Send2Gs(const ::google::protobuf::MethodDescriptor* method, const google::protobuf::Message& message,   uint32_t gs_node_id);
void Send2GsPlayer(const google::protobuf::Message& message, entt::entity player);
void Send2GsPlayer(const google::protobuf::Message& message, EntityPtr& entity);
void Send2GsPlayer(const google::protobuf::Message& message, Guid player_id);
void Send2PlayerViaGs(const google::protobuf::Message& message, Guid player_id);//通过gs转发给客户端
void Send2PlayerViaGs(const google::protobuf::Message& message, EntityPtr& player);
void Send2PlayerViaGs(const google::protobuf::Message& message, entt::entity player);
/*注意，controller发给player消息和gs发给player的消息是异步的，不能保证ms gs 的消息哪个先到player
* 如果需要保证的话，自己在收到对应的nodeserver回包以后再同步消息给player
* 如果gs发过来的消息,需要按顺序推送给给客户端，不能直接在gs2controller的微服务里面里面调用这个接口
* 必须得replied返回给gs后再在gs返回给客户。
*/
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id);
void Send2Player(const google::protobuf::Message& message, Guid player_id);
void Send2Gate(const ::google::protobuf::MethodDescriptor* method, const google::protobuf::Message& message, uint32_t gate_id);


void CallGsPlayerMethod(const google::protobuf::Message& message, entt::entity player);

