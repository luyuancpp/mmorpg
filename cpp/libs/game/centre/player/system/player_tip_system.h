#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "base/core/type_define/type_define.h"

class PlayerTipSystem
{
public:
    static void SendToPlayer(entt::entity player, uint32_t tip_id, const StringVector& str_param);
    static void SendToPlayer(Guid player_id, uint32_t tip_id, const StringVector& str_param);
};
