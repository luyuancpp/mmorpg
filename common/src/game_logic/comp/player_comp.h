#pragma once
#include <memory>
#include <string>

#include "src/common_type/common_type.h"

#include "entt/src/entt/entity/entity.hpp"

struct GateSession
{
     uint64_t session_id_{UINT64_MAX};
};

struct PlayerId 
{
    Guid player_id_{0};
};


