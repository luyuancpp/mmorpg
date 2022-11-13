#include "session.h"

#include "src/util/snow_flake.h"

NodeId node_id(Guid sesion_id)
{
    return sesion_id >> ServerSequence32::kNodeBit;
}
