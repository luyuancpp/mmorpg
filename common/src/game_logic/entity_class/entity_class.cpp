#include "entity_class.h"

#include "src/game_logic/game_registry.h"

namespace common
{
    EntityClass::EntityClass()
        : entity_(reg().create())
    {

    }

    EntityClass::~EntityClass()
    {
        reg().destroy(entity_);
    }
}