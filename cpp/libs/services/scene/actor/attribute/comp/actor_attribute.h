#pragma once

#include <bitset>
#include <boost/dynamic_bitset.hpp>

#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"

struct AttributeDirtyFlagsComp
{
    std::bitset<kAttributeCalculatorMax> attributeBits;
};

