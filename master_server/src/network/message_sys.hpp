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
/*ע�⣬ms����player��Ϣ��gs����player����Ϣ���첽�ģ����ܱ�֤ms gs ����Ϣ�ĸ��ȵ�player
* �����Ҫ��֤�Ļ����Լ����յ���Ӧ��nodeserver�ذ��Ժ���ͬ����Ϣ��player
* ���gs����������Ϣ,��Ҫ��˳�����͸����ͻ��ˣ�����ֱ����gs2ms��΢�������������������ӿ�
* �����replied���ظ�gs������gs���ظ��ͻ���
*/
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, common::Guid player_id);
void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id);


#endif//MASTER_SERVER_SRC_MESSAGE_SYS_HPP_
