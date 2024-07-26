#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "type_define/type_define.h"

class PlayerTipSystem
{
public:
    static void SendTipToPlayer(entt::entity player, uint32_t tip_id, const StringVector& str_param);
    static void SendTipToPlayer(Guid player_id, uint32_t tip_id, const StringVector& str_param);
};
