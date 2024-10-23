#pragma once

#include <bitset>

#include "game_logic/actor/constants/actor_state_attribute_calculator_constants.h"

struct ActorAttributeBitSetComp
{
    std::bitset<kAttributeCalculatorMax> attributeBits;
};
