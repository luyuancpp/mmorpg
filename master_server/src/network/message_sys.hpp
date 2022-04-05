#ifndef MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
#define MASTER_SERVER_SRC_MESSAGE_SYS_HPP_

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

void Send2Gs(const google::protobuf::Message& message,   uint32_t gs_node_id);
void Send2GsPlayer(const google::protobuf::Message& message,	entt::entity player);
void Send2GsPlayer(const google::protobuf::Message& message, common::EntityPtr& entity);
void Send2GsPlayer(const google::protobuf::Message& message, common::Guid player_id);
/*注意，ms发给player消息和gs发给player的消息是异步的，不能保证ms gs 的消息哪个先到player
* 如果需要保证的话，自己在收到对应的nodeserver回包以后再同步消息给player
* 如果gs发过来的消息,需要按顺序推送给给客户端，不能直接在gs2ms的微服务里面里面调用这个接口
* 必须得replied返回给gs后再在gs返回给客户。
*/
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, common::Guid player_id);
void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id);


#endif//MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
