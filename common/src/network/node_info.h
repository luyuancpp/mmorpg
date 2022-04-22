#ifndef SRC_SERVER_COMMON_NODE_INFO_H_
#define SRC_SERVER_COMMON_NODE_INFO_H_

#include "src/common_type/common_type.h"

namespace common
{
	enum eServerType : uint32_t
	{
		kMainSceneServer,//主世界场景
		kRoomServer,//副本场景
		kMainSceneCrossServer,//跨服主世界场景
		kRoomSceneCrossServer,//跨服副本场景
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


