#pragma once

#include <absl/numeric/int128.h>

class Location;
struct Hex;

class AoiSystem
{
public:
    static void Update(double delta);
    static inline absl::uint128 GetGridId(const Location& l);
    static inline absl::uint128 GetGridId(const Hex& hex);
};
