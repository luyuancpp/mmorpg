#ifndef SRC_SERVER_COMMON_NODE_INFO_H_
#define SRC_SERVER_COMMON_NODE_INFO_H_

#include "src/common_type/common_type.h"

namespace common
{
	enum EnumNodeType : uint32_t
	{
		LOGIN_NODE_TYPE,
		MASTER_NODE_TYPE,
		GAME_SERVER_NODTE_TYPE,
		GATEWAY_NOTE_TYPE,
	};

class NodeInfo
{
public:
    uint32_t node_id_{ 0 };
    uint8_t node_type_{ 0 };
};

}//namespace common

#endif//SRC_SERVER_COMMON_NODE_INFO_H_


