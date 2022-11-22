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
void Send2PlayerViaGs(const google::protobuf::Message& message, Guid player_id);//ͨ��gsת�����ͻ���
void Send2PlayerViaGs(const google::protobuf::Message& message, EntityPtr& player);
void Send2PlayerViaGs(const google::protobuf::Message& message, entt::entity player);
/*ע�⣬controller����player��Ϣ��gs����player����Ϣ���첽�ģ����ܱ�֤ms gs ����Ϣ�ĸ��ȵ�player
* �����Ҫ��֤�Ļ����Լ����յ���Ӧ��nodeserver�ذ��Ժ���ͬ����Ϣ��player
* ���gs����������Ϣ,��Ҫ��˳�����͸����ͻ��ˣ�����ֱ����gs2controller��΢�������������������ӿ�
* �����replied���ظ�gs������gs���ظ��ͻ���
*/
void Send2Player(const google::protobuf::Message& message, entt::entity player);
void Send2Player(const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id);
void Send2Player(const google::protobuf::Message& message, Guid player_id);
void Send2Gate(const ::google::protobuf::MethodDescriptor* method, const google::protobuf::Message& message, uint32_t gate_id);


void CallGsPlayerMethod(const google::protobuf::Message& message, entt::entity player);

