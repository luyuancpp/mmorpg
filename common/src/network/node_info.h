#ifndef SRC_SERVER_COMMON_NODE_INFO_H_
#define SRC_SERVER_COMMON_NODE_INFO_H_

#include "src/common_type/common_type.h"

namespace common
{
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
		kLoginNode,
		kMasterNode,
		kGsNode,
		kGateWayNode,
	};

}//namespace common

#endif//SRC_SERVER_COMMON_NODE_INFO_H_


