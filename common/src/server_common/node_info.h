#ifndef SRC_SERVER_COMMON_NODE_INFO_H_
#define SRC_SERVER_COMMON_NODE_INFO_H_

#include "src/common_type/common_type.h"

namespace common
{
	enum eServerType : uint8_t
	{
		kMainServer,
		kRoomServer
	};
	enum EnumNodeType : uint32_t
	{
		LOGIN_NODE_TYPE,
		MASTER_NODE_TYPE,
		GAME_SERVER_NODTE_TYPE,
		GATEWAY_NOTE_TYPE,
	};

}//namespace common

#endif//SRC_SERVER_COMMON_NODE_INFO_H_


