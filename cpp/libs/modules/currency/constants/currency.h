#pragma once

#include <cstdint>

// Currency type IDs — extend this enum as new currencies are added.
enum CurrencyType : uint32_t
{
    kCurrencyGold = 0,
    kCurrencyDiamond = 1,
    kCurrencyBindDiamond = 2,
    kCurrencyMax
};
