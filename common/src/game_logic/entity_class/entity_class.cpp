#include "entity_class.h"

#include "src/game_logic/game_registry.h"

namespace common
{
    EntityHandle::EntityHandle()
        : entity_(new entt::entity(reg.create()), [](entt::entity* p){reg.destroy(*p);delete p;})
    {

    }

}