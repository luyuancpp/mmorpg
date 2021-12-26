#include "entity_class.h"

#include "src/game_logic/game_registry.h"

namespace common
{
    EntityHandle::EntityHandle()
        : entity_(reg().create())
    {

    }

    EntityHandle::~EntityHandle()
    {
        reg().destroy(entity_);
    }

    EntityHandle::EntityHandle(const EntityHandle& r)
    {
        entity_ = reg().create();
    }

    EntityHandle& EntityHandle::operator=(const EntityHandle& r)
    {
        if (&r == this)
        {
            return *this;
        }
        entity_ = reg().create();
        return *this;
    }
}