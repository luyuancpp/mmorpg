#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/util/game_registry.h"
#include "src/network/gate_node.h"

void Send2Gs(uint32_t service_method_id, const google::protobuf::Message& message,  uint32_t gs_node_id);
void Send2GsPlayer(uint32_t service_method_id, const google::protobuf::Message& message, entt::entity player);
void Send2GsPlayer(uint32_t service_method_id, const google::protobuf::Message& message, EntityPtr& entity);
void Send2GsPlayer(uint32_t service_method_id, const google::protobuf::Message& message, Guid player_id);
void Send2PlayerViaGs(uint32_t service_method_id, const google::protobuf::Message& message, Guid player_id);//ͨ��gsת�����ͻ���
void Send2PlayerViaGs(uint32_t service_method_id, const google::protobuf::Message& message, EntityPtr& player);
void Send2PlayerViaGs(uint32_t service_method_id, const google::protobuf::Message& message, entt::entity player);
/*ע�⣬controller����player��Ϣ��gs����player����Ϣ���첽�ģ����ܱ�֤ms gs ����Ϣ�ĸ��ȵ�player
* �����Ҫ��֤�Ļ����Լ����յ���Ӧ��nodeserver�ذ��Ժ���ͬ����Ϣ��player
* ���gs����������Ϣ,��Ҫ��˳�����͸����ͻ��ˣ�����ֱ����gs2controller��΢�������������������ӿ�
* �����replied���ظ�gs������gs���ظ��ͻ���
*/
void Send2Player(uint32_t service_method_id, const google::protobuf::Message& message, entt::entity player);
void Send2Player(uint32_t service_method_id, const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id);
void Send2Player(uint32_t service_method_id, const google::protobuf::Message& message, Guid player_id);
void Send2Gate(uint32_t service_method_id, const google::protobuf::Message& message, uint32_t gate_id);


void CallGsPlayerMethod(uint32_t service_method_id, const google::protobuf::Message& message, entt::entity player);

