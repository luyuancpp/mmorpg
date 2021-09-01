#ifndef GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_
#define GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_

#include "src/common_type/common_type.h"

namespace gateway
{
    struct GateClient
    {
        uint32_t game_server_id_{ 0 };
    };

    using GateClientList = std::unordered_map<uint64_t, GateClient>;

    extern GateClientList* g_gate_clients_;
}

#endif//GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_
