#pragma once

enum eBuffTag {
    kNone = 0,
    kMetal = 1 << 1,
    kWood = 1 << 2,
    kWater = 1 << 3,
    kFire = 1 << 4,
    kEarth = 1 << 5,
    kControl = 1 << 6,
    kStrongControl = 1 << 7,
};