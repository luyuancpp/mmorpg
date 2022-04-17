#ifndef SRC_SERVER_COMMON_NODE_INFO_H_
#define SRC_SERVER_COMMON_NODE_INFO_H_

#include "src/common_type/common_type.h"

namespace common
{
	enum eServerType : uint8_t
	{
		kMainSceneServer,//主世界场景
		kRoomServer,//副本场景
		kMainSceneCrossServer,//跨服主世界场景
		kRoomSceneCrossServer,//跨服副本场景
	};
	enum EnumNodeType : uint32_t
	{
		LOGIN_NODE_TYPE,
		MASTER_NODE_TYPE,
		GAME_SERVER_NODE_TYPE,
		GATEWAY_NODE_TYPE,
	};

}//namespace common

#endif//SRC_SERVER_COMMON_NODE_INFO_H_


