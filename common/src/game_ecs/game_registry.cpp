#include "game_registry.h"

namespace common
{
    entt::registry& reg()
    {
        static entt::registry singleton;
        return singleton;
    }

}//namespace common
