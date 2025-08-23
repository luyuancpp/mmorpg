#pragma once

#include <bitset>

#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"

struct ActorAttributeBitSetComp
{
    std::bitset<kAttributeCalculatorMax> attributeBits;
};
