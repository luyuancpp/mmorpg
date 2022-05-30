#include "session.h"

#include "src/util/snow_flake.h"

uint32_t node_id(Guid guid)
{
    return guid >> ServerSequence::kNodeBit;
}
