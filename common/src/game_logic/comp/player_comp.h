#pragma once
#include "entt/src/entt/entity/entity.hpp"
#include "src/common_type/common_type.h"

//可以丢弃时间的派发器，比如提示,服务器压力大的时候可以把提示扔了

struct PlayerNodeInfo
{
    NodeId controller_node_id_{kInvalidNodeId};
    NodeId game_node_id_{kInvalidNodeId};
    NodeId lobby_node_id_{kInvalidNodeId};
    NodeId gate_node_id_{kInvalidNodeId};
    SessionId gate_session_id_{kInvalidSessionId};
};