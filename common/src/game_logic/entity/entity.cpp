#include "entity.h"

#include "src/game_logic/game_registry.h"

namespace common
{
    void EnityPtrDeleter(entt::entity* p)
    {
        reg.destroy(*p);
        delete p;
    }
    EntityPtr::EntityPtr()
        : entity_(new entt::entity(reg.create()), EnityPtrDeleter)
    {

    }

}