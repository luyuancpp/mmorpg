#include "entity_class.h"

#include "src/game_logic/game_registry.h"

namespace common
{
    EntityHandle::EntityHandle()
        : entity_(reg().create()){}

    EntityHandle::~EntityHandle()
    {
        reg().destroy(entity_);
    }
}