#pragma once
#include "src/common_type/common_type.h"

enum eServerType : uint32_t
{
	kMainSceneServer,//�����糡��
	kRoomServer,//��������
	kMainSceneCrossServer,//��������糡��
	kRoomSceneCrossServer,//�����������
};

struct GsServerType
{
	uint32_t server_type_{ kMainSceneServer };
};

enum EnumNodeType : uint32_t
{
	kLoginNode,//��¼������
	kControllerNode,//�����л����ķ�����
	kGameNode,//�߼�������
	kGateNode,//���ط�����
	kDatabaseNode,//���ݿ������
};



