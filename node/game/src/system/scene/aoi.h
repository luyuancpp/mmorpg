#pragma once

#include <absl/numeric/int128.h>

class Location;

class AoiSystem
{
public:
    static void Update(double delta);
    static absl::uint128 GetGridIndex(const Location& l);
};
